/*
 * ESP32 Sens peripheral
 *
 * Copyright (c) 2019 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/guest-random.h"
#include "qapi/error.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/misc/esp32_sens.h"
#include "hw/irq.h"

static uint64_t esp32_sens_read(void *opaque, hwaddr addr, unsigned int size)
{
    Esp32SensState *s = ESP32_SENS(opaque);
    uint32_t r = 0;
    
    switch(addr) {
    case A_SENS_SAR_MEAS_START1:
        r = 0x10000+2800+rand()%4;
        break;
    case A_SENS_SAR_I2C_CTRL:
        r = s->i2c_ctrl | (1<<8);
        break;
    case A_SENS_SAR_TOUCH_CTRL2:
        r = (1<<10);
        break;
    case A_SENS_SAR_START_FORCE:
        r = s->sar_start_force;
        break;
    case A_SENS_SAR_TOUCH_OUT1 ... A_SENS_SAR_TOUCH_OUT1+4*4:
        int n1=((addr-A_SENS_SAR_TOUCH_OUT1)/4)*2;
        r = ((1500-s->touch_sensor[n1]+rand()%20)<<16) | (1500-s->touch_sensor[n1+1]+rand()%20);
        break;
    case A_SENS_ULP_CP_SLEEP_CYC0 ... A_SENS_ULP_CP_SLEEP_CYC0+4*4:
        r = s->ulp_sleep_cyc[(addr-A_SENS_ULP_CP_SLEEP_CYC0)/4];
        break;
    }
  //  printf("esp32_sens_read %lx=%x\n",addr,r);
    return r;
}

static void esp32_sens_write(void *opaque, hwaddr addr, uint64_t value,
                                 unsigned int size) {
    Esp32SensState *s = ESP32_SENS(opaque);
//    printf("esp32_sens_write %lx=%lx\n",addr,value);
    switch(addr) {
    case A_SENS_SAR_I2C_CTRL:
        s->i2c_ctrl=(uint32_t)value;
        break;
    case A_SENS_ULP_CP_SLEEP_CYC0 ... A_SENS_ULP_CP_SLEEP_CYC0+4*4:
        s->ulp_sleep_cyc[(addr-A_SENS_ULP_CP_SLEEP_CYC0)/4]=(uint32_t)value;
        break;
    case A_SENS_SAR_START_FORCE:
        s->sar_start_force=value;
        qemu_set_irq(s->start_ulp,value);
        if(value==0)
            s->ulp_sleep_cyc[0]=200;
        break;
    }
}

static const MemoryRegionOps esp32_sens_ops = {
    .read =  esp32_sens_read,
    .write = esp32_sens_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void esp32_sens_init(Object *obj)
{
    Esp32SensState *s = ESP32_SENS(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &esp32_sens_ops, s,
                          TYPE_ESP32_SENS, 0x400);
    s->ulp_sleep_cyc[0]=200;
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->start_ulp, ESP32_START_ULP_GPIO, 1);
}


static const TypeInfo esp32_sens_info = {
    .name = TYPE_ESP32_SENS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Esp32SensState),
    .instance_init = esp32_sens_init,
};

static void esp32_sens_register_types(void)
{
    type_register_static(&esp32_sens_info);
}

type_init(esp32_sens_register_types)
