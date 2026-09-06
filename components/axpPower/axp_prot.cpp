#include "axp_prot.h"
#include "XPowersLib.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "i2c_bsp.h"
#include <stdio.h>
#include <string.h>

const char *TAG = "axp2101";

static XPowersPMU axp2101;

static int AXP2101_SLAVE_Read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
    int ret;
    uint8_t count = 3;
    do
    {
        ret = (i2c_read_buff(axp2101_dev_handle, regAddr, data, len) == ESP_OK) ? 0 : -1;
        if (ret == 0)
            break;
        vTaskDelay(pdMS_TO_TICKS(100));
        count--;
    } while (count);
    return ret;
}

static int AXP2101_SLAVE_Write(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len) {
    int ret;
    uint8_t count = 3;
    do
    {
        ret = (i2c_write_buff(axp2101_dev_handle, regAddr, data, len) == ESP_OK) ? 0 : -1;
        if (ret == 0)
            break;
        vTaskDelay(pdMS_TO_TICKS(100));
        count--;
    } while (count);
    return ret;
}

void axp_i2c_prot_init(void) {
    if (axp2101.begin(AXP2101_SLAVE_ADDRESS, AXP2101_SLAVE_Read, AXP2101_SLAVE_Write)) {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    } else {
        ESP_LOGE(TAG, "Init PMU FAILED!");
    }
}

void axp_cmd_init(void) {
    ///* Set up the startup battery temperature management. disableTSPinMeasure() - Disable battery temperature measurement. */
    axp2101.disableTSPinMeasure();
    int data = axp2101.readRegister(0x26);
    ESP_LOGW("axp2101_init_log","reg_26:0x%02x",data);
    if(data & 0x01) {
        axp2101.enableWakeup();
        ESP_LOGW("axp2101_init_log","i2c_wakeup");
    }
    if(data & 0x08) {
        axp2101.wakeupControl(XPOWERS_AXP2101_WAKEUP_PWROK_TO_LOW, false);
        ESP_LOGW("axp2101_init_log","When setting the wake-up operation, pwrok does not need to be pulled down.");
    }
    if(axp2101.getPowerKeyPressOffTime() != XPOWERS_POWEROFF_4S) {
        axp2101.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
        ESP_LOGW("axp2101_init_log","Press and hold the pwr button for 4 seconds to shut down the device.");
    }
    if(axp2101.getPowerKeyPressOnTime() != XPOWERS_POWERON_128MS) {
        axp2101.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
        ESP_LOGW("axp2101_init_log","Click PWR to turn on the device.");
    }
    if(axp2101.getChargingLedMode() != XPOWERS_CHG_LED_OFF) {
        axp2101.setChargingLedMode(XPOWERS_CHG_LED_OFF);
        ESP_LOGW("axp2101_init_log","Disable the CHGLED function.");
    }
    if(axp2101.getChargeTargetVoltage() != XPOWERS_AXP2101_CHG_VOL_4V1) {
        axp2101.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);
        ESP_LOGW("axp2101_init_log","Set the full charge voltage of the battery to 4.1V.");
    }
    if(axp2101.getButtonBatteryVoltage() != 3300) {
        axp2101.setButtonBatteryChargeVoltage(3300);
        ESP_LOGW("axp2101_init_log","Set Button Battery charge voltage");
    }
    if(axp2101.isEnableButtonBatteryCharge() == 0) {
        axp2101.enableButtonBatteryCharge();
        ESP_LOGW("axp2101_init_log","Enable Button Battery charge");
    }
    if(axp2101.getDC1Voltage() != 3300) {
        axp2101.setDC1Voltage(3300);
        ESP_LOGW("axp2101_init_log","Set DCDC1 to output 3V3");
    }
    if(axp2101.getALDO3Voltage() != 3300) {
        axp2101.setALDO3Voltage(3300);
        ESP_LOGW("axp2101_init_log","Set ALDO3 to output 3V3");
    }
    if(axp2101.getALDO4Voltage() != 3300) {
        axp2101.setALDO4Voltage(3300);
        ESP_LOGW("axp2101_init_log","Set ALDO4 to output 3V3");
    }
}

void axp_basic_sleep_start(void) {
    /*Disable interrupts and clear interrupt flag bits*/
    axp2101.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    axp2101.clearIrqStatus();
    /*Log output*/
    int power_value = axp2101.readRegister(0x26);
    ESP_LOGW("axp2101_log","reg_26:0x%02x",power_value);
    /*The power setting after waking up is the same as that before going to sleep.*/
    if(!(power_value & 0x04)) {
        axp2101.wakeupControl(XPOWERS_AXP2101_WAKEUP_DC_DLO_SELECT, true);
        ESP_LOGW("axp2101_log","The power setting after waking up is the same as that before going to sleep.");
    }
    /*When setting the wake-up operation, pwrok does not need to be pulled down.*/
    if((power_value & 0x08)) {
        axp2101.wakeupControl(XPOWERS_AXP2101_WAKEUP_PWROK_TO_LOW, false);
        ESP_LOGW("axp2101_log","When setting the wake-up operation, pwrok does not need to be pulled down.");
    }
    /*Set the wake-up source, the interrupt pin of axp2101*/
    if(!(power_value & 0x10)) {
        axp2101.wakeupControl(XPOWERS_AXP2101_WAKEUP_IRQ_PIN_TO_LOW, true);
        ESP_LOGW("axp2101_log","Set the wake-up source, the interrupt pin of axp2101");
    }
    /*Enable entering sleep mode*/
    axp2101.enableSleep();
    /*Log output*/
    power_value = axp2101.readRegister(0x26);
    ESP_LOGW("axp2101_log","reg_26:0x%02x",power_value);

    /*Disable the relevant power supply*/
    axp2101.disableDC2();
    axp2101.disableDC3();
    axp2101.disableDC4();
    axp2101.disableDC5();
    axp2101.disableALDO1();
    axp2101.disableALDO2();
    axp2101.disableBLDO1();
    axp2101.disableBLDO2();
    axp2101.disableCPUSLDO();
    axp2101.disableDLDO1();
    axp2101.disableDLDO2();
    axp2101.disableALDO4();
    axp2101.disableALDO3();
}

void state_axp2101_task(void *arg) {
    for (;;) {
        // ESP_LOGI(TAG, "Power Temperature: %.2f°C", axp2101.getTemperature());

        ESP_LOGI(TAG, "isCharging: %s", axp2101.isCharging() ? "YES" : "NO");

        ESP_LOGI(TAG, "isDischarge: %s", axp2101.isDischarge() ? "YES" : "NO");

        ESP_LOGI(TAG, "isStandby: %s", axp2101.isStandby() ? "YES" : "NO");

        ESP_LOGI(TAG, "isVbusIn: %s", axp2101.isVbusIn() ? "YES" : "NO");

        ESP_LOGI(TAG, "isVbusGood: %s", axp2101.isVbusGood() ? "YES" : "NO");

        uint8_t charge_status = axp2101.getChargerStatus();
        if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE) {
            ESP_LOGI(TAG, "Charger Status: tri_charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE) {
            ESP_LOGI(TAG, "Charger Status: pre_charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE) {
            ESP_LOGI(TAG, "Charger Status: constant charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE) {
            ESP_LOGI(TAG, "Charger Status: constant voltage");
        } else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE) {
            ESP_LOGI(TAG, "Charger Status: charge done");
        } else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE) {
            ESP_LOGI(TAG, "Charger Status: not charge");
        }

        ESP_LOGI(TAG, "getBattVoltage: %d mV", axp2101.getBattVoltage());

        ESP_LOGI(TAG, "getVbusVoltage: %d mV", axp2101.getVbusVoltage());

        ESP_LOGI(TAG, "getSystemVoltage: %d mV", axp2101.getSystemVoltage());

        if (axp2101.isBatteryConnect()) {
            ESP_LOGI(TAG, "getBatteryPercent: %d %%", axp2101.getBatteryPercent());
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "\n\n");
    }
}

void axp2101_isCharging_task(void *arg) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20000));
        ESP_LOGI(TAG, "isCharging: %s", axp2101.isCharging() ? "YES" : "NO");
        uint8_t charge_status = axp2101.getChargerStatus();
        if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE) {
            ESP_LOGI(TAG, "Charger Status: tri_charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE) {
            ESP_LOGI(TAG, "Charger Status: pre_charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE) {
            ESP_LOGI(TAG, "Charger Status: constant charge");
        } else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE) {
            ESP_LOGI(TAG, "Charger Status: constant voltage");
        } else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE) {
            ESP_LOGI(TAG, "Charger Status: charge done");
        } else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE) {
            ESP_LOGI(TAG, "Charger Status: not charge");
        }
        ESP_LOGI(TAG, "getBattVoltage: %d mV", axp2101.getBattVoltage());
    }
}

/* ------------------------------------------------------------------------- */
/* Battery and charger status                                                 */
/* ------------------------------------------------------------------------- */

static bool s_power_ready = false;

const char *axp_charge_state_str(axp_charge_state_t state) {
    switch (state) {
    case AXP_CHARGE_STATE_TRICKLE: return "trickle";
    case AXP_CHARGE_STATE_PRE:     return "pre";
    case AXP_CHARGE_STATE_CC:      return "constant_current";
    case AXP_CHARGE_STATE_CV:      return "constant_voltage";
    case AXP_CHARGE_STATE_DONE:    return "done";
    case AXP_CHARGE_STATE_STOP:    return "stopped";
    default:                       return "unknown";
    }
}

static axp_charge_state_t charge_state_from_register(uint8_t status) {
    switch (status) {
    case XPOWERS_AXP2101_CHG_TRI_STATE:  return AXP_CHARGE_STATE_TRICKLE;
    case XPOWERS_AXP2101_CHG_PRE_STATE:  return AXP_CHARGE_STATE_PRE;
    case XPOWERS_AXP2101_CHG_CC_STATE:   return AXP_CHARGE_STATE_CC;
    case XPOWERS_AXP2101_CHG_CV_STATE:   return AXP_CHARGE_STATE_CV;
    case XPOWERS_AXP2101_CHG_DONE_STATE: return AXP_CHARGE_STATE_DONE;
    case XPOWERS_AXP2101_CHG_STOP_STATE: return AXP_CHARGE_STATE_STOP;
    default:                             return AXP_CHARGE_STATE_UNKNOWN;
    }
}

bool axp_power_ready(void) {
    return s_power_ready;
}

esp_err_t axp_power_init(void) {
    if (s_power_ready) {
        return ESP_OK;
    }
    i2c_master_Init();
    if (!axp2101.begin(AXP2101_SLAVE_ADDRESS, AXP2101_SLAVE_Read, AXP2101_SLAVE_Write)) {
        ESP_LOGE(TAG, "PMU not responding on I2C; battery state stays unknown");
        return ESP_ERR_NOT_FOUND;
    }
    axp_cmd_init();

    /* Report the charger configuration as found before this boot changes it, so
       a board that ships with charging disabled is visible in the log. */
    ESP_LOGI(TAG, "charger before init: gauge=%d cell_charge=%d curr_step=%u target_mv_opt=%u",
             (int)axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 3),
             (int)axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 1),
             (unsigned)axp2101.getChargerConstantCurr(),
             (unsigned)axp2101.getChargeTargetVoltage());

    /* getBatteryPercent() reads the gauge register, which is meaningless while
       the gauge is off, and the cell battery charger has its own enable bit
       separate from the button battery one set in axp_cmd_init(). Both are
       already set on the PhotoPainter board, so follow axp_cmd_init() and only
       write when the bit is actually clear: a redundant read-modify-write on
       0x18 costs an I2C transaction on every wake and has been observed to
       NACK and retry. */
    if (!axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 3)) {
        axp2101.enableGauge();
        ESP_LOGW(TAG, "fuel gauge was disabled; enabled it");
    }
    if (!axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 1)) {
        axp2101.enableCellbatteryCharge();
        ESP_LOGW(TAG, "cell battery charging was disabled; enabled it");
    }

    ESP_LOGI(TAG, "charger after init: gauge=%d cell_charge=%d",
             (int)axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 3),
             (int)axp2101.getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 1));

    s_power_ready = true;
    return ESP_OK;
}

esp_err_t axp_power_read_status(axp_power_status_t *out) {
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->percent = -1;
    out->voltage_mv = -1;
    out->charge_state = AXP_CHARGE_STATE_UNKNOWN;
    if (!s_power_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    out->battery_connected = axp2101.isBatteryConnect();
    out->charging = axp2101.isCharging();
    out->vbus_present = axp2101.isVbusIn();
    out->charge_state = charge_state_from_register(axp2101.getChargerStatus());

    if (out->battery_connected) {
        uint16_t millivolt = axp2101.getBattVoltage();
        if (millivolt > 0) {
            out->voltage_mv = (int)millivolt;
        }
        /* The gauge register reads 0xFF while it is still settling after a cold
           start; anything outside 0-100 is reported as unknown rather than
           clamped, so HA shows `unknown` instead of a made-up level. */
        int percent = axp2101.getBatteryPercent();
        if (percent >= 0 && percent <= 100) {
            out->percent = percent;
        }
    }
    return ESP_OK;
}
