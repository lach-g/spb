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

/* spb_mqtt_version_t and spb_ssl_version_t are passed to Paho by value. */
SPB_STATIC_ASSERT(SPB_MQTT_VERSION_DEFAULT == MQTTVERSION_DEFAULT);
SPB_STATIC_ASSERT(SPB_MQTT_VERSION_3_1     == MQTTVERSION_3_1);
SPB_STATIC_ASSERT(SPB_MQTT_VERSION_3_1_1   == MQTTVERSION_3_1_1);
SPB_STATIC_ASSERT(SPB_MQTT_VERSION_5       == MQTTVERSION_5);
SPB_STATIC_ASSERT(SPB_SSL_VERSION_DEFAULT  == MQTT_SSL_VERSION_DEFAULT);
SPB_STATIC_ASSERT(SPB_SSL_VERSION_TLS_1_0  == MQTT_SSL_VERSION_TLS_1_0);
SPB_STATIC_ASSERT(SPB_SSL_VERSION_TLS_1_1  == MQTT_SSL_VERSION_TLS_1_1);
SPB_STATIC_ASSERT(SPB_SSL_VERSION_TLS_1_2  == MQTT_SSL_VERSION_TLS_1_2);

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
    spb_mqtt_version_t mqtt_version; /* fixed at create; selects v3 vs v5 Paho callbacks */
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
    return spb_create_with_options(client, uri, client_id, NULL);
}

spb_error_t spb_create_with_options(spb_client_t* client, const char* uri, const char* client_id, const spb_create_options_t* options)
{
    static const spb_create_options_t default_options = { 0 };

    if (client == NULL ||
        uri == NULL ||
        client_id == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    if (options == NULL) {
        options = &default_options;
    }

    int persistence_type;
    void* persistence_context = NULL;
    switch (options->persistence) {
    case SPB_PERSISTENCE_NONE:
        persistence_type = MQTTCLIENT_PERSISTENCE_NONE;
        break;
    case SPB_PERSISTENCE_FILE:
        persistence_type = MQTTCLIENT_PERSISTENCE_DEFAULT;
        /* Paho copies the directory string; the cast only drops const. */
        persistence_context = (void*)options->persistence_dir;
        break;
    default:
        return SPB_ERR_INVALID_ARGUMENT;
    }

    switch (options->mqtt_version) {
    case SPB_MQTT_VERSION_DEFAULT:
    case SPB_MQTT_VERSION_3_1:
    case SPB_MQTT_VERSION_3_1_1:
    case SPB_MQTT_VERSION_5:
        break;
    default:
        return SPB_ERR_INVALID_ARGUMENT;
    }

    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    create_opts.MQTTVersion = options->mqtt_version;

    int rc = MQTTAsync_createWithOptions(&client->paho_lib.client, uri, client_id, persistence_type, persistence_context, &create_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        return (spb_error_t)rc;
    }

    client->mqtt_version = options->mqtt_version;

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
static void response_onsuccess5_wrapper_cb(void* context, MQTTAsync_successData5* response);
static void response_onfailure5_wrapper_cb(void* context,  MQTTAsync_failureData5* response);

/* Paho rejects v3 onSuccess/onFailure on publish and subscribe for an MQTT 5
 * client (MQTTASYNC_BAD_MQTT_OPTION), so pick the callback pair by version. */
static void set_response_callbacks(const spb_client_t* client, MQTTAsync_responseOptions* opts)
{
    if (client->mqtt_version == SPB_MQTT_VERSION_5) {
        opts->onSuccess5 = response_onsuccess5_wrapper_cb;
        opts->onFailure5 = response_onfailure5_wrapper_cb;
    } else {
        opts->onSuccess = response_onsuccess_wrapper_cb;
        opts->onFailure = response_onfailure_wrapper_cb;
    }
}

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
    static const spb_ssl_options_t default_ssl = { 0 };

    if (client == NULL ||
        client->paho_lib.client == NULL) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    const spb_will_options_t* will = connect_options.will;
    if (will != NULL &&
        (will->topic == NULL ||
         will->payloadlen < 0 ||
         (will->payload == NULL && will->payloadlen != 0))) {
        return SPB_ERR_INVALID_ARGUMENT;
    }

    client->connect_options = connect_options;

    MQTTAsync_connectOptions* paho_opts = &client->paho_lib.connect_options;
    paho_opts->keepAliveInterval = connect_options.keep_alive_interval;
    paho_opts->MQTTVersion = client->mqtt_version;
    if (client->mqtt_version == SPB_MQTT_VERSION_5) {
        /* MQTT 5 replaces Clean Session with Clean Start; Paho rejects a
         * non-zero cleansession for a v5 connect. */
        paho_opts->cleansession = 0;
        paho_opts->cleanstart = connect_options.clean_session;
    } else {
        paho_opts->cleansession = connect_options.clean_session;
        paho_opts->cleanstart = 0;
    }
    paho_opts->onSuccess = connect_onsuccess_wrapper_cb;
    paho_opts->onFailure = connect_onfailure_wrapper_cb;
    paho_opts->context = client;

    /* Paho deep-copies the will and SSL options inside MQTTAsync_connect, so
     * these locals (and the caller's strings) only need to outlive the call. */
    MQTTAsync_willOptions paho_will = MQTTAsync_willOptions_initializer;
    if (will != NULL) {
        paho_will.topicName = will->topic;
        paho_will.payload.data = will->payload;
        paho_will.payload.len = will->payloadlen;
        paho_will.qos = will->qos;
        paho_will.retained = will->retained;
        paho_opts->will = &paho_will;
    } else {
        paho_opts->will = NULL;
    }

    /* Always supplied (Paho ignores it for tcp://) so an ssl:// URI without
     * explicit options still gets the secure zero-initialised defaults. */
    const spb_ssl_options_t* ssl = connect_options.ssl != NULL ? connect_options.ssl : &default_ssl;
    MQTTAsync_SSLOptions paho_ssl = MQTTAsync_SSLOptions_initializer;
    paho_ssl.trustStore = ssl->trust_store;
    paho_ssl.CApath = ssl->ca_path;
    paho_ssl.keyStore = ssl->key_store;
    paho_ssl.privateKey = ssl->private_key;
    paho_ssl.privateKeyPassword = ssl->private_key_password;
    paho_ssl.enabledCipherSuites = ssl->enabled_cipher_suites;
    paho_ssl.sslVersion = ssl->ssl_version;
    paho_ssl.enableServerCertAuth = !ssl->disable_server_cert_auth;
    paho_ssl.verify = !ssl->disable_hostname_verify;
    paho_opts->ssl = &paho_ssl;

    int rc = MQTTAsync_connect(client->paho_lib.client, paho_opts);

    /* Don't leave the stored options pointing at this stack frame. */
    paho_opts->will = NULL;
    paho_opts->ssl = NULL;

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
    set_response_callbacks(client, &paho_resp_opts);
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
    set_response_callbacks(client, &paho_resp_opts);
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
        spb_successdata_t success_data = { .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE) };

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
            .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE),
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
        spb_successdata_t success_data = { .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE) };

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
            .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE),
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

/* Shared by the MQTT 3.x and MQTT 5 response wrappers: dispatch to the user
 * callback, then free the per-call context. */
static void dispatch_response_success(void* context, spb_successdata_t* success_data)
{
    if (context != NULL) {
        spb_response_ctx_t* ctx = (spb_response_ctx_t*)context;

        if (ctx->options.onsuccess_cb != NULL) {
            ctx->options.onsuccess_cb(ctx->options.context, success_data);
        }

        free(ctx);
    } else {
        SPB_LOG_ERROR("response on success callback invoked with NULL context — spb_response_ctx_t invariant violated, user callback not dispatched");
    }
}

static void dispatch_response_failure(void* context, spb_failuredata_t* failure_data)
{
    if (context != NULL) {
        spb_response_ctx_t* ctx = (spb_response_ctx_t*)context;

        if (ctx->options.onfailure_cb != NULL) {
            ctx->options.onfailure_cb(ctx->options.context, failure_data);
        }

        free(ctx);
    } else {
        SPB_LOG_ERROR("response on failure callback invoked with NULL context — spb_response_ctx_t invariant violated, user callback not dispatched");
    }
}

static void response_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    SPB_LOG_TRACE("response on success callback");
    spb_successdata_t success_data = { .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE) };
    dispatch_response_success(context, &success_data);
}

static void response_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    SPB_LOG_TRACE("response on failure callback");
    spb_failuredata_t failure_data = {
        .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE),
        .code = response ? response->code : 0,
        .message = response ? response->message : NULL,
    };
    dispatch_response_failure(context, &failure_data);
}

static void response_onsuccess5_wrapper_cb(void* context, MQTTAsync_successData5* response)
{
    SPB_LOG_TRACE("response on success (MQTT 5) callback");
    spb_successdata_t success_data = { .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE) };
    dispatch_response_success(context, &success_data);
}

static void response_onfailure5_wrapper_cb(void* context,  MQTTAsync_failureData5* response)
{
    SPB_LOG_TRACE("response on failure (MQTT 5) callback");
    spb_failuredata_t failure_data = {
        .token = (response ? (spb_token_t)response->token : SPB_TOKEN_NONE),
        .code = response ? response->code : 0,
        .message = response ? response->message : NULL,
    };
    dispatch_response_failure(context, &failure_data);
}