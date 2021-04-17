#include <EIL.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <ArduinoJson.h>
#include <Sensor.h>
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <sdkconfig.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_task_wdt.h"
#include "driver/ledc.h"
#include <esp_tls.h>
#include <esp_http_client.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"

#include <lmic.h>
#include <hal/hal.h>

#undef CONFIG_HTTPD_MAX_REQ_HDR_LEN
#define CONFIG_HTTPD_MAX_REQ_HDR_LEN 1024

#undef HTTPD_MAX_REQ_HDR_LEN
#define HTTPD_MAX_REQ_HDR_LEN 1024

#undef INCLUDE_vTaskSuspend
#define INCLUDE_vTaskSuspend 1

#undef CONFIG_HTTPD_WS_SUPPORT
#define CONFIG_HTTPD_WS_SUPPORT y

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY 4

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5        /* Time ESP32 will go to sleep (in seconds) */

Sensor sensor;
bool websocket_connected = false;
WiFiMulti wifiMulti;
TaskHandle_t EILTask;
void EILTaskPinnedToCore1(void *params);
TaskHandle_t DownloadFirmwareTask;

EIL eil;

bool wifiConnected = false;
bool upgradeRequest = false;

char *initScript;
char *loopScript;

uint16_t bufferdInt;
String output;

struct async_resp_arg
{
  httpd_handle_t hd;
  int fd;
};

uint8_t SSID[32] = "2.4";
uint8_t PASSPHARSE[64] = "tenerife";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

/***************************************LORA STUFF**********************/

static const u1_t PROGMEM NWKSKEY[16] = {0xDA, 0x80, 0x85, 0x85, 0xB3, 0x1F, 0x8B, 0x4F, 0xB0, 0x4D, 0x01, 0xD9, 0x2D, 0x95, 0x5A, 0x70}; // MSB...LSB
static const u1_t PROGMEM APPSKEY[16] = {0x8C, 0x97, 0xB1, 0x84, 0x02, 0xD5, 0x1B, 0xF2, 0x58, 0x19, 0x4F, 0x8D, 0xD7, 0x1C, 0x2D, 0x99}; // MSB...LSB
static const u4_t DEVADDR = 0x26013276;

void os_getArtEui(u1_t *buf) {}
void os_getDevEui(u1_t *buf) {}
void os_getDevKey(u1_t *buf) {}

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 12,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LMIC_UNUSED_PIN,
    .dio = {13, 5, LMIC_UNUSED_PIN},
};

/***************************************LORA STUFF*************************************/

void downloadFirmware(void *params);
const char *getScript();
void setScript(const char *script);
void initialize_HTTP_Server();

void delay_(char *res) { delay(atoi(res)); }

void printConsole(char *res)
{
  Serial.println(res);
}

char *_high_(char *res)
{
  return (char *)"TRUE";
}
char *_low_(char *res)
{
  return (char *)"FALSE";
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
  switch (evt->event_id)
  {
  case HTTP_EVENT_ERROR:
    ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
    //printf("%.*s", evt->data_len, (char *)evt->data);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_ON_DATA:
    //Serial.printf("%.*s", evt->data_len, (char *)evt->data);
    char script[evt->data_len];
    sprintf(script, "%.*s}\n", evt->data_len, (const char *)evt->data);
    Serial.println("\nCode: \n " + String(script) + "\n\n");
    if (upgradeRequest)
      setScript((const char *)script);

    break;
  }
  return ESP_OK;
}

// GPIO26

bool isNum(char *string)
{
  for (int i = 0; i < strlen(string); i++)
  {
    if (string[i] < 48 || string[i] > 57)
      return false;
  }
  return true;
}

char *getInputOutput26(char *res)
{
  if (strcmp((const char *)res, "TRUE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_26);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_26, 1);
    return (char *)"DONE";
  }
  else if (strcmp((const char *)res, "FALSE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_26);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_26, 0);
    return (char *)"DONE";
  }
  else if (isNum(res))
  {
    Serial.println(res);
    ledcSetup(0, 5000, 12); // Channel 0, 5000Hz, 12 Bit Timer
    ledcAttachPin(26, 0);
    ledcWrite(0, (atoi(res) / 100) * 4095);
    return NULL;
  }
  else if (strcmp((const char *)res, "LD") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_26);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    char *array = (char *)malloc(128); // SEHR WICHTIG, sunst ged goanix!
    strcpy(array, (gpio_get_level(GPIO_NUM_26) == 1) ? "TRUE" : "FALSE");
    return array;
  }
  else
  {
    return (char *)"0";
  }
}

// GPIO27

char *getInputOutput27(char *res)
{
  //Serial.println("Hallo : " + String(res));
  if (strcmp(res, "TRUE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_27);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_27, 1);
    return (char *)"DONE";
  }
  else if (strcmp(res, "FALSE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_27);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_27, 0);
    return (char *)"DONE";
  }
  else if (isNum(res))
  {
    Serial.println(res);
    ledcSetup(0, 5000, 12); // Channel 0, 5000Hz, 12 Bit Timer
    ledcAttachPin(25, 0);
    ledcWrite(0, (atoi(res) / 100) * 4095);
    return NULL;
  }
  else if (strcmp(res, "LD") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_27);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    char *array = (char *)malloc(128); // SEHR WICHTIG, sunst ged goanix!
    strcpy(array, (gpio_get_level(GPIO_NUM_27) == 1) ? "TRUE" : "FALSE");
    return array;
  }
  else
  {
    return (char *)"0";
  }
}

char *getOutput25(char *res)
{
  //Serial.println("Hallo : " + String(res));
  if (strcmp(res, "TRUE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_25);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_25, 1);
    return (char *)"DONE";
  }
  else if (strcmp(res, "FALSE") == 0)
  {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_25);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_25, 0);
    return (char *)"DONE";
  }
  else if (isNum(res))
  {
    Serial.println(res);
    ledcSetup(0, 5000, 12); // Channel 0, 5000Hz, 12 Bit Timer
    ledcAttachPin(25, 0);
    int valuePerCent = (int)((float)(atof(res) / 100.0f) * 4096);
    ledcWrite(0, valuePerCent);
    return NULL;
  }
  else if (strcmp(res, "LD") == 0)
  {
    return (char *)"FALSE";
  }
  else
  {
    return (char *)"0";
  }
}

char *getADC_CH0(char *res)
{
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten((adc1_channel_t)ADC_CHANNEL_0, ADC_ATTEN_DB_11);
  //for best possible result, we use a simple multisample algorithm:
  uint32_t adc_reading = 0;
  for (int i = 0; i < 32; i++)
  {
    adc_reading += adc1_get_raw((adc1_channel_t)ADC_CHANNEL_0);
  }
  //Serial.printf("ADC Reading: %d\n",adc_reading/32);
  //SUPER DUPER BAD , BUT I JUST WORKS
  //no, just using char ret[50]; instead of  char *ret = (char *)malloc(50); doesnt work
  char *ret = (char *)malloc(16);
  sprintf(ret, "%d", adc_reading / 32);
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

void PRINT(char *res)
{
  Serial.print(res);
}

void PRINTLN(char *res)
{
  Serial.println(res);
}

char *temp(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getTemperature());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *pres(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getPressure());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *ax(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getAccelX_G());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *ay(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getAccelY_G());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *az(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getAccelZ_G());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *ToFm(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%.2f", sensor.getDistance_m());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *ToFcm(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%d", sensor.getDistance_cm());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

char *ToFmm(char *res)
{
  char *ret = (char *)malloc(16);
  sprintf(ret, "%d", sensor.getDistance_mm());
  char *retcopy = ret;
  free(ret);
  return retcopy;
}

void removeChar(char *s, char c)
{
  int j, n = strlen(s);
  for (int i = j = 0; i < n; i++)
    if (s[i] != c)
      s[j++] = s[i];

  s[j] = '\0';
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id)
  {
  case SYSTEM_EVENT_STA_START:
    //Serial.println("STA Start done.");
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    Serial.printf("IP :  %s\n", ip4addr_ntoa(&ip_info.ip));

    if (upgradeRequest)
    {
      xTaskCreate(
          downloadFirmware,     /* Task function. */
          "downloadFirmware",   /* name of task. */
          10920,                /* Stack size of task */
          NULL,                 /* parameter of the task */
          2,                    /* priority of the task */
          &DownloadFirmwareTask /* Task handle to keep track of created task */
      );
    }

    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

void WiFiConnect(char *res)
{
  char **params = (char **)malloc(164);
  eil.getParameters(&res, params);
  eil.removeChar(params[0], '"');
  eil.removeChar(params[1], '"');

  Serial.println("Connecting to WiFi...");
  esp_task_wdt_reset();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
  wifi_event_group = xEventGroupCreate();

  esp_log_level_set("wifi", ESP_LOG_NONE); // disable wifi driver logging
  tcpip_adapter_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_config_t apconf = {};
  memcpy(apconf.sta.ssid, (const char *)eil.getVariable(params[0]), 32);
  memcpy(apconf.sta.password, (const char *)eil.getVariable(params[1]), 64);
  apconf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &apconf));

  esp_err_t esp_err;
  esp_err = esp_wifi_connect();
  if (esp_err != ESP_OK)
  {
    Serial.println("Not connecting! Code:" + String(ESP_OK));
  }

  free(params);
  initialize_HTTP_Server();
}

void goInDeepSleep(char *res)
{
  char **params = (char **)malloc(164);
  eil.getParameters(&res, params);
  eil.removeChar(params[0], '"');
  esp_sleep_enable_timer_wakeup(atoi(params[0]) * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void HttpGet(char *res)
{
  char **params = (char **)malloc(164);
  eil.getParameters(&res, params);
  eil.removeChar(params[0], '"');
  eil.removeChar(params[1], '"');

  esp_http_client_config_t config = {};
  char *query_data = (char *)malloc(512);
  sprintf(query_data, "%s?value=%s", params[0], params[1]);
  // "http://192.168.0.159:8080/thingNode";
  config.url = query_data;
  config.event_handler = _http_event_handle;
  config.query = "";
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGI(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  esp_http_client_cleanup(client);
  free(params);
  free(query_data);
}

void HttpPost(char *res)
{
  char **params = (char **)malloc(164);
  eil.getParameters(&res, params);
  eil.removeChar(params[0], '"');
  char *post_data = (char *)malloc(512);
  sprintf(post_data, "{\"value\":\"%s\"}", eil.getVariable(params[1]));
  esp_http_client_config_t config = {};
  // "http://192.168.0.159:8080/forsterMessaging"
  config.url = params[0];
  config.event_handler = _http_event_handle;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, post_data, strlen(post_data));
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGI(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  esp_http_client_cleanup(client);
  free(params);
  free(post_data);
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16)
{
  const uint8_t *src = (const uint8_t *)mem;
  for (uint32_t i = 0; i < len; i++)
  {
    if (i % cols == 0)
    {
    }
    src++;
  }
}

void onEvent(ev_t ev)
{
  //Serial.print(os_getTime());
  //Serial.print(": ");
  switch (ev)
  {
  case EV_SCAN_TIMEOUT:
    Serial.println(F("EV_SCAN_TIMEOUT"));
    break;
  case EV_BEACON_FOUND:
    Serial.println(F("EV_BEACON_FOUND"));
    break;
  case EV_BEACON_MISSED:
    Serial.println(F("EV_BEACON_MISSED"));
    break;
  case EV_BEACON_TRACKED:
    Serial.println(F("EV_BEACON_TRACKED"));
    break;
  case EV_JOINING:
    Serial.println(F("EV_JOINING"));
    break;
  case EV_JOINED:
    Serial.println(F("EV_JOINED"));
    break;
  case EV_RFU1:
    Serial.println(F("EV_RFU1"));
    break;
  case EV_JOIN_FAILED:
    Serial.println(F("EV_JOIN_FAILED"));
    break;
  case EV_REJOIN_FAILED:
    Serial.println(F("EV_REJOIN_FAILED"));
    break;
  case EV_TXCOMPLETE:
    //Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
    //digitalWrite(25, HIGH);
    //delay(100);
    //digitalWrite(4, HIGH);
    if (LMIC.txrxFlags & TXRX_ACK)
    {
      Serial.println(F("Received ack"));
      Serial.print(F("Received "));
      Serial.print(LMIC.dataLen);
      Serial.print(F(" bytes of payload: "));
      //received_info = "";
      for (int i = 0; i < LMIC.dataLen; i++)
      {
        if (LMIC.frame[LMIC.dataBeg + i] < 0x10)
        {
          Serial.print(F("0"));
        }
        Serial.print(char(LMIC.frame[LMIC.dataBeg + i]));
        //received_info += char(LMIC.frame[LMIC.dataBeg + i]);
      }
    }
    // Schedule next transmission
    //os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
    break;
  case EV_LOST_TSYNC:
    Serial.println(F("EV_LOST_TSYNC"));
    break;
  case EV_RESET:
    Serial.println(F("EV_RESET"));
    break;
  case EV_RXCOMPLETE:
    // data received in ping slot
    Serial.println(F("EV_RXCOMPLETE"));
    break;
  case EV_LINK_DEAD:
    Serial.println(F("EV_LINK_DEAD"));
    break;
  case EV_LINK_ALIVE:
    Serial.println(F("EV_LINK_ALIVE"));
    break;
  default:
    Serial.println(F("Unknown event"));
    break;
  }
}

void WSOpen(char *res) {}
void WSSend(char *res) {}
void WSClose(char *res) {}

void forceTxSingleChannelDr()
{
  int channel = 0;
  int dr = DR_SF7;

  for (int i = 0; i < 9; i++)
  { // For EU; for US use i<71
    if (i != channel)
    {
      LMIC_disableChannel(i);
    }
  }
  // Set data rate (SF) and transmit power for uplink
  LMIC_setDrTxpow(dr, 14);
}

void LoRaSetup()
{
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

// Set static session parameters. Instead of dynamically establishing a session
// by joining the network, precomputed session parameters are be provided.
#ifdef PROGMEM
  // On AVR, these values are stored in flash and only copied to RAM
  // once. Copy them to a temporary buffer here, LMIC_setSession will
  // copy them into a buffer of its own again.
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession(0x1, DEVADDR, nwkskey, appskey);
#else
  // If not running an AVR with PROGMEM, just use the arrays directly
  LMIC_setSession(0x1, DEVADDR, NWKSKEY, APPSKEY);
#endif

  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI); // g-band
  //LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI); // g-band
  //LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7), BAND_CENTI);  // g-band
  //LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK), BAND_MILLI);   // g2-band
  LMIC_setLinkCheckMode(0);

  LMIC.dn2Dr = DR_SF9;
  LMIC_setClockError(MAX_CLOCK_ERROR * 5 / 100);
  forceTxSingleChannelDr();
  // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
  LMIC_setDrTxpow(DR_SF7, 14);
}

void LoRaSend(char *res)
{
  char **params = (char **)malloc(164);
  eil.getParameters(&res, params);
  char *post_data = (char *)malloc(512);
  sprintf(post_data, "{\"value\":\"%s\"}", eil.getVariable(params[0]));

  //uint8_t mydata[strlen(post_data)];
  //data_str.getBytes(mydata, strlen(post_data));

  //Serial.println(data_str);
  if (LMIC.opmode & OP_TXRXPEND)
  {
    Serial.println(F("OP_TXRXPEND, not sending"));
    LoRaSetup();
    LoRaSend(res);
  }
  else
  {
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, (uint8_t *)post_data, strlen(post_data), 0);
    //Serial.println(F("Packet queued"));
  }

  free(post_data);
  free(params);
}

void downloadFirmware(void *params)
{
  esp_http_client_config_t config = {};
  config.url = "http://iotdev.htlwy.ac.at/thing/iotusecases2020/getThingscript?value=\"sensornode01\"";
  config.event_handler = _http_event_handle;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Content-Length", "*");
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGE(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  esp_http_client_cleanup(client);
  vTaskSuspend(DownloadFirmwareTask);
}

const char *getScript()
{
  FILE *f = fopen("/spiffs/script.txt", "r");
  if (f == NULL)
    Serial.println("Failed to open file for reading");

  int prev = ftell(f);
  fseek(f, 0L, SEEK_END);
  int size = ftell(f);
  fseek(f, prev, SEEK_SET);
  char line[size];
  int c = getc(f);
  uint16_t index = 0;
  while (c != EOF)
  {
    line[index] = (char)c;
    index++;
    c = getc(f);
  }
  line[index] = '\0';
  fclose(f);
  char *retStr = (char *)malloc(size);
  strcpy(retStr, line);
  return retStr;
}

void setScript(const char *script)
{
  fclose(fopen("/spiffs/script.txt", "w"));
  FILE *f = fopen("/spiffs/script.txt", "w");
  if (f == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file for writing");
  }
  fprintf(f, "%s", script);
  fclose(f);

  ESP.restart();
}

const char *getInit(const char *jsonString)
{
  const size_t capacity = JSON_OBJECT_SIZE(2) + 80;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(jsonString);
  return root["init"];
}

const char *getLoop(const char *jsonString)
{
  const size_t capacity = JSON_OBJECT_SIZE(2) + 80;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(jsonString);
  return root["loop"];
}

void serialCheck()
{
  while (Serial.available())
  {
    //Serial.println("Receiving.");
    bufferdInt = Serial.read();
    output += char(bufferdInt);
    if (bufferdInt == 10 || bufferdInt == 0)
    {
      Serial.println(output);
      setScript(output.c_str());
    }
  }
}

static esp_err_t echo_handler(httpd_req_t *req)
{
  if (strcmp(req->uri, "/setSettings") == 0)
  {
    const char resp[] = "Applying new settings.";
    httpd_resp_send(req, resp, sizeof(resp));
    char content[req->content_len];
    httpd_req_recv(req, content, req->content_len);

    fclose(fopen("/spiffs/settings.json", "w"));
    FILE *f = fopen("/spiffs/settings.json", "w");
    content[req->content_len] = '\0';
    fprintf(f, content);
    fclose(f);
  }
  else if (strcmp(req->uri, "/getSettings") == 0)
  {
    FILE *f = fopen("/spiffs/settings.json", "r");
    if (f == NULL)
    {
      ESP_LOGE(TAG, "Failed to open file for reading");
    }
    int prev = ftell(f);
    fseek(f, 0L, SEEK_END);
    int size = ftell(f);
    fseek(f, prev, SEEK_SET);
    char line[size];
    int c = getc(f);
    uint16_t index = 0;
    while (c != EOF)
    {
      line[index] = (char)c;
      index++;
      c = getc(f);
    }
    fclose(f);
    //Serial.print("Read from file: ");
    //Serial.println(line);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); //SUPER DUPER ULTRA MEGA EXTRA WICHTIG
    httpd_resp_send(req, line, sizeof(line));
  }
  else if (strcmp(req->uri, "/setCode") == 0)
  {
    char content[req->content_len];
    httpd_req_recv(req, content, req->content_len);
    setScript(content);
  }
  else if (strcmp(req->uri, "/getCode") == 0)
  {
    char content[req->content_len];
    httpd_req_recv(req, content, req->content_len);
    FILE *f = fopen("/spiffs/script.txt", "r");
    if (f == NULL)
    {
      ESP_LOGE(TAG, "Failed to open file for reading");
    }
    int prev = ftell(f);
    fseek(f, 0L, SEEK_END);
    int size = ftell(f);
    fseek(f, prev, SEEK_SET);
    char line[size];
    int c = getc(f);
    uint16_t index = 0;
    while (c != EOF)
    {
      line[index] = (char)c;
      index++;
      c = getc(f);
    }
    fclose(f);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); //SUPER DUPER ULTRA MEGA EXTRA WICHTIG
    httpd_resp_send(req, line, sizeof(line));
  }
  return ESP_OK;
}

static const httpd_uri_t http_setSettings = {
    .uri = "/setSettings",
    .method = HTTP_POST,
    .handler = echo_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t http_getSettings = {
    .uri = "/getSettings",
    .method = HTTP_GET,
    .handler = echo_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t http_getCode = {
    .uri = "/getCode",
    .method = HTTP_GET,
    .handler = echo_handler,
    .user_ctx = NULL,

};

static const httpd_uri_t http_setCode = {
    .uri = "/setCode",
    .method = HTTP_POST,
    .handler = echo_handler,
    .user_ctx = NULL,
};

void initialize_HTTP_Server()
{
  //webSocket.begin();
  //webSocket.onEvent(webSocketEvent);

  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_resp_headers = 1024;

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &http_setSettings);
    httpd_register_uri_handler(server, &http_getSettings);
    httpd_register_uri_handler(server, &http_getCode);
    httpd_register_uri_handler(server, &http_setCode);
  }
}

void initSPIFFS()
{
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true};

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    }
    else if (ret == ESP_ERR_NOT_FOUND)
    {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
}

void initFirmware()
{
  Serial.println("Initializing Hardware.");
  sensor.initSensors();
  initSPIFFS();
  LoRaSetup();
}

void upgradeScipt()
{
  char *data = (char *)malloc(32);
  strcpy(data, "[\"2.4\",\"tenerife\"]");
  WiFiConnect(data);
  free(data);
}

void checkForUpgradeRequest()
{
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = (1ULL << GPIO_NUM_36);
  io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
  //io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
  if (gpio_get_level(GPIO_NUM_36) == 1)
  {
    Serial.println("Requesting Update.");
    upgradeRequest = true;
    upgradeScipt();
  }
}

void createEILTask()
{
  xTaskCreatePinnedToCore(
      EILTaskPinnedToCore1,   /* Funktion, die ausgeführt wird */
      "EILTaskPinnedToCore1", /* Taskname */
      100000,                 /* Stapelspeichergröße */
      NULL,                   /* Übergabeparameter */
      1,                      /* Task Priorität (1 da nur eine Task rennt) */
      &EILTask,               /* Adresse der EILTask für Task-Kontrolle */
      1                       /* Task rennt auf Core 1 = 2. Kern */
  );
}

//ESP.wdtDisable();

void setup()
{
  wifi_event_group = xEventGroupCreate();
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  Serial.begin(115200);
  Serial.println("************************************");
  Serial.println("*                                  *");
  Serial.println("*     Sensor Node Firmware V1.0    *");
  Serial.println("*    Developed by Sebastian Tatar  *");
  Serial.println("*      IoT UseCases 2020/2021      *");
  Serial.println("*                                  *");
  Serial.println("************************************");
  Serial.println("");
  initFirmware();
  checkForUpgradeRequest();

  eil.initVM();
  eil.registerFunction("Delay", 0xA0, &delay_);
  eil.registerFunction("Print", 0xA1, &PRINT);
  eil.registerFunction("Println", 0xA2, &PRINTLN);
  eil.registerFunction("WiFiConnect", 0xA3, &WiFiConnect);
  eil.registerFunction("HttpGet", 0xA4, &HttpGet);
  eil.registerFunction("HttpPost", 0xA5, &HttpPost);
  eil.registerFunction("LoRaSend", 0xA9, &LoRaSend);
  eil.registerFunction("Sleep", 0xAA, &goInDeepSleep);
  eil.registerVariable("%IO26", &getInputOutput26);
  eil.registerVariable("%IO27", &getInputOutput27);
  eil.registerVariable("%IO25", &getOutput25);
  eil.registerVariable("%IADC0", &getADC_CH0);
  eil.registerVariable("%TEMP", &temp);
  eil.registerVariable("%PRES", &pres);
  eil.registerVariable("%AX", &ax);
  eil.registerVariable("%AY", &ay);
  eil.registerVariable("%AZ", &az);
  eil.registerVariable("%TOFM", &ToFm);
  eil.registerVariable("%TOFCM", &ToFcm);
  eil.registerVariable("%TOFMM", &ToFmm);
  eil.registerVariable("HIGH", &_high_);
  eil.registerVariable("LOW", &_low_);

  if (!upgradeRequest)
    createEILTask(); /* pin task to core 0 */
}

void EILTaskPinnedToCore1(void *params)
{
  Serial.println(getInit(getScript()));
  Serial.println(getLoop(getScript()));
  eil.insertScript(getInit(getScript()));
  eil.handleVM();
  eil.insertScript(getLoop(getScript()));

  for (;;)
  {
    eil.handleVM();
  }
}

void loop()
{
  serialCheck();
}
