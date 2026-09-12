/* Compile the real manager against a small synchronous ESP-IDF test double.
 * This catches the second-cycle abort and dangling handlers without hardware.
 * It does not model FreeRTOS task races; those require the on-device checks.
 */
#include "fake_idf.h"
#include "wifi_manager.h"
#include <stdio.h>

struct fake_handler { esp_event_base_t base; handler_t callback; };
static struct fake_handler *handlers[8];
static int group_count, netif_count, default_handlers, handler_count;
static int connect_calls, init_step, fail_step;
static bool loop_exists, driver_live, sntp_live;
static void (*sync_callback)(struct timeval *);

static esp_err_t step(void) { return ++init_step == fail_step ? ESP_FAIL : ESP_OK; }
static void emit(esp_event_base_t base, int32_t id)
{
    ip_event_got_ip_t event = {0};
    for (int i = 0; i < 8; ++i) {
        if (handlers[i] && handlers[i]->base == base) {
            handlers[i]->callback(NULL, base, id, &event);
        }
    }
}
EventGroupHandle_t xEventGroupCreate(void)
{
    if (step() != ESP_OK) return NULL;
    ++group_count;
    return calloc(1, sizeof(unsigned));
}
void vEventGroupDelete(EventGroupHandle_t group)
{
    assert(handler_count == 0 && !sntp_live);
    --group_count;
    free(group);
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t b) { assert(g); return *g |= b; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t b) { assert(g); return *g &= ~b; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t b, int clear, int all, unsigned timeout)
{ (void)b; (void)clear; (void)all; (void)timeout; return *g; }
esp_err_t nvs_flash_init(void) { return step(); }
esp_err_t esp_netif_init(void) { return step(); }
esp_err_t esp_event_loop_create_default(void)
{
    if (step() != ESP_OK) return ESP_FAIL;
    if (loop_exists) return ESP_ERR_INVALID_STATE;
    loop_exists = true;
    return ESP_OK;
}
esp_netif_t *esp_netif_create_default_wifi_sta(void)
{
    if (step() != ESP_OK) return NULL;
    ++netif_count;
    ++default_handlers;
    return calloc(1, sizeof(esp_netif_t));
}
void esp_netif_destroy(void *netif) { --netif_count; free(netif); }
void esp_netif_destroy_default_wifi(void *netif) { --default_handlers; esp_netif_destroy(netif); }
esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{ (void)config; if (step() != ESP_OK) return ESP_FAIL; assert(!driver_live); driver_live = true; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { assert(driver_live); driver_live = false; return ESP_OK; }
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, handler_t cb, void *arg, esp_event_handler_instance_t *out)
{
    (void)id; (void)arg;
    if (step() != ESP_OK) return ESP_FAIL;
    for (int i = 0; i < 8; ++i) {
        if (!handlers[i]) {
            handlers[i] = malloc(sizeof(*handlers[i]));
            handlers[i]->base = base;
            handlers[i]->callback = cb;
            ++handler_count;
            if (out) *out = handlers[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id, esp_event_handler_instance_t instance)
{
    (void)base; (void)id;
    for (int i = 0; i < 8; ++i) {
        if (handlers[i] == instance) { free(handlers[i]); handlers[i] = NULL; --handler_count; return ESP_OK; }
    }
    assert(false);
    return ESP_FAIL;
}
esp_err_t esp_wifi_set_mode(int mode) { (void)mode; return step(); }
esp_err_t esp_wifi_set_config(int iface, const wifi_config_t *config) { (void)iface; (void)config; return step(); }
esp_err_t esp_wifi_start(void)
{ if (step() != ESP_OK) return ESP_FAIL; emit(WIFI_EVENT, WIFI_EVENT_STA_START); emit(IP_EVENT, IP_EVENT_STA_GOT_IP); return ESP_OK; }
esp_err_t esp_wifi_stop(void) { return ESP_OK; }
esp_err_t esp_wifi_connect(void) { ++connect_calls; return ESP_OK; }
esp_err_t esp_wifi_disconnect(void) { emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED); return ESP_OK; }
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *record) { record->rssi = -50; return ESP_OK; }
void esp_sntp_setoperatingmode(int mode) { (void)mode; }
void esp_sntp_setservername(int index, const char *name) { (void)index; (void)name; }
void esp_sntp_set_time_sync_notification_cb(void (*cb)(struct timeval *)) { sync_callback = cb; }
void esp_sntp_init(void)
{
    assert(!sntp_live);
    sntp_live = true;
    struct timeval now = {1800000000, 0};
    sync_callback(&now); /* Deliberately respond before init returns. */
}
void esp_sntp_stop(void) { assert(sntp_live); sntp_live = false; }
bool esp_sntp_enabled(void) { return sntp_live; }

static void assert_clean(void)
{
    assert(group_count == 0 && netif_count == 0 && default_handlers == 0);
    assert(handler_count == 0 && !driver_live && !sntp_live);
}
static void cycle(void)
{
    init_step = 0;
    fail_step = 0;
    assert(wifi_manager_init() == ESP_OK);
    assert(wifi_manager_init() == ESP_OK); /* Idempotent while initialized. */
    assert(handler_count == 2);
    assert(wifi_manager_connect(100) == ESP_OK);
    assert(wifi_manager_sync_time(100) == ESP_OK);
    int calls = connect_calls;
    emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    assert(connect_calls == calls + 1); /* Unexpected disconnect still retries. */
    calls = connect_calls;
    assert(wifi_manager_disconnect() == ESP_OK);
    assert(connect_calls == calls); /* Intentional disconnect must not retry. */
    wifi_manager_deinit();
    wifi_manager_deinit();
    assert_clean();
}
int main(void)
{
#ifdef _WIN32
    /* Failed assertions must terminate unattended runs without a dialog. */
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    for (int i = 0; i < 20; ++i) cycle();
    /* Inject an error at every allocation/registration/configuration stage,
     * then prove that the next full cycle succeeds without leaked resources. */
    for (int i = 1; i <= 10; ++i) {
        init_step = 0;
        fail_step = i;
        assert(wifi_manager_init() != ESP_OK);
        assert_clean();
        cycle();
    }
    init_step = 0;
    fail_step = 0;
    assert(wifi_manager_init() == ESP_OK);
    fail_step = init_step + 1;
    assert(wifi_manager_connect(100) == ESP_FAIL);
    wifi_manager_deinit();
    assert_clean();
    cycle();
    puts("WiFi lifecycle: repeated cycles, partial failures, disconnect and SNTP checks passed");
    return 0;
}
