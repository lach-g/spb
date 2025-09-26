#ifndef SPB_H
#define SPB_H


#ifdef __cplusplus
extern "C" {
#endif


typedef struct spb_client spb_client_t;

typedef struct {
    void* payload;
    int payloadlen;
    int retained;
    int qos;
} spb_message_t;

typedef struct {
    int token;
} spb_successdata_t;

typedef struct {
    int token;
    int code;
    const char *message;
} spb_failuredata_t;

/* Redefined MQTTAsync callbacks */
typedef void spb_connectionlost(void* context, char* cause);
    /* Return 1 to indicate message successfully processed, otherwise 0. Allocate and copy any message/topic pointers as on return the ones provided will be freed  */
typedef int spb_messagearrived(void* context, char* topicName, int topicLen, spb_message_t* message);
typedef void spb_deliverycomplete(void* context, int token);
typedef void spb_onsuccess(void* context, spb_successdata_t* response);
typedef void spb_onfailure(void* context,  spb_failuredata_t* response);

typedef struct {
    spb_connectionlost* connection_lost_cb;
    spb_messagearrived* messagearrived_cb;
    spb_deliverycomplete* delivery_complete_cb;
    void* context;
} spb_client_cb_t;

typedef struct {
    int keep_alive_interval;
    int clean_session;
    spb_onsuccess* onsuccess_cb;
    spb_onfailure* onfailure_cb;
    void* context;
} spb_connect_options_t;

typedef struct {
    spb_onsuccess* onsuccess_cb;
    spb_onfailure* onfailure_cb;
    void* context;
} spb_response_options_t;

typedef struct {
    spb_onsuccess* onsuccess_cb;
    spb_onfailure* onfailure_cb;
    void* context;
} spb_disconnect_options_t;


spb_client_t* spb_init(void);
int spb_create(spb_client_t* client, const char* uri, const char* client_id);
int spb_setcallbacks(spb_client_t* client, void* context, spb_connectionlost* connection_lost_cb, spb_messagearrived* messagearrived_cb, spb_deliverycomplete* delivery_complete_cb);
int spb_connect(spb_client_t* client, spb_connect_options_t connect_options);
int spb_sendmessage(spb_client_t* client, const char* topic, const spb_message_t* message, spb_response_options_t response_options);
int spb_subscribe(spb_client_t* client, const char* topic, int qos, spb_response_options_t response);
int spb_disconnect(spb_client_t* client, spb_disconnect_options_t disconnect_options);
void spb_destroy(spb_client_t* client);


#ifdef __cplusplus
}
#endif


#endif