#ifndef AXP_PROT_H
#define AXP_PROT_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void axp_i2c_prot_init(void);
void axp_cmd_init(void);
//void axp_basic_sleep_start(void);
//void state_axp2101_task(void *arg);
void axp2101_isCharging_task(void *arg);

/* Charger state machine as reported by the AXP2101 status register. */
typedef enum {
    AXP_CHARGE_STATE_TRICKLE = 0,
    AXP_CHARGE_STATE_PRE,
    AXP_CHARGE_STATE_CC,
    AXP_CHARGE_STATE_CV,
    AXP_CHARGE_STATE_DONE,
    AXP_CHARGE_STATE_STOP,
    AXP_CHARGE_STATE_UNKNOWN,
} axp_charge_state_t;

typedef struct {
    bool battery_connected;
    /* Fuel gauge reading, 0-100. -1 when no battery is present or the gauge
       returned a value outside the valid range. */
    int percent;
    /* Battery voltage in mV, or -1 when unavailable. */
    int voltage_mv;
    bool charging;
    bool vbus_present;
    axp_charge_state_t charge_state;
} axp_power_status_t;

/**
 * Bring up the I2C master and the AXP2101, apply the project power settings and
 * enable the fuel gauge and cell battery charging. Safe to call more than once;
 * the work is done on the first successful call.
 */
esp_err_t axp_power_init(void);

/** True once axp_power_init() has succeeded. */
bool axp_power_ready(void);

/**
 * Read the current battery and charger state. Returns ESP_ERR_INVALID_STATE
 * when the PMIC has not been initialized.
 */
esp_err_t axp_power_read_status(axp_power_status_t *out);

const char *axp_charge_state_str(axp_charge_state_t state);

#ifdef __cplusplus
}
#endif

#endif
