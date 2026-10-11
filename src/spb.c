#include "spb.h"

#include <MQTTAsync.h>

 /* TODO: Make generic? */
#include <stdio.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * Compile-time gated logging
 *
 * Set SPB_LOG_LEVEL to one of the level constants below (via -D or CMake's
 * SPB_LOG_LEVEL cache variable) to enable logging at and above that level.
 * The default (undefined / WARN) emits WARN and ERROR calls. Set to OFF to
 * strip every log call and its format string from the binary at compile
 * time — no branches, no .rodata entries.
 *
 * Usage:
 *   SPB_LOG_ERROR("connect failed: code %d", rc);
 *   SPB_LOG_WARN("unexpected NULL context — dispatch skipped");
 *   SPB_LOG_INFO("client created");
 *   SPB_LOG_TRACE("message arrived callback");
 * ------------------------------------------------------------------------- */

#define SPB_LOG_LEVEL_OFF   0
#define SPB_LOG_LEVEL_ERROR 1
#define SPB_LOG_LEVEL_WARN  2
#define SPB_LOG_LEVEL_INFO  3
#define SPB_LOG_LEVEL_TRACE 4

#ifndef SPB_LOG_LEVEL
#  define SPB_LOG_LEVEL SPB_LOG_LEVEL_WARN
#endif

#if SPB_LOG_LEVEL > SPB_LOG_LEVEL_OFF
#include <stdarg.h>
static void spb_log_(const char* level, const char* fmt, ...);
static void spb_log_(const char* level, const char* fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[spb][%s] ", level);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
#endif

#if SPB_LOG_LEVEL >= SPB_LOG_LEVEL_ERROR
#  define SPB_LOG_ERROR(...) spb_log_("ERROR", __VA_ARGS__)
#else
#  define SPB_LOG_ERROR(...) ((void)0)
#endif

#if SPB_LOG_LEVEL >= SPB_LOG_LEVEL_WARN
#  define SPB_LOG_WARN(...)  spb_log_("WARN",  __VA_ARGS__)
#else
#  define SPB_LOG_WARN(...)  ((void)0)
#endif

#if SPB_LOG_LEVEL >= SPB_LOG_LEVEL_INFO
#  define SPB_LOG_INFO(...)  spb_log_("INFO",  __VA_ARGS__)
#else
#  define SPB_LOG_INFO(...)  ((void)0)
#endif

#if SPB_LOG_LEVEL >= SPB_LOG_LEVEL_TRACE
#  define SPB_LOG_TRACE(...) spb_log_("TRACE", __VA_ARGS__)
#else
#  define SPB_LOG_TRACE(...) ((void)0)
#endif

/* Keep the public spb_error_t mirror in lockstep with Paho's codes. If Paho
 * ever renumbers, these fail the build instead of silently mistranslating.
 * A negative-array-size typedef is used rather than C11 _Static_assert because
 * the library is built as strict C99 (see CMakeLists.txt). */
#define SPB_CONCAT_(a, b) a##b
#define SPB_CONCAT(a, b) SPB_CONCAT_(a, b)
#define SPB_STATIC_ASSERT(cond) \
    typedef char SPB_CONCAT(spb_static_assert_, __LINE__)[(cond) ? 1 : -1]

SPB_STATIC_ASSERT(SPB_SUCCESS               == MQTTASYNC_SUCCESS);
SPB_STATIC_ASSERT(SPB_FAILURE               == MQTTASYNC_FAILURE);
SPB_STATIC_ASSERT(SPB_PERSISTENCE_ERROR     == MQTTASYNC_PERSISTENCE_ERROR);
SPB_STATIC_ASSERT(SPB_DISCONNECTED          == MQTTASYNC_DISCONNECTED);
SPB_STATIC_ASSERT(SPB_MAX_MESSAGES_INFLIGHT == MQTTASYNC_MAX_MESSAGES_INFLIGHT);
SPB_STATIC_ASSERT(SPB_BAD_UTF8_STRING       == MQTTASYNC_BAD_UTF8_STRING);
SPB_STATIC_ASSERT(SPB_NULL_PARAMETER        == MQTTASYNC_NULL_PARAMETER);
SPB_STATIC_ASSERT(SPB_TOPICNAME_TRUNCATED   == MQTTASYNC_TOPICNAME_TRUNCATED);
SPB_STATIC_ASSERT(SPB_BAD_STRUCTURE         == MQTTASYNC_BAD_STRUCTURE);
SPB_STATIC_ASSERT(SPB_BAD_QOS               == MQTTASYNC_BAD_QOS);
SPB_STATIC_ASSERT(SPB_NO_MORE_MSGIDS        == MQTTASYNC_NO_MORE_MSGIDS);
SPB_STATIC_ASSERT(SPB_OPERATION_INCOMPLETE  == MQTTASYNC_OPERATION_INCOMPLETE);
SPB_STATIC_ASSERT(SPB_MAX_BUFFERED_MESSAGES == MQTTASYNC_MAX_BUFFERED_MESSAGES);
SPB_STATIC_ASSERT(SPB_SSL_NOT_SUPPORTED     == MQTTASYNC_SSL_NOT_SUPPORTED);
SPB_STATIC_ASSERT(SPB_BAD_PROTOCOL          == MQTTASYNC_BAD_PROTOCOL);
SPB_STATIC_ASSERT(SPB_BAD_MQTT_OPTION       == MQTTASYNC_BAD_MQTT_OPTION);
SPB_STATIC_ASSERT(SPB_WRONG_MQTT_VERSION    == MQTTASYNC_WRONG_MQTT_VERSION);
SPB_STATIC_ASSERT(SPB_0_LEN_WILL_TOPIC      == MQTTASYNC_0_LEN_WILL_TOPIC);
SPB_STATIC_ASSERT(SPB_COMMAND_IGNORED       == MQTTASYNC_COMMAND_IGNORED);
SPB_STATIC_ASSERT(SPB_MAX_BUFFERED          == MQTTASYNC_MAX_BUFFERED);

/* spb_token_t must be layout- and signedness-compatible with Paho's
 * MQTTAsync_token so the `*out_token = paho_resp_opts.token;` assignments
 * and the MQTTAsync_token -> spb_token_t casts in the wrapper callbacks are
 * value-preserving no-ops.  If Paho ever redefines its token type these fail
 * the build instead of silently truncating or changing sign. */
SPB_STATIC_ASSERT(sizeof(spb_token_t) == sizeof(MQTTAsync_token));
SPB_STATIC_ASSERT(((spb_token_t)-1 < 0) == ((MQTTAsync_token)-1 < 0));

typedef struct paho_client_cb {
    MQTTAsync_connectionLost* connection_lost_cb;
    MQTTAsync_messageArrived* messagearrived_cb;
    MQTTAsync_deliveryComplete* delivery_complete_cb;
} paho_client_cb_t;

typedef struct paho_lib {
    MQTTAsync client;
    MQTTAsync_connectOptions connect_options;
    MQTTAsync_disconnectOptions disconnect_options;
    paho_client_cb_t client_cb;
} paho_lib_t;

struct spb_client {
    paho_lib_t paho_lib;
    spb_client_cb_t client_callbacks;
    spb_connect_options_t connect_options;
    spb_disconnect_options_t disconnect_options;
};

/* Per-call response state. Publish (spb_sendmessage) and subscribe
 * (spb_subscribe) each allocate one of these and hand it to Paho as the
 * response context, so concurrent/interleaved operations no longer share a
 * single slot and cannot clobber each other's callbacks. The matching wrapper
 * callback frees it after dispatching to the user callback. */
typedef struct spb_response_ctx {
    spb_response_options_t options;
} spb_response_ctx_t;

spb_client_t* spb_init(void)
{
    spb_client_t* client = calloc(1, sizeof(spb_client_t));
    if (client != NULL) {
        MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
        client->paho_lib.connect_options = conn_opts;

        MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
        client->paho_lib.disconnect_options = disc_opts;
    }

    return client;
}

spb_error_t spb_create(spb_client_t* client, const char* uri, const char* client_id)
{
    if (client == NULL ||
        uri == NULL ||
        client_id == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    int rc = MQTTAsync_create(&client->paho_lib.client, uri, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        return (spb_error_t)rc;
    }

    return SPB_SUCCESS;
}

static void connectionlost_wrapper_cb(void* context, char* cause);
static int messagearrived_wrapper_cb(void* context, char* topic_name, int topic_len, MQTTAsync_message* message);
static void deliverycomplete_wrapper_cb(void* context, MQTTAsync_token token);
static void connect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response);
static void connect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response);
static void disconnect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response);
static void disconnect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response);
static void response_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response);
static void response_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response);

spb_error_t spb_setcallbacks(
    spb_client_t* client,
    void* context,
    spb_connectionlost* connection_lost_cb,
    spb_messagearrived* messagearrived_cb,
    spb_deliverycomplete* delivery_complete_cb)
{
    if (client == NULL ||
        client->paho_lib.client == NULL ||
        messagearrived_cb == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    client->client_callbacks.connection_lost_cb = connection_lost_cb;
    client->client_callbacks.messagearrived_cb = messagearrived_cb;
    client->client_callbacks.delivery_complete_cb = delivery_complete_cb;
    client->client_callbacks.context = context;

	int rc = MQTTAsync_setCallbacks(
	    client->paho_lib.client,
	    client,
	    connectionlost_wrapper_cb,
	    messagearrived_wrapper_cb,
	    deliverycomplete_wrapper_cb);
	if (rc != MQTTASYNC_SUCCESS) {
	    return (spb_error_t)rc;
	}

    return SPB_SUCCESS;
}

spb_error_t spb_connect(spb_client_t* client, spb_connect_options_t connect_options)
{
    if (client == NULL ||
        client->paho_lib.client == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    client->connect_options = connect_options;

    client->paho_lib.connect_options.keepAliveInterval = client->connect_options.keep_alive_interval;
    client->paho_lib.connect_options.cleansession = client->connect_options.clean_session;
    client->paho_lib.connect_options.onSuccess = connect_onsuccess_wrapper_cb;
    client->paho_lib.connect_options.onFailure = connect_onfailure_wrapper_cb;
    client->paho_lib.connect_options.context = client;

    int rc = MQTTAsync_connect(client->paho_lib.client, &client->paho_lib.connect_options);
    if (rc != MQTTASYNC_SUCCESS) {
        return (spb_error_t)rc;
    }

    return SPB_SUCCESS;
}

spb_error_t spb_sendmessage(spb_client_t* client, const char* topic, const spb_message_t* message, spb_response_options_t response_options, spb_token_t* out_token)
{
    if (client == NULL ||
        client->paho_lib.client == NULL ||
        topic == NULL ||
        message == NULL ||
        message->payload == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    spb_response_ctx_t* ctx = calloc(1, sizeof(spb_response_ctx_t));
    if (ctx == NULL) {
        return SPB_ERR_NOMEM;
    }
    ctx->options = response_options;

    MQTTAsync_responseOptions paho_resp_opts = MQTTAsync_responseOptions_initializer;
    paho_resp_opts.onSuccess = response_onsuccess_wrapper_cb;
    paho_resp_opts.onFailure = response_onfailure_wrapper_cb;
    paho_resp_opts.context = ctx;

    MQTTAsync_message paho_msg = MQTTAsync_message_initializer;
    paho_msg.payload = message->payload;
    paho_msg.payloadlen = message->payloadlen;
    paho_msg.qos = message->qos;
    paho_msg.retained = message->retained;

	int rc = MQTTAsync_sendMessage(client->paho_lib.client, topic, &paho_msg, &paho_resp_opts);
	if (rc != MQTTASYNC_SUCCESS) {
	    free(ctx);
	    return (spb_error_t)rc;
	}

    if (out_token != NULL) {
        *out_token = (spb_token_t)paho_resp_opts.token;
    }

    return SPB_SUCCESS;
}

spb_error_t spb_subscribe(spb_client_t* client, const char* topic, int qos, spb_response_options_t response_options, spb_token_t* out_token)
{
    if (client == NULL ||
        client->paho_lib.client == NULL ||
        topic == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    spb_response_ctx_t* ctx = calloc(1, sizeof(spb_response_ctx_t));
    if (ctx == NULL) {
        return SPB_ERR_NOMEM;
    }
    ctx->options = response_options;

    MQTTAsync_responseOptions paho_resp_opts = MQTTAsync_responseOptions_initializer;
    paho_resp_opts.onSuccess = response_onsuccess_wrapper_cb;
    paho_resp_opts.onFailure = response_onfailure_wrapper_cb;
    paho_resp_opts.context = ctx;

    int rc = MQTTAsync_subscribe(client->paho_lib.client, topic, qos, &paho_resp_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        free(ctx);
        return (spb_error_t)rc;
    }

    if (out_token != NULL) {
        *out_token = (spb_token_t)paho_resp_opts.token;
    }

    return SPB_SUCCESS;
}

spb_error_t spb_disconnect(spb_client_t* client, spb_disconnect_options_t disconnect_options)
{
    if (client == NULL ||
        client->paho_lib.client == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    client->disconnect_options = disconnect_options;
    client->paho_lib.disconnect_options.onSuccess = disconnect_onsuccess_wrapper_cb;
    client->paho_lib.disconnect_options.onFailure = disconnect_onfailure_wrapper_cb;
    client->paho_lib.disconnect_options.context = client;

    int rc = MQTTAsync_disconnect(client->paho_lib.client, &client->paho_lib.disconnect_options);
    if (rc != MQTTASYNC_SUCCESS) {
        return (spb_error_t)rc;
    }

    return SPB_SUCCESS;
}

void spb_destroy(spb_client_t* client)
{
    if (client != NULL) {
        MQTTAsync_destroy(&client->paho_lib.client);
        free(client);
    }
}

const char* spb_strerror(spb_error_t code)
{
    /* NOTE: this switch deliberately has no `default:` label and lists every
     * spb_error_t enumerator. Under -Wall (-Wswitch) + -Werror that makes it a
     * build error to add a new code without handling it here, which prevents an
     * spb-layer code from silently falling through to MQTTAsync_strerror and
     * leaking a Paho error string for a code Paho never issued. spb-layer codes
     * must return their own string; only the Paho-mirrored range defers to
     * Paho. */
    switch (code) {
    /* spb-layer codes — handled here so they never reach Paho */
    case SPB_SUCCESS:               return "success";
    case SPB_ERR_INVALID_ARGUMENT:  return "invalid argument";
    case SPB_ERR_NOMEM:             return "out of memory";

    /* Paho-mirrored codes (-1..-19) — defer to Paho's own strings */
    case SPB_FAILURE:
    case SPB_PERSISTENCE_ERROR:
    case SPB_DISCONNECTED:
    case SPB_MAX_MESSAGES_INFLIGHT:
    case SPB_BAD_UTF8_STRING:
    case SPB_NULL_PARAMETER:
    case SPB_TOPICNAME_TRUNCATED:
    case SPB_BAD_STRUCTURE:
    case SPB_BAD_QOS:
    case SPB_NO_MORE_MSGIDS:
    case SPB_OPERATION_INCOMPLETE:
    case SPB_MAX_BUFFERED_MESSAGES:
    case SPB_SSL_NOT_SUPPORTED:
    case SPB_BAD_PROTOCOL:
    case SPB_BAD_MQTT_OPTION:
    case SPB_WRONG_MQTT_VERSION:
    case SPB_0_LEN_WILL_TOPIC:
    case SPB_COMMAND_IGNORED:
    case SPB_MAX_BUFFERED:
        return MQTTAsync_strerror((int)code);
    }

    /* Reached only for an integer cast to spb_error_t that matches no known
     * code. Never forwarded to Paho. */
    return "unknown error code";
}

static void connectionlost_wrapper_cb(void* context, char* cause)
{
    SPB_LOG_TRACE("connection lost callback");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        if (client->client_callbacks.connection_lost_cb != NULL) {
            client->client_callbacks.connection_lost_cb(client->client_callbacks.context, cause);
        }
    } else {
        SPB_LOG_WARN("connection lost callback invoked with NULL context — user callback not dispatched");
    }
}

static int messagearrived_wrapper_cb(void* context, char* topic_name, int topic_len, MQTTAsync_message* message)
{
    SPB_LOG_TRACE("message arrived callback");
    int rc = 0;

    if (context == NULL) {
        SPB_LOG_WARN("message arrived callback invoked with NULL context — user callback not dispatched");
    } else if (topic_name == NULL) {
        SPB_LOG_WARN("message arrived callback invoked with NULL topic_name — user callback not dispatched");
    } else if (message == NULL) {
        SPB_LOG_WARN("message arrived callback invoked with NULL message — user callback not dispatched");
    } else {
        spb_client_t* client = (spb_client_t*)context;
        spb_message_t msg = {
            .payload = message->payload,
            .payloadlen = message->payloadlen,
            .retained = message->retained,
            .qos = message->qos,
        };

        if (client->client_callbacks.messagearrived_cb != NULL) {
            rc = client->client_callbacks.messagearrived_cb(client->client_callbacks.context, topic_name, topic_len, &msg);
        }
    }

    if (message != NULL) {
        MQTTAsync_freeMessage(&message);
    }

    if (topic_name != NULL) {
        MQTTAsync_free(topic_name);
    }

    return rc;
}

static void deliverycomplete_wrapper_cb(void* context, MQTTAsync_token token)
{
    SPB_LOG_TRACE("delivery complete callback");
    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        if (client->client_callbacks.delivery_complete_cb != NULL) {
            client->client_callbacks.delivery_complete_cb(client->client_callbacks.context, (spb_token_t)token);
        }
    } else {
        SPB_LOG_WARN("delivery complete callback invoked with NULL context — user callback not dispatched");
    }
}

static void connect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    SPB_LOG_TRACE("connect on success callback");
    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_successdata_t success_data = { .token = (spb_token_t)(response ? response->token : 0) };

        if (client->connect_options.onsuccess_cb != NULL) {
            client->connect_options.onsuccess_cb(client->connect_options.context, &success_data);
        }
    } else {
        SPB_LOG_WARN("connect on success callback invoked with NULL context — user callback not dispatched");
    }
}

static void connect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    SPB_LOG_TRACE("connect on failure callback");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_failuredata_t failure_data = {
            .token = (spb_token_t)(response ? response->token : 0),
            .code = response ? response->code : 0,
            .message = response ? response->message : NULL,
        };

        if (client->connect_options.onfailure_cb != NULL) {
            client->connect_options.onfailure_cb(client->connect_options.context, &failure_data);
        }
    } else {
        SPB_LOG_WARN("connect on failure callback invoked with NULL context — user callback not dispatched");
    }
}

static void disconnect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    SPB_LOG_TRACE("disconnect on success callback");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_successdata_t success_data = { .token = (spb_token_t)(response ? response->token : 0) };

        if (client->disconnect_options.onsuccess_cb != NULL) {
            client->disconnect_options.onsuccess_cb(client->disconnect_options.context, &success_data);
        }
    } else {
        SPB_LOG_WARN("disconnect on success callback invoked with NULL context — user callback not dispatched");
    }
}

static void disconnect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    SPB_LOG_TRACE("disconnect on failure callback");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_failuredata_t failure_data = {
            .token = (spb_token_t)(response ? response->token : 0),
            .code = response ? response->code : 0,
            .message = response ? response->message : NULL,
        };

        if (client->disconnect_options.onfailure_cb != NULL) {
            client->disconnect_options.onfailure_cb(client->disconnect_options.context, &failure_data);
        }
    } else {
        SPB_LOG_WARN("disconnect on failure callback invoked with NULL context — user callback not dispatched");
    }
}

static void response_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    SPB_LOG_TRACE("response on success callback");

    if (context != NULL) {
        spb_response_ctx_t* ctx = (spb_response_ctx_t*)context;
        spb_successdata_t success_data = { .token = (spb_token_t)(response ? response->token : 0) };

        if (ctx->options.onsuccess_cb != NULL) {
            ctx->options.onsuccess_cb(ctx->options.context, &success_data);
        }

        free(ctx);
    } else {
        SPB_LOG_ERROR("response on success callback invoked with NULL context — spb_response_ctx_t invariant violated, user callback not dispatched");
    }
}

static void response_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    SPB_LOG_TRACE("response on failure callback");

    if (context != NULL) {
        spb_response_ctx_t* ctx = (spb_response_ctx_t*)context;
        spb_failuredata_t failure_data = {
            .token = (spb_token_t)(response ? response->token : 0),
            .code = response ? response->code : 0,
            .message = response ? response->message : NULL,
        };

        if (ctx->options.onfailure_cb != NULL) {
            ctx->options.onfailure_cb(ctx->options.context, &failure_data);
        }

        free(ctx);
    } else {
        SPB_LOG_ERROR("response on failure callback invoked with NULL context — spb_response_ctx_t invariant violated, user callback not dispatched");
    }
}