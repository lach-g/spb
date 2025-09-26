#include <stdio.h>
#include <stdlib.h>
#include "spb.h"

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <windows.h>
#endif

#if defined(_WRS_KERNEL)
#include <OsWrapper.h>
#endif

#define ADDRESS     "tcp://test.mosquitto.org:1883"
#define CLIENTID    "spb-c-subscriber"
#define TOPIC       "spb"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

int disc_finished = 0;
int subscribed = 0;
int finished = 0;

void onConnect(void* context, spb_successdata_t* response);
void onConnectFailure(void* context, spb_failuredata_t* response);

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


int msgarrvd(void *context, char *topicName, int topicLen, spb_message_t *message)
{
    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);
    return 1;
}

void onDisconnectFailure(void* context, spb_failuredata_t* response)
{
	printf("Disconnect failed, rc %d\n", response->code);
	disc_finished = 1;
}

void onDisconnect(void* context, spb_successdata_t* response)
{
	printf("Successful disconnection\n");
	disc_finished = 1;
}

void onSubscribe(void* context, spb_successdata_t* response)
{
	printf("Subscribe succeeded\n");
	subscribed = 1;
}

void onSubscribeFailure(void* context, spb_failuredata_t* response)
{
	printf("Subscribe failed, rc %d\n", response->code);
	finished = 1;
}


void onConnectFailure(void* context, spb_failuredata_t* response)
{
	printf("Connect failed, rc %d\n", response->code);
	finished = 1;
}


void onConnect(void* context, spb_successdata_t* response)
{
	spb_client_t* client = context;
	spb_response_options_t response_options = {
		.onsuccess_cb = onSubscribe,
		.onfailure_cb = onSubscribeFailure,
		.context = client,
	};

	printf("Successful connection\n");

	printf("Subscribing to topic %s\nfor client %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
	int rc;
	if ((rc = spb_subscribe(client, TOPIC, QOS, response_options)) != 0) {
		printf("Failed to start subscribe, return code %d\n", rc);
		finished = 1;
	}
}


int main(int argc, char* argv[])
{
	spb_client_t* client = spb_init();
	int rc;
	int ch;

	const char* uri = (argc > 1) ? argv[1] : ADDRESS;
	printf("Using server at %s\n", uri);

	if ((rc = spb_create(client, uri, CLIENTID))
			!= 0) {
		printf("Failed to create client, return code %d\n", rc);
		rc = EXIT_FAILURE;
		goto exit;
	}

	if ((rc = spb_setcallbacks(client, client, connlost, msgarrvd, NULL)) != 0) {
		printf("Failed to set callbacks, return code %d\n", rc);
		rc = EXIT_FAILURE;
		goto destroy_exit;
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
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}

	while (!subscribed && !finished)
		#if defined(_WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif

	if (finished)
		goto exit;

	do {
		ch = getchar();
	} while (ch!='Q' && ch != 'q');

	spb_disconnect_options_t disconnect_options = {
		.onsuccess_cb = onDisconnect,
		.onfailure_cb = onDisconnectFailure,
	};

	if ((rc = spb_disconnect(client, disconnect_options)) != 0) {
		printf("Failed to start disconnect, return code %d\n", rc);
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}

 	while (!disc_finished) {
		#if defined(_WIN32)
			Sleep(100);
		#else
			usleep(10000L);
		#endif
 	}

destroy_exit:
	spb_destroy(client);
exit:
 	return rc;
}