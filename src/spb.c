#include "spb.h"

#include <MQTTAsync.h>

 /* TODO: Make generic? */
#include <stdlib.h>

typedef struct paho_client_cb {
    MQTTAsync_connectionLost* connection_lost_cb;
    MQTTAsync_messageArrived* messagearrived_cb;
    MQTTAsync_deliveryComplete* delivery_complete_cb;
} paho_client_cb_t;

typedef struct paho_lib {
    MQTTAsync client;
    MQTTAsync_connectOptions connect_options;
    MQTTAsync_responseOptions response_options;
    MQTTAsync_disconnectOptions disconnect_options;
    paho_client_cb_t client_cb;
} paho_lib_t;

struct spb_client {
    paho_lib_t paho_lib;
    spb_client_cb_t client_callbacks;
    spb_connect_options_t connect_options;
    spb_response_options_t response_options;
    spb_disconnect_options_t disconnect_options;
};

spb_client_t* spb_init(void)
{
    spb_client_t* client = malloc(sizeof(spb_client_t));
    if (client != NULL) {
        MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
        client->paho_lib.connect_options = conn_opts;

        MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
        client->paho_lib.disconnect_options = disc_opts;

        MQTTAsync_responseOptions resp_opts = MQTTAsync_responseOptions_initializer;
        client->paho_lib.response_options = resp_opts;
    }

    return client;
}

int spb_create(spb_client_t* client, const char* uri, const char* client_id)
{
    if (client == NULL ||
        uri == NULL ||
        client_id == NULL) {
        return -1;
    }

    if (MQTTAsync_create(&client->paho_lib.client, uri, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTASYNC_SUCCESS) {
        return -1;
    }

    return 0;
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

int spb_setcallbacks(
    spb_client_t* client,
    void* context,
    spb_connectionlost* connection_lost_cb,
    spb_messagearrived* messagearrived_cb,
    spb_deliverycomplete* delivery_complete_cb)
{
    if (client == NULL ||
        messagearrived_cb == NULL) {
        return -1;
    }

    client->client_callbacks.connection_lost_cb = connection_lost_cb;
    client->client_callbacks.messagearrived_cb = messagearrived_cb;
    client->client_callbacks.delivery_complete_cb = delivery_complete_cb;
    client->client_callbacks.context = context;

	if (MQTTAsync_setCallbacks(
	    client->paho_lib.client,
	    client,
	    connectionlost_wrapper_cb,
	    messagearrived_wrapper_cb,
	    deliverycomplete_wrapper_cb) != MQTTASYNC_SUCCESS) {
	    return -1;
	}

    return 0;
}

int spb_connect(spb_client_t* client, const spb_connect_options_t connect_options)
{
    if (client == NULL) {
        return -1;
    }

    client->connect_options = connect_options;

    client->paho_lib.connect_options.keepAliveInterval = client->connect_options.keep_alive_interval;
    client->paho_lib.connect_options.cleansession = client->connect_options.clean_session;
    client->paho_lib.connect_options.onSuccess = connect_onsuccess_wrapper_cb;
    client->paho_lib.connect_options.onFailure = connect_onfailure_wrapper_cb;
    client->paho_lib.connect_options.context = client;

    if (MQTTAsync_connect(client->paho_lib.client, &client->paho_lib.connect_options) != MQTTASYNC_SUCCESS) {
        return -1;
    }

    return 0;
}

int spb_sendmessage(spb_client_t* client, const char* topic, const spb_message_t* message, spb_response_options_t response_options)
{
    if (client == NULL ||
        topic == NULL ||
        message == NULL ||
        message->payload == NULL) {
        return -1;
    }

    client->response_options = response_options;
    client->paho_lib.response_options.onSuccess = response_onsuccess_wrapper_cb;
    client->paho_lib.response_options.onFailure = response_onfailure_wrapper_cb;
    client->paho_lib.response_options.context = client;

    MQTTAsync_message paho_msg = MQTTAsync_message_initializer;
    paho_msg.payload = message->payload;
    paho_msg.payloadlen = message->payloadlen;
    paho_msg.qos = message->qos;
    paho_msg.retained = message->retained;

	if (MQTTAsync_sendMessage(client->paho_lib.client, topic, &paho_msg, &client->paho_lib.response_options) != MQTTASYNC_SUCCESS) {
	    return -1;
	}

    return 0;
}

int spb_subscribe(spb_client_t* client, const char* topic, int qos, spb_response_options_t response_options)
{
    if (client == NULL ||
        topic == NULL) {
        return -1;
    }

    // TODO: is using the same response options overwriting the spb_sendmessage() version if it also publishes?
    client->response_options = response_options;
    client->paho_lib.response_options.onSuccess = response_onsuccess_wrapper_cb;
    client->paho_lib.response_options.onFailure = response_onfailure_wrapper_cb;
    client->paho_lib.response_options.context = client;

    if (MQTTAsync_subscribe(client->paho_lib.client, topic, qos, &client->paho_lib.response_options) != MQTTASYNC_SUCCESS) {
        return -1;
    }

    return 0;
}

int spb_disconnect(spb_client_t* client, spb_disconnect_options_t disconnect_options)
{
    if (client == NULL) {
        return -1;
    }

    client->disconnect_options = disconnect_options;
    client->paho_lib.disconnect_options.onSuccess = disconnect_onsuccess_wrapper_cb;
    client->paho_lib.disconnect_options.onFailure = disconnect_onfailure_wrapper_cb;
    client->paho_lib.disconnect_options.context = client;

    if (MQTTAsync_disconnect(client->paho_lib.client, &client->paho_lib.disconnect_options) != MQTTASYNC_SUCCESS) {
        return -1;
    }

    return 0;
}

void spb_destroy(spb_client_t* client)
{
    if (client != NULL) {
        MQTTAsync_destroy(&client->paho_lib.client);
        free(client);
    }
}

static void connectionlost_wrapper_cb(void* context, char* cause)
{
    printf("spb layer: connection lost callback");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        if (client->client_callbacks.connection_lost_cb != NULL) {
            client->client_callbacks.connection_lost_cb(client->client_callbacks.context, cause);
        }
    }
}

static int messagearrived_wrapper_cb(void* context, char* topic_name, int topic_len, MQTTAsync_message* message)
{
    printf("spb layer: message arrived callback\n");
    int rc = 0;

    if (context != NULL &&
        topic_name != NULL &&
        message != NULL) {
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
    printf("spb layer: delivery complete callback\n");
    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        if (client->client_callbacks.delivery_complete_cb != NULL) {
            client->client_callbacks.delivery_complete_cb(client->client_callbacks.context, token);
        }
    }
}

static void connect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    printf("spb layer: connect on success callback\n");
    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_successdata_t success_data = { .token = response->token };

        if (client->connect_options.onsuccess_cb != NULL) {
            client->connect_options.onsuccess_cb(client->connect_options.context, &success_data);
        }
    }
}

static void connect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    printf("spb layer: connect on failure callback\n");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_failuredata_t failure_data = {
            .token = response->token,
            .code = response->code,
            .message = response->message,
        };

        if (client->connect_options.onfailure_cb != NULL) {
            client->connect_options.onfailure_cb(client->connect_options.context, &failure_data);
        }
    }
}

static void disconnect_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    printf("spb layer: disconnect on success callback\n");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_successdata_t success_data = { .token = response->token };

        if (client->disconnect_options.onsuccess_cb != NULL) {
            client->disconnect_options.onsuccess_cb(client->disconnect_options.context, &success_data);
        }
    }
}

static void disconnect_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    printf("spb layer: disconnect on failure callback\n");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_failuredata_t failure_data = {
            .token = response->token,
            .code = response->code,
            .message = response->message,
        };

        if (client->disconnect_options.onfailure_cb != NULL) {
            client->disconnect_options.onfailure_cb(client->disconnect_options.context, &failure_data);
        }
    }
}

static void response_onsuccess_wrapper_cb(void* context, MQTTAsync_successData* response)
{
    printf("spb layer: response on success callback\n");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_successdata_t success_data = { .token = response->token };

        if (client->response_options.onsuccess_cb != NULL) {
            client->response_options.onsuccess_cb(client->response_options.context, &success_data);
        }
    }
}

static void response_onfailure_wrapper_cb(void* context,  MQTTAsync_failureData* response)
{
    printf("spb layer: response on failure callback\n");

    if (context != NULL) {
        spb_client_t* client = (spb_client_t*)context;
        spb_failuredata_t failure_data = {
            .token = response->token,
            .code = response->code,
            .message = response->message,
        };

        if (client->response_options.onfailure_cb != NULL) {
            client->response_options.onfailure_cb(client->response_options.context, &failure_data);
        }
    }
}