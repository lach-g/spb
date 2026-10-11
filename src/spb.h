#ifndef SPB_H
#define SPB_H


#ifdef __cplusplus
extern "C" {
#endif


typedef struct spb_client spb_client_t;

/* Delivery / subscribe token that identifies a specific in-flight async
 * request.  The underlying width and signedness match Paho's MQTTAsync_token
 * exactly; spb.c pins that constraint at compile time with SPB_STATIC_ASSERT
 * so any future Paho redefinition breaks the build rather than silently
 * truncating.  Callers should treat the value as opaque: compare for equality
 * only, do not perform arithmetic on it. */
typedef int spb_token_t;

/* Return codes for the public spb_* API.
 *
 * The values in the -1..-19 range mirror Eclipse Paho's MQTTASYNC_* codes
 * one-to-one, so an underlying Paho failure is propagated verbatim while
 * application code only ever needs to include this header (no Paho leakage).
 * spb.c pins these to the MQTTASYNC_* values with _Static_assert.
 *
 * Codes at -100 and below are raised by the spb layer itself, before or
 * instead of a Paho call, and occupy a range Paho does not use. */
typedef enum spb_error {
    SPB_SUCCESS               = 0,   /* operation started successfully */

    /* Mirror of Paho MQTTASYNC_* (-1..-19), propagated verbatim */
    SPB_FAILURE               = -1,
    SPB_PERSISTENCE_ERROR     = -2,
    SPB_DISCONNECTED          = -3,
    SPB_MAX_MESSAGES_INFLIGHT = -4,
    SPB_BAD_UTF8_STRING       = -5,
    SPB_NULL_PARAMETER        = -6,
    SPB_TOPICNAME_TRUNCATED   = -7,
    SPB_BAD_STRUCTURE         = -8,
    SPB_BAD_QOS               = -9,
    SPB_NO_MORE_MSGIDS        = -10,
    SPB_OPERATION_INCOMPLETE  = -11,
    SPB_MAX_BUFFERED_MESSAGES = -12,
    SPB_SSL_NOT_SUPPORTED     = -13,
    SPB_BAD_PROTOCOL          = -14,
    SPB_BAD_MQTT_OPTION       = -15,
    SPB_WRONG_MQTT_VERSION    = -16,
    SPB_0_LEN_WILL_TOPIC      = -17,
    SPB_COMMAND_IGNORED       = -18,
    SPB_MAX_BUFFERED          = -19,

    /* spb-layer errors (reserved range, no collision with Paho) */
    SPB_ERR_INVALID_ARGUMENT  = -100, /* NULL / invalid argument to an spb_* call */
    SPB_ERR_NOMEM             = -101, /* allocation failure inside the spb layer */
} spb_error_t;

typedef struct {
    void* payload;
    int payloadlen;
    int retained;
    int qos;
} spb_message_t;

typedef struct {
    spb_token_t token;
} spb_successdata_t;

typedef struct {
    spb_token_t token;
    int code;
    const char *message;
} spb_failuredata_t;

/* Redefined MQTTAsync callbacks */
typedef void spb_connectionlost(void* context, char* cause);
    /* Return 1 to indicate message successfully processed, otherwise 0. Allocate and copy any message/topic pointers as on return the ones provided will be freed  */
typedef int spb_messagearrived(void* context, char* topicName, int topicLen, spb_message_t* message);
typedef void spb_deliverycomplete(void* context, spb_token_t token);
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
spb_error_t spb_create(spb_client_t* client, const char* uri, const char* client_id);
spb_error_t spb_setcallbacks(spb_client_t* client, void* context, spb_connectionlost* connection_lost_cb, spb_messagearrived* messagearrived_cb, spb_deliverycomplete* delivery_complete_cb);
spb_error_t spb_connect(spb_client_t* client, spb_connect_options_t connect_options);
spb_error_t spb_sendmessage(spb_client_t* client, const char* topic, const spb_message_t* message, spb_response_options_t response_options, spb_token_t* out_token);
spb_error_t spb_subscribe(spb_client_t* client, const char* topic, int qos, spb_response_options_t response, spb_token_t* out_token);
spb_error_t spb_disconnect(spb_client_t* client, spb_disconnect_options_t disconnect_options);
void spb_destroy(spb_client_t* client);

/* Human-readable, static string for an spb_error_t code. Never returns NULL. */
const char* spb_strerror(spb_error_t code);


#ifdef __cplusplus
}
#endif


#endif