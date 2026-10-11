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
 * only, do not perform arithmetic on it.
 *
 * QoS 1/2 publishes and subscribes get a token in 1..65535 (the MQTT packet
 * id). Tokens are reused once a request completes, so a token only
 * identifies a request while it is in flight. */
typedef int spb_token_t;

/* No token. QoS 0 publishes have no packet id, so spb_sendmessage reports
 * this for them (and spb_deliverycomplete never fires for QoS 0). Wrapper
 * callbacks also report it if Paho supplies no response data. */
#define SPB_TOKEN_NONE ((spb_token_t)0)

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

/* MQTT protocol version. Values mirror Paho's MQTTVERSION_* (pinned in spb.c).
 * Selected once at spb_create_with_options time and used for every connect
 * on that client, because Paho fixes the persisted message format at create. */
typedef enum spb_mqtt_version {
    SPB_MQTT_VERSION_DEFAULT = 0, /* try 3.1.1, fall back to 3.1 */
    SPB_MQTT_VERSION_3_1     = 3,
    SPB_MQTT_VERSION_3_1_1   = 4,
    SPB_MQTT_VERSION_5       = 5,
} spb_mqtt_version_t;

/* Where Paho keeps QoS 1/2 in-flight messages. Zero (NONE) is the default. */
typedef enum spb_persistence {
    SPB_PERSISTENCE_NONE = 0, /* in-memory only; lost if the process exits */
    SPB_PERSISTENCE_FILE = 1, /* Paho's file-system persistence */
} spb_persistence_t;

/* Options for spb_create_with_options. A zero-initialised struct gives the
 * same behaviour as spb_create. Strings are copied; the caller may free them
 * once the call returns. */
typedef struct {
    spb_mqtt_version_t mqtt_version;
    spb_persistence_t persistence;
    const char* persistence_dir; /* SPB_PERSISTENCE_FILE only; NULL = working directory */
} spb_create_options_t;

/* MQTT Last Will and Testament, published by the broker on the client's
 * behalf if the connection drops ungracefully. payload is binary-safe.
 * Topic and payload are copied by spb_connect; the caller may free them once
 * spb_connect returns. */
typedef struct {
    const char* topic;      /* required, non-empty */
    const void* payload;    /* may be NULL only if payloadlen is 0 */
    int payloadlen;
    int qos;
    int retained;
} spb_will_options_t;

/* TLS protocol version. Values mirror Paho's MQTT_SSL_VERSION_* (pinned in spb.c). */
typedef enum spb_ssl_version {
    SPB_SSL_VERSION_DEFAULT = 0,
    SPB_SSL_VERSION_TLS_1_0 = 1,
    SPB_SSL_VERSION_TLS_1_1 = 2,
    SPB_SSL_VERSION_TLS_1_2 = 3,
} spb_ssl_version_t;

/* TLS settings, used when the broker URI scheme is ssl:// or wss://.
 * File paths are PEM. A zero-initialised struct is secure by default: the
 * server certificate is verified and so is its host name. Strings are copied
 * by spb_connect; the caller may free them once spb_connect returns. */
typedef struct {
    const char* trust_store;           /* CA certificate(s) trusted by the client */
    const char* ca_path;               /* directory of CA certificates */
    const char* key_store;             /* client certificate chain (mutual TLS) */
    const char* private_key;           /* client private key, if not in key_store */
    const char* private_key_password;
    const char* enabled_cipher_suites; /* OpenSSL cipher list; NULL = library default */
    spb_ssl_version_t ssl_version;
    int disable_server_cert_auth;      /* non-zero skips server certificate verification */
    int disable_hostname_verify;       /* non-zero skips the certificate host name check */
} spb_ssl_options_t;

typedef struct {
    int keep_alive_interval;
    int clean_session;         /* sent as Clean Start under MQTT 5 */
    const spb_will_options_t* will; /* NULL = no will */
    const spb_ssl_options_t* ssl;   /* NULL = zero-initialised spb_ssl_options_t; ignored for tcp:// */
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
/* As spb_create, with protocol version and persistence options. options may be NULL. */
spb_error_t spb_create_with_options(spb_client_t* client, const char* uri, const char* client_id, const spb_create_options_t* options);
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