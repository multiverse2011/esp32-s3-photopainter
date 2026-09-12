#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#define setenv(name, value, overwrite) _putenv_s(name, value)
#define tzset _tzset
static inline struct tm *localtime_r(const time_t *clock, struct tm *result)
{ return localtime_s(result, clock) == 0 ? result : NULL; }
struct timeval { time_t tv_sec; long tv_usec; };
#else
#include <sys/time.h>
#endif
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_WIFI_NOT_CONNECT 0x300f
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define BIT0 1
#define BIT1 2
#define BIT2 4
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
typedef unsigned EventBits_t;
typedef unsigned *EventGroupHandle_t;
typedef int esp_netif_t;
typedef int esp_event_base_t;
typedef void (*handler_t)(void *, esp_event_base_t, int32_t, void *);
typedef struct fake_handler *esp_event_handler_instance_t;
#define WIFI_EVENT 1
#define IP_EVENT 2
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT_STA_START 1
#define WIFI_EVENT_STA_DISCONNECTED 2
#define IP_EVENT_STA_GOT_IP 3
#define WIFI_MODE_STA 1
#define WIFI_IF_STA 1
#define WIFI_AUTH_WPA2_PSK 1
#define CONFIG_WIFI_SSID "test"
#define CONFIG_WIFI_PASSWORD "test-password"
#define CONFIG_TIMEZONE "UTC0"
#define IPSTR "ip"
#define IP2STR(x) 0
#define SNTP_OPMODE_POLL 0
#define WIFI_INIT_CONFIG_DEFAULT() {0}
typedef struct { int unused; } wifi_init_config_t;
typedef struct { struct { char ssid[32]; char password[64]; struct { int authmode; } threshold; } sta; } wifi_config_t;
typedef struct { int rssi; } wifi_ap_record_t;
typedef struct { struct { int ip; } ip_info; } ip_event_got_ip_t;
EventGroupHandle_t xEventGroupCreate(void);
void vEventGroupDelete(EventGroupHandle_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, int, int, unsigned);
esp_err_t nvs_flash_init(void);
esp_err_t esp_netif_init(void);
esp_err_t esp_event_loop_create_default(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
void esp_netif_destroy_default_wifi(void *);
void esp_netif_destroy(void *);
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_event_handler_instance_register(esp_event_base_t, int32_t, handler_t, void *, esp_event_handler_instance_t *);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t, int32_t, esp_event_handler_instance_t);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_set_config(int, const wifi_config_t *);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *);
void esp_sntp_setoperatingmode(int);
void esp_sntp_setservername(int, const char *);
void esp_sntp_set_time_sync_notification_cb(void (*)(struct timeval *));
void esp_sntp_init(void);
void esp_sntp_stop(void);
bool esp_sntp_enabled(void);
