// TODO: Change this to <...>
#include "../src/spb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

#if defined(_WRS_KERNEL)
#include <OsWrapper.h>
#endif

#define ADDRESS     "tcp://test.mosquitto.org:1883"
#define CLIENTID    "spb-c-publisher"
#define TOPIC       "spb"
#define PAYLOAD     "Hello there"
#define QOS         1
#define TIMEOUT     10000L

int finished = 0;


void onDisconnectFailure(void* context, spb_failuredata_t* response)
{
	printf("Disconnect failed\n");
	finished = 1;
}

void onDisconnect(void* context, spb_successdata_t* response)
{
	printf("Successful disconnection\n");
	finished = 1;
}

void onSendFailure(void* context, spb_failuredata_t* response)
{
	spb_client_t* client = context;

	printf("Message send failed token %d error code %d\n", response->token, response->code);
	const spb_disconnect_options_t disconnect_options = {
		.onsuccess_cb = onDisconnect,
		.onfailure_cb = onDisconnectFailure,
		.context = client
	};

	int rc;
	if ((rc = spb_disconnect(client, disconnect_options)) != 0) {
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void onSend(void* context, spb_successdata_t* response)
{
	spb_client_t* client = context;

	printf("Message with token value %d delivery confirmed\n", response->token);
	const spb_disconnect_options_t disconnect_options = {
		.onsuccess_cb = onDisconnect,
		.onfailure_cb = onDisconnectFailure,
		.context = client
	};

	int rc;
	if ((rc = spb_disconnect(client, disconnect_options)) != 0) {
		printf("Failed to start disconnect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void onConnectFailure(void* context, spb_failuredata_t* response)
{
	printf("Connect failed, rc %d\n", response ? response->code : 0);
	finished = 1;
}

void onConnect(void* context, spb_successdata_t* response)
{
	spb_client_t* client = context;

	printf("Successful connection\n");

	spb_response_options_t response_options = {
		.onsuccess_cb = onSend,
		.onfailure_cb = onSendFailure,
		.context = client,
	};

	const spb_message_t message = {
		.payload = PAYLOAD,
		.payloadlen = (int)strlen(PAYLOAD),
		.qos = QOS,
		.retained = 0
	};

	int rc;
	if ((rc = spb_sendmessage(client, TOPIC, &message, response_options)) != 0) {
		printf("Failed to start sendMessage, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}
}

void connlost(void *context, char *cause)
{
	spb_client_t* client = context;
	spb_connect_options_t connect_options = {
		.keep_alive_interval = 20,
		.clean_session = 1,
		.onsuccess_cb = onConnect,
		.onfailure_cb = onConnectFailure,
		.context = client
	};

	printf("\nConnection lost\n");
	if (cause)
		printf("     cause: %s\n", cause);
	printf("Reconnecting\n");

	int rc;
	if ((rc = spb_connect(client, connect_options)) != 0) {
		printf("Failed to start connect, return code %d\n", rc);
		finished = 1;
	}
}

int messageArrived(void* context, char* topicName, int topicLen, spb_message_t* m)
{
	/* not expecting any messages */
	return 1;
}

void deliveryComplete(void* context, int token)
{
	printf("Delivery complete\n");
}

int main(int argc, char* argv[]) {

	int rc;

	spb_client_t* client = spb_init();
	if (client == NULL) {
		printf("Failed to initialize spb client\n");
		exit(EXIT_FAILURE);
	}

	const char* uri = ADDRESS;
	printf("Using server at %s\n", uri);

	if ((rc = spb_create(client, uri, CLIENTID)) != 0) {
		printf("Failed to create client object, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if ((rc = spb_setcallbacks(client, client, connlost, messageArrived, deliveryComplete)) != 0) {
		printf("Failed to set callback, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	spb_connect_options_t connect_options = {
		.keep_alive_interval = 20,
		.clean_session = 1,
		.onsuccess_cb = onConnect,
		.onfailure_cb = onConnectFailure,
		.context = client
	};

	if ((rc = spb_connect(client, connect_options)) != 0) {
		printf("Failed to start connect, return code %d\n", rc);
		exit(EXIT_FAILURE);
	}

	printf("Waiting for publication of %s on topic %s for client with ClientID: %s\n", PAYLOAD, TOPIC, CLIENTID);
	while (!finished)
		#if defined(_WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	spb_destroy(client);
 	return rc;
}