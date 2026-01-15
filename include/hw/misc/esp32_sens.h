#pragma once

#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/registerfields.h"
//#include "hw/misc/esp32_reg.h"


#define TYPE_ESP32_SENS "misc.esp32.sens"
#define ESP32_SENS(obj) OBJECT_CHECK(Esp32SensState, (obj), TYPE_ESP32_SENS)

#define ESP32_START_ULP_GPIO "start_ulp"

typedef struct Esp32SensState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    uint32_t touch_sensor[14];
    uint32_t ulp_sleep_cyc[5];
    qemu_irq start_ulp;
    uint32_t sar_start_force;
    uint32_t i2c_ctrl;
} Esp32SensState;
REG32(SENS_ULP_CP_SLEEP_CYC0,0x18)
REG32(SENS_SAR_I2C_CTRL,0x50)
REG32(SENS_SAR_MEAS_START1,0x54)
REG32(SENS_SAR_TOUCH_CTRL2,0x84)
REG32(SENS_SAR_START_FORCE, 0x2c)
    FIELD(SENS_SAR_START_FORCE,ULP_FORCE_START,8,1)
    FIELD(SENS_SAR_START_FORCE,ULP_START,9,1)
    FIELD(SENS_SAR_START_FORCE,ULP_PC,11,11)
REG32(SENS_SAR_TOUCH_OUT1,0x70)

