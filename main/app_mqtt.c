#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_esp8266.h"
#include "app_relay.h"
#include "app_sensors.h"
#include "app_thermostat.h"
#include "app_mqtt.h"

#include "cJSON.h"


#ifdef CONFIG_MQTT_OTA
#include "app_ota.h"
extern QueueHandle_t otaQueue;
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/ota"
#endif //CONFIG_MQTT_OTA



extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int PUBLISHED_BIT;
extern const int INIT_FINISHED_BIT;

int16_t mqtt_reconnect_counter;


extern int16_t connect_reason;
extern const int mqtt_disconnect;
#define FW_VERSION "0.02.05"

extern QueueHandle_t relayQueue;
/* extern QueueHandle_t thermostatQueue; */
extern QueueHandle_t mqttQueue;

static const char *TAG = "MQTTS_MQTTS";


#ifdef CONFIG_MQTT_OTA
#define OTA_NB 1
#else
#define OTA_NB 0
#endif //CONFIG_MQTT_OTA

#define NB_SUBSCRIPTIONS  (OTA_NB + CONFIG_MQTT_RELAYS_NB)

#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/thermostat"

const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] =
  {
#ifdef CONFIG_MQTT_OTA
    OTA_TOPIC,
#endif //CONFIG_MQTT_OTA
#if CONFIG_MQTT_RELAYS_NB
    RELAY_TOPIC"/0",
#if CONFIG_MQTT_RELAYS_NB > 1
    RELAY_TOPIC"/1",
#if CONFIG_MQTT_RELAYS_NB > 2
    RELAY_TOPIC"/2",
#if CONFIG_MQTT_RELAYS_NB > 3
    RELAY_TOPIC"/3",
#endif //CONFIG_MQTT_RELAYS_NB > 3
#endif //CONFIG_MQTT_RELAYS_NB > 2
#endif //CONFIG_MQTT_RELAYS_NB > 1
#endif //CONFIG_MQTT_RELAYS_NB
    /* THERMOSTAT_TOPIC */
  };


extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{

  if (strncmp(event->topic, RELAY_TOPIC, strlen(RELAY_TOPIC)) == 0) {
    char id=255;
    if (strncmp(event->topic, RELAY_TOPIC "/0", strlen(RELAY_TOPIC "/0")) == 0) {
      id=0;
    }
    if (strncmp(event->topic, RELAY_TOPIC "/1", strlen(RELAY_TOPIC "/1")) == 0) {
      id=1;
    }
    if (strncmp(event->topic, RELAY_TOPIC "/2", strlen(RELAY_TOPIC "/2")) == 0) {
      id=2;
    }
    if (strncmp(event->topic, RELAY_TOPIC "/3", strlen(RELAY_TOPIC "/3")) == 0) {
      id=3;
    }
    if(id == 255)
      {
        ESP_LOGI(TAG, "unexpected relay id");
        return;
      }
    if (event->data_len >= 32 )
      {
        ESP_LOGI(TAG, "unexpected relay cmd payload");
        return;
      }
    char tmpBuf[32];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    if (root) {
      cJSON * state = cJSON_GetObjectItem(root,"state");
      if (state) {
        char value = state->valueint;
        ESP_LOGI(TAG, "id: %d, value: %d", id, value);
        struct RelayMessage r={id, value};
        ESP_LOGE(TAG, "Sending to relayQueue with timeout");
        if (xQueueSend( relayQueue
                        ,( void * )&r
                        ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
          ESP_LOGE(TAG, "Cannot send to relayQueue");
        }
        ESP_LOGE(TAG, "Sending to relayQueue finished");
        return;
      }
    }
    ESP_LOGE(TAG, "bad json payload");
  }
#ifdef CONFIG_MQTT_OTA
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");

    }
  }
#endif //CONFIG_MQTT_OTA

  /* if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) { */
  /*   if (event->data_len >= 64 ) */
  /*     { */
  /*       ESP_LOGI(TAG, "unextected relay cmd payload"); */
  /*       return; */
  /*     } */
  /*   char tmpBuf[64]; */
  /*   memcpy(tmpBuf, event->data, event->data_len); */
  /*   tmpBuf[event->data_len] = 0; */
  /*   cJSON * root   = cJSON_Parse(tmpBuf); */
  /*   if (root) { */
  /*     cJSON * ttObject = cJSON_GetObjectItem(root,"targetTemperature"); */
  /*     cJSON * ttsObject = cJSON_GetObjectItem(root,"targetTemperatureSensibility"); */
  /*     struct ThermostatMessage t = {0,0}; */
  /*     if (ttObject) { */
  /*       float targetTemperature = ttObject->valuedouble; */
  /*       ESP_LOGI(TAG, "targetTemperature: %f", targetTemperature); */
  /*       t.targetTemperature = targetTemperature; */
  /*     } */
  /*     if (ttsObject) { */
  /*       float targetTemperatureSensibility = ttsObject->valuedouble; */
  /*       ESP_LOGI(TAG, "targetTemperatureSensibility: %f", targetTemperatureSensibility); */
  /*       t.targetTemperatureSensibility = targetTemperatureSensibility; */
  /*     } */
  /*     if (t.targetTemperature || t.targetTemperatureSensibility) { */
  /*       if (xQueueSend( thermostatQueue */
  /*                       ,( void * )&t */
  /*                       ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
  /*         ESP_LOGE(TAG, "Cannot send to thermostatQueue"); */
  /*       } */
  /*     } */
  /*   } */
  /* } */
}

void publish_connected_data(esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason);
      xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish connected data, msg_id=%d", msg_id);
      }
    }
}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
    void * unused;
    if (xQueueSend( mqttQueue
                    ,( void * )&unused
                    ,MQTT_QUEUE_TIMEOUT) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");
    }
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    mqtt_reconnect_counter=0;
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    connect_reason=mqtt_disconnect;
    xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | PUBLISHED_BIT | INIT_FINISHED_BIT);
    mqtt_reconnect_counter += 1; //one reconnect each 10 seconds
    ESP_LOGI(TAG, "mqtt_reconnect_counter: %d", mqtt_reconnect_counter);
    if (mqtt_reconnect_counter  > (6 * 5)) //5 mins, force wifi reconnect
      {
        esp_wifi_stop();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_wifi_start();
        mqtt_reconnect_counter = 0;
      }
    break;

  case MQTT_EVENT_SUBSCRIBED:
    xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    xEventGroupSetBits(mqtt_event_group, PUBLISHED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
    ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
    dispatch_mqtt_event(event);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  }
  return ESP_OK;
}

static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < NB_SUBSCRIPTIONS; i++) {
    xEventGroupClearBits(mqtt_event_group, SUBSCRIBED_BIT);
    msg_id = esp_mqtt_client_subscribe(client, SUBSCRIPTIONS[i], 0);
    if (msg_id > 0) {
      ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & SUBSCRIBED_BIT) {
          ESP_LOGI(TAG, "subscribe ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "subscribe ack not received, msg_id=%d", msg_id);
        }
    } else {
      ESP_LOGI(TAG, "failed to subscribe %s, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
    }
  }
}

esp_mqtt_client_handle_t mqtt_init()
{
  const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER,
    .event_handle = mqtt_event_handler,
    .cert_pem = (const char *)mqtt_iot_cipex_ro_pem_start,
    .client_id = CONFIG_MQTT_CLIENT_ID,
    .lwt_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected",
    .keepalive = MQTT_TIMEOUT
  };

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  return client;
}


void mqtt_start(esp_mqtt_client_handle_t client)
{
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

void handle_mqtt_sub_pub(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  void * unused;
  while(1) {
    if( xQueueReceive( mqttQueue, &unused , portMAX_DELAY) )
      {
        xEventGroupClearBits(mqtt_event_group, INIT_FINISHED_BIT);
        mqtt_subscribe(client);
        xEventGroupSetBits(mqtt_event_group, INIT_FINISHED_BIT);
        publish_connected_data(client);
        publish_all_relays_data(client);
        publish_thermostat_data(client);
#ifdef CONFIG_MQTT_OTA
        publish_ota_data(client, OTA_READY);
#endif //CONFIG_MQTT_OTA
        publish_sensors_data(client);

      }
  }
}



/* #include "string.h" */
/* #include "assert.h" */

/* #include "esp_log.h" */

/* #include "freertos/FreeRTOS.h" */
/* #include "freertos/event_groups.h" */
/* #include "freertos/queue.h" */

/* #include "app_esp8266.h" */

/* #include "app_mqtt.h" */


/* #include "app_ota.h" */

/* #if CONFIG_MQTT_RELAYS_NB */
/* #include "app_relay.h" */
/* extern QueueHandle_t relayQueue; */
/* #endif //CONFIG_MQTT_RELAYS_NB */

/* #ifdef CONFIG_MQTT_SENSOR_DHT22 */
/* #include "app_sensors.h" */
/* #endif //CONFIG_MQTT_SENSOR_DHT22 */
/* /\* #include "app_thermostat.h" *\/ */

/* #include "cJSON.h" */



/* extern EventGroupHandle_t wifi_event_group; */
/* extern EventGroupHandle_t mqtt_event_group; */
/* extern const int CONNECTED_BIT; */
/* extern const int SUBSCRIBED_BIT; */
/* extern const int PUBLISHED_BIT; */
/* extern const int INIT_FINISHED_BIT; */


/* int16_t connect_reason; */
/* const int boot = 0; */
/* const int mqtt_disconnect = 1; */

/* #define FW_VERSION "0.02.015" */

/* extern QueueHandle_t otaQueue; */
/* extern QueueHandle_t thermostatQueue; */
/* extern QueueHandle_t mqttQueue; */

/* static const char *TAG = "MQTTS_MQTTS"; */

/* #define MQTT_SSL 1 */


/* #ifdef MQTT_SSL */

/* #include "mqtt_iot_cipex_ro_cert.h" */

/* ssl_ca_crt_key_t ssl_cck; */

/* #define SSL_CA_CERT_KEY_INIT(s,a,b)  ((ssl_ca_crt_key_t *)s)->cacrt = a;\ */
/*                                      ((ssl_ca_crt_key_t *)s)->cacrt_len = b;\ */
/*                                      ((ssl_ca_crt_key_t *)s)->cert = NULL;\ */
/*                                      ((ssl_ca_crt_key_t *)s)->cert_len = 0;\ */
/*                                      ((ssl_ca_crt_key_t *)s)->key = NULL;\ */
/*                                      ((ssl_ca_crt_key_t *)s)->key_len = 0; */

/* #endif //MQTT_SSL */

/* #define NB_SUBSCRIPTIONS  (1 + CONFIG_MQTT_RELAYS_NB) */

/* #define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay" */
/* #define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/ota" */
/* #define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/cmd/thermostat" */

/* const char *SUBSCRIPTIONS[NB_SUBSCRIPTIONS] = */
/*   { */
/*     OTA_TOPIC, */
/* #if CONFIG_MQTT_RELAYS_NB */
/*     RELAY_TOPIC"/0", */
/* #if CONFIG_MQTT_RELAYS_NB > 1 */
/*     RELAY_TOPIC"/1", */
/* #if CONFIG_MQTT_RELAYS_NB > 2 */
/*     RELAY_TOPIC"/2", */
/* #if CONFIG_MQTT_RELAYS_NB > 3 */
/*     RELAY_TOPIC"/3", */
/* #endif //CONFIG_MQTT_RELAYS_NB > 3 */
/* #endif //CONFIG_MQTT_RELAYS_NB > 2 */
/* #endif //CONFIG_MQTT_RELAYS_NB > 1 */
/* #endif //CONFIG_MQTT_RELAYS_NB */
/*     /\* THERMOSTAT_TOPIC *\/ */
/*   }; */

/* #define MIN(a,b) ((a<b)?a:b) */

/* void dispatch_mqtt_event(MessageData* data) */
/* { */

/*   ESP_LOGI(TAG, "MQTT_EVENT_DATA"); */
/*   ESP_LOGI(TAG, "TOPIC=%.*s", data->topicName->lenstring.len, data->topicName->lenstring.data); */
/*   ESP_LOGI(TAG, "DATA=%.*s", data->message->payloadlen, (char*) data->message->payload); */

/*   const char * topic = data->topicName->lenstring.data; */
/*   int topicLen = data->topicName->lenstring.len; */

/* #if CONFIG_MQTT_RELAYS_NB */
/*   if (strncmp(topic, RELAY_TOPIC, MIN(topicLen, strlen(RELAY_TOPIC))) == 0) { */
/*     char id=255; */
/*     if (strncmp(topic, RELAY_TOPIC "/0", MIN(topicLen, strlen(RELAY_TOPIC "/0"))) == 0) { */
/*       id=0; */
/*     } */
/*     if (strncmp(topic, RELAY_TOPIC "/1", MIN(topicLen, strlen(RELAY_TOPIC "/1"))) == 0) { */
/*       id=1; */
/*     } */
/*     if (strncmp(topic, RELAY_TOPIC "/2", MIN(topicLen, strlen(RELAY_TOPIC "/2"))) == 0) { */
/*       id=2; */
/*     } */
/*     if (strncmp(topic, RELAY_TOPIC "/3", MIN(topicLen, strlen(RELAY_TOPIC "/3"))) == 0) { */
/*       id=3; */
/*     } */
/*     if(id == 255) */
/*       { */
/*         ESP_LOGI(TAG, "unexpected relay id"); */
/*         return; */
/*       } */
/*     if (data->message->payloadlen >= 32 ) */
/*       { */
/*         ESP_LOGI(TAG, "unexpected relay cmd payload"); */
/*         return; */
/*       } */
/*     char tmpBuf[32]; */
/*     memcpy(tmpBuf, data->message->payload, data->message->payloadlen); */
/*     tmpBuf[data->message->payloadlen] = 0; */
/*     cJSON * root   = cJSON_Parse(tmpBuf); */
/*     if (root) { */
/*       cJSON * state = cJSON_GetObjectItem(root,"state"); */
/*       if (state) { */
/*         char value = state->valueint; */
/*         ESP_LOGI(TAG, "id: %d, value: %d", id, value); */
/*         struct RelayMessage r={id, value}; */
/*         ESP_LOGE(TAG, "Sending to relayQueue with timeout"); */
/*         if (xQueueSend( relayQueue */
/*                         ,( void * )&r */
/*                         ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
/*           ESP_LOGE(TAG, "Cannot send to relayQueue"); */
/*         } */
/*         ESP_LOGE(TAG, "Sending to relayQueue finished"); */
/*         return; */
/*       } */
/*     } */
/*     ESP_LOGE(TAG, "bad json payload"); */
/*     return; */
/*   } */
/* #endif //CONFIG_MQTT_RELAYS_NB */

/*   if (strncmp(topic, OTA_TOPIC, MIN(topicLen, strlen(OTA_TOPIC))) == 0) { */
/*     struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"}; */
/*     if (xQueueSend( otaQueue */
/*                     ,( void * )&o */
/*                     ,MQTT_QUEUE_TIMEOUT) != pdPASS) { */
/*       ESP_LOGE(TAG, "Cannot send to relayQueue"); */

/*     } */
/*   } */

/*   /\* if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) { *\/ */
/*   /\*   if (event->data_len >= 64 ) *\/ */
/*   /\*     { *\/ */
/*   /\*       ESP_LOGI(TAG, "unextected relay cmd payload"); *\/ */
/*   /\*       return; *\/ */
/*   /\*     } *\/ */
/*   /\*   char tmpBuf[64]; *\/ */
/*   /\*   memcpy(tmpBuf, event->data, event->data_len); *\/ */
/*   /\*   tmpBuf[event->data_len] = 0; *\/ */
/*   /\*   cJSON * root   = cJSON_Parse(tmpBuf); *\/ */
/*   /\*   if (root) { *\/ */
/*   /\*     cJSON * ttObject = cJSON_GetObjectItem(root,"targetTemperature"); *\/ */
/*   /\*     cJSON * ttsObject = cJSON_GetObjectItem(root,"targetTemperatureSensibility"); *\/ */
/*   /\*     struct ThermostatMessage t = {0,0}; *\/ */
/*   /\*     if (ttObject) { *\/ */
/*   /\*       float targetTemperature = ttObject->valuedouble; *\/ */
/*   /\*       ESP_LOGI(TAG, "targetTemperature: %f", targetTemperature); *\/ */
/*   /\*       t.targetTemperature = targetTemperature; *\/ */
/*   /\*     } *\/ */
/*   /\*     if (ttsObject) { *\/ */
/*   /\*       float targetTemperatureSensibility = ttsObject->valuedouble; *\/ */
/*   /\*       ESP_LOGI(TAG, "targetTemperatureSensibility: %f", targetTemperatureSensibility); *\/ */
/*   /\*       t.targetTemperatureSensibility = targetTemperatureSensibility; *\/ */
/*   /\*     } *\/ */
/*   /\*     if (t.targetTemperature || t.targetTemperatureSensibility) { *\/ */
/*   /\*       if (xQueueSend( thermostatQueue *\/ */
/*   /\*                       ,( void * )&t *\/ */
/*   /\*                       ,MQTT_QUEUE_TIMEOUT) != pdPASS) { *\/ */
/*   /\*         ESP_LOGE(TAG, "Cannot send to thermostatQueue"); *\/ */
/*   /\*       } *\/ */
/*   /\*     } *\/ */
/*   /\*   } *\/ */
/*   /\* } *\/ */
/* } */

/* void publish_connected_data(MQTTClient* pclient) */
/* { */
/*   if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT) */
/*     { */
/*       const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected"; */
/*       char data[256]; */
/*       memset(data,0,256); */

/*       sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason); */

/*       MQTTMessage message; */
/*       message.qos = QOS1; */
/*       message.retained = 1; */
/*       message.payload = data; */
/*       message.payloadlen = strlen(data); */
/*       int rc = MQTTPublish(pclient, connect_topic, &message); */
/*       if (rc == 0) { */
/*         ESP_LOGI(TAG, "sent publish connected data successful, rc=%d", rc); */
/*       } else { */
/*         ESP_LOGI(TAG, "failed to publish connected data, rc=%d", rc); */
/*       } */
/*     } */
/* } */



/* static void mqtt_subscribe(MQTTClient* pclient) */
/* { */
/*   int rc; */

/*   for (int i = 0; i < NB_SUBSCRIPTIONS; i++) { */
/*     xEventGroupClearBits(mqtt_event_group, SUBSCRIBED_BIT); */
/*     if ((rc = MQTTSubscribe(pclient, SUBSCRIPTIONS[i], QOS1, dispatch_mqtt_event)) == 0) { */
/*       ESP_LOGI(TAG, "subscribed %s successful", SUBSCRIPTIONS[i]); */
/*     } else { */
/*       ESP_LOGI(TAG, "failed to subscribe %s, rc=%d", SUBSCRIPTIONS[i], rc); */
/*     } */
/*     //vTaskDelay(500 / portTICK_PERIOD_MS); */

/*   } */
/*   xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT); */

/* } */

/* MQTTClient* mqtt_init() */
/* { */
/* #ifdef MQTT_SSL */
/*   NetworkInitSSL(&network); */
/* #else */
/*   NetworkInit(&network); */
/* #endif //MQTT_SSL */
/*   unsigned char sendbuf[256] = {0}, readbuf[256] = {0}; */

/*   MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf)); */
/*   /\* if (MQTTClientInit(&client, &network, 0, NULL, 0, NULL, 0) == false) { *\/ */
/*   /\*   ESP_LOGE(TAG, "mqtt init err"); *\/ */
/*   /\* } *\/ */

/*   connectData.MQTTVersion = 3; */
/*   connectData.clientID.cstring = CONFIG_MQTT_CLIENT_ID; */
/*   connectData.username.cstring = CONFIG_MQTT_USERNAME; */
/*   connectData.password.cstring = CONFIG_MQTT_PASSWORD; */
/*   connectData.willFlag = 1; */
/*   connectData.will.topicName.cstring = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected"; */
/*   connectData.keepAliveInterval = MQTT_TIMEOUT; */

/*   return &client; */
/* } */

/* void mqtt_connect(void *pvParameter){ */

/*   if (NB_SUBSCRIPTIONS > MAX_MESSAGE_HANDLERS) { */
/*     ESP_LOGE(TAG, "MAX subscription limit(%d) passed", MAX_MESSAGE_HANDLERS); */
/*   } */

/*   MQTTClient* pclient = (MQTTClient*) pvParameter; */
/*   int rc; */
/*   connect_reason=boot; */
/*   while(true) { */
/*     ESP_LOGI(TAG, "wait/check wifi connect..."); */
/*     xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY); */
/*     ESP_LOGI(TAG, "received, now connecting"); */

/* #ifdef MQTT_SSL */
/*     SSL_CA_CERT_KEY_INIT(&ssl_cck, mqtt_iot_cipex_ro_pem, mqtt_iot_cipex_ro_pem_len); */

/*     if ((rc = NetworkConnectSSL(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT, &ssl_cck, TLSv1_1_client_method(), SSL_VERIFY_NONE, 8192)) != 0) { */
/* #else */
/*       if ((rc = NetworkConnect(&network, CONFIG_MQTT_SERVER, CONFIG_MQTT_PORT)) != 0) { */
/* #endif //MQTT_SSL */
/*         ESP_LOGE(TAG, "Return code from network connect is %d", rc); */
/*       } */
/*       if ((rc = MQTTConnect(pclient, &connectData)) != 0) { */
/*         ESP_LOGE(TAG, "Return code from MQTT connect is %d", rc); */
/*         network.disconnect(&network); */
/*         vTaskDelay(10000 / portTICK_PERIOD_MS); */
/*         continue; */
/*       } */

/*       ESP_LOGI(TAG, "MQTT Connected"); */

/* #if defined(MQTT_TASK) */

/*       if ((rc = MQTTStartTask(pclient)) != pdPASS) { */
/*         ESP_LOGE(TAG, "Return code from start tasks is %d", rc); */
/*       } else { */
/*         ESP_LOGI(TAG, "Use MQTTStartTask"); */
/*       } */

/* #endif */
/*       xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT); */
/*       mqtt_subscribe(pclient); */
/*       xEventGroupSetBits(mqtt_event_group, INIT_FINISHED_BIT); */
/*       publish_connected_data(pclient); */
/* #if CONFIG_MQTT_RELAYS_NB */
/*       publish_all_relays_data(pclient); */
/* #endif //CONFIG_MQTT_RELAYS_NB */
/*       publish_ota_data(pclient, OTA_READY); */
/*       /\* publish_thermostat_data(pclient); *\/ */

/* #ifdef CONFIG_MQTT_SENSOR_DHT22 */
/*       publish_sensors_data(pclient); */
/* #endif //CONFIG_MQTT_SENSOR_DHT22 */

/*       while (pclient->isconnected) { */
/*         vTaskDelay(1000 / portTICK_PERIOD_MS); */
/*       } */
/*       xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | PUBLISHED_BIT | INIT_FINISHED_BIT); */
/*       connect_reason=mqtt_disconnect; */
/*       network.disconnect(&network); */
/*       ESP_LOGE(TAG, "MQTT disconnected, will reconnect in 10 seconds"); */
/*       vTaskDelay(10000 / portTICK_PERIOD_MS); */
/*   } */
/* } */

/* void mqtt_start (MQTTClient* pclient) */
/* { */
/*   ESP_LOGI(TAG, "init mqtt (re)connect thread, fw version " FW_VERSION); */
/*   xTaskCreate(mqtt_connect, "mqtt_connect", configMINIMAL_STACK_SIZE * 5, pclient, 5, NULL); */
/*   xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY); */
/* } */

