/*
 * ESP32 RTC_CNTL (RTC block controller) device
 *
 * Copyright (c) 2019-2024 Espressif Systems (Shanghai) Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/hw.h"
#include "hw/sysbus.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/misc/esp32s3_reg.h"
#include "hw/misc/esp32s3_rtc_cntl.h"
#include "sysemu/runstate.h"

static void esp32s3_rtc_update_cpu_stall(Esp32s3RtcCntlState* s);
static void esp32s3_rtc_update_clk(Esp32s3RtcCntlState* s);

#define DEBUG 0

static void sleep_timer_cb(void *opaque)
{
    Esp32s3RtcCntlState *s = (Esp32s3RtcCntlState*) opaque;
    qemu_set_irq(s->rtc_wakeup,RTC_ULP_TRIG_EN);
}

static uint64_t esp32s3_rtc_cntl_read(void *opaque, hwaddr addr, unsigned int size)
{
    Esp32s3RtcCntlState *s = ESP32S3_RTC_CNTL(opaque);
    uint64_t r = 0;
    switch (addr) {
    case A_RTC_CNTL_OPTIONS0:
        r = s->options0_reg;
        break;
    case A_RTC_CNTL_TIME_UPDATE:
        r = R_RTC_CNTL_TIME_UPDATE_VALID_MASK;
        break;
    case A_RTC_CNTL_TIME0:
        r = s->time_reg & UINT32_MAX;
        break;
    case A_RTC_CNTL_TIME1:
        r = s->time_reg >> 32;
        break;

    case A_RTC_CNTL_RESET_STATE:
        r = FIELD_DP32(r, RTC_CNTL_RESET_STATE, RESET_CAUSE_PROCPU, s->reset_cause[0]);
        r = FIELD_DP32(r, RTC_CNTL_RESET_STATE, RESET_CAUSE_APPCPU, s->reset_cause[1]);
        r = FIELD_DP32(r, RTC_CNTL_RESET_STATE, PROCPU_STAT_VECTOR_SEL, s->stat_vector_sel[0]);
        r = FIELD_DP32(r, RTC_CNTL_RESET_STATE, APPCPU_STAT_VECTOR_SEL, s->stat_vector_sel[1]);
        break;

    case A_RTC_CNTL_STORE0:
    case A_RTC_CNTL_STORE1:
    case A_RTC_CNTL_STORE2:
    case A_RTC_CNTL_STORE3:
        r = s->scratch_reg[(addr - A_RTC_CNTL_STORE0) / 4];
        break;

    case A_RTC_CNTL_CLK_CONF:
        r = FIELD_DP32(r, RTC_CNTL_CLK_CONF, FAST_CLK_RTC_SEL, s->rtc_fastclk);
        r = FIELD_DP32(r, RTC_CNTL_CLK_CONF, ANA_CLK_RTC_SEL, s->rtc_slowclk);
        break;

    case A_RTC_CNTL_SW_CPU_STALL:
        r = s->sw_cpu_stall_reg;
        break;

    case A_RTC_CNTL_STATE0:
        r = s->state0;
        break;

    case A_RTC_CNTL_SLEEP_TIMER0:
        r = s->sleep_timer0_reg;
        break;

    case A_RTC_CNTL_SLEEP_TIMER1:
        r = s->sleep_timer1_reg;
        break;

    case A_RTC_CNTL_STORE4:
    case A_RTC_CNTL_STORE5:
    case A_RTC_CNTL_STORE6:
    case A_RTC_CNTL_STORE7:
        r = s->scratch_reg[(addr - A_RTC_CNTL_STORE4) / 4 + 4];
        break;
    case A_RTC_CNTL_RTC_PWC_REG:
        r=0x925;
        break;
    case A_RTC_CNTL_RTC_LOW_POWER_ST:
        r = s->low_power_state_reg;
	    break;
    case A_RTC_CNTL_WAKEUP_STATE:
        r = s->wakeup_state_reg;
        break;
    case A_RTC_CNTL_EXT_WAKEUP1:
        r = s->ext_wakeup1;
        break;
    case A_RTC_CNTL_EXT_WAKEUP_CONF:
        r = s->wakeup_conf;
        break;
    case A_RTC_CNTL_INT_RAW:
        r = s->int_raw;
        break;
    case A_RTC_CNTL_INT_ENA:
        r = s->int_en;
        break;
    case A_RTC_CNTL_INT_ST:
        r = s->int_raw & s->int_en;
        break;
    case A_RTC_CNTL_SDIO_CONF:
        r = s->sdio_conf;
        break;
    case A_RTC_CNTL_DIG_PWC:
        r = s->dig_pwc;
        break;
    case A_RTC_CNTL_ULP_CP_TIMER:
        r = s->ulp_cp_timer;
        break;
    case A_RTC_CNTL_ULP_CP_TIMER1:
        r = s->ulp_cp_timer1;
        break;
    case A_RTC_CNTL_RTC_SLP_WAKEUP_CAUSE:
        r= s->wakeup_cause;
        break;
    }
    if(DEBUG)
        printf("esp32s3_rtc_cntl_read %x %x\n",(int)addr,(int)r);
    return r;
}

static void esp32s3_rtc_cntl_write(void *opaque, hwaddr addr, uint64_t value,
                                 unsigned int size)
{
    if(DEBUG)
        printf("esp32s3_rtc_cntl_write %x %x\n",(int)addr,(int)value);
    Esp32s3RtcCntlState *s = ESP32S3_RTC_CNTL(opaque);
    uint32_t old_slp_timer_en;

    switch (addr) {
    case A_RTC_CNTL_OPTIONS0:
        if (value & R_RTC_CNTL_OPTIONS0_SW_SYS_RESET_MASK) {
            s->reset_cause[0] = ESP32_SW_SYS_RESET;
            s->reset_cause[1] = ESP32_SW_SYS_RESET;
            qemu_irq_pulse(s->dig_reset_req);
            value &= ~(R_RTC_CNTL_OPTIONS0_SW_SYS_RESET_MASK);
        }
        if (value & R_RTC_CNTL_OPTIONS0_SW_APPCPU_RESET_MASK) {
            s->reset_cause[1] = ESP32_SW_CPU_RESET;
            qemu_irq_pulse(s->cpu_reset_req[1]);
            value &= ~(R_RTC_CNTL_OPTIONS0_SW_APPCPU_RESET_MASK);
        }
        if (value & R_RTC_CNTL_OPTIONS0_SW_PROCPU_RESET_MASK) {
            s->reset_cause[0] = ESP32_SW_CPU_RESET;
            qemu_irq_pulse(s->cpu_reset_req[0]);
            value &= ~(R_RTC_CNTL_OPTIONS0_SW_PROCPU_RESET_MASK);
        }
        s->options0_reg = value;
        esp32s3_rtc_update_cpu_stall(s);
        break;

    case A_RTC_CNTL_TIME_UPDATE:
        if (value & R_RTC_CNTL_TIME_UPDATE_UPDATE_MASK) {
            s->time_reg = muldiv64(
                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->time_base_ns,
                s->rtc_slowclk_freq, NANOSECONDS_PER_SECOND);
        }
        break;

    case A_RTC_CNTL_RESET_STATE:
        s->stat_vector_sel[0] = FIELD_EX32(value, RTC_CNTL_RESET_STATE,
                                           PROCPU_STAT_VECTOR_SEL);
        s->stat_vector_sel[1] = FIELD_EX32(value, RTC_CNTL_RESET_STATE,
                                           APPCPU_STAT_VECTOR_SEL);
        break;
    case A_RTC_CNTL_STATE0:
        s->state0 = value;
        if(FIELD_EX32(value, RTC_CNTL_STATE0,SLEEP_EN)) {
            s->int_raw = 1;
            uint64_t sleep_time=s->sleep_timer0_reg | ((uint64_t)FIELD_EX32(s->sleep_timer1_reg,RTC_CNTL_SLEEP_TIMER1,VAL_HI)<<32);
            int timer_en=FIELD_EX32(s->wakeup_state_reg, RTC_CNTL_WAKEUP_STATE, WAKEUP_ENA_RTC_TIMER);
            uint64_t sleep_ns=muldiv64(
                sleep_time - s->time_reg, NANOSECONDS_PER_SECOND,
                s->rtc_slowclk_freq);
            if(DEBUG)
                printf("Sleep %d, %d, %d, %d\n",(uint32_t)s->time_reg, (uint32_t)sleep_time, timer_en, (uint32_t)sleep_ns);
            if(timer_en)
                timer_mod(&s->sleep_timer, qemu_clock_get_ns(QEMU_CLOCK_REALTIME)+sleep_ns);
            s->low_power_state_reg=FIELD_DP32(s->low_power_state_reg,RTC_CNTL_LOW_POWER_ST, RTC_RDY_FOR_WAKEUP,1);
            qemu_system_suspend_request();
        }
      //      qemu_set_irq(s->enable_ulp_timer,FIELD_EX32(s->ulp_cp_timer,  RTC_CNTL_ULP_CP_TIMER,SLP_TIMER_EN));
        break;
    case A_RTC_CNTL_ULP_CP_TIMER:
        old_slp_timer_en=FIELD_EX32(s->ulp_cp_timer,  RTC_CNTL_ULP_CP_TIMER, SLP_TIMER_EN);
        s->ulp_cp_timer = value;
        uint32_t slp_timer_en=FIELD_EX32(value,  RTC_CNTL_ULP_CP_TIMER, SLP_TIMER_EN);
        qemu_set_irq(s->set_ulp_pc,FIELD_EX32(value,RTC_CNTL_ULP_CP_TIMER,PC_INIT));
        if(slp_timer_en != old_slp_timer_en)
            qemu_set_irq(s->enable_ulp_timer, slp_timer_en?s->ulp_cp_timer1:0);
        
        break;
    case A_RTC_CNTL_ULP_CP_TIMER1:
        s->ulp_cp_timer1 = value;
        break;

    case A_RTC_CNTL_STORE0:
    case A_RTC_CNTL_STORE1:
    case A_RTC_CNTL_STORE2:
    case A_RTC_CNTL_STORE3:
        s->scratch_reg[(addr - A_RTC_CNTL_STORE0) / 4] = value;
        break;

    case A_RTC_CNTL_SLEEP_TIMER0:
        s->sleep_timer0_reg = value;
        break;

    case A_RTC_CNTL_SLEEP_TIMER1:
        s->sleep_timer1_reg = value;
        break;

    case A_RTC_CNTL_CLK_CONF:
     //   s->soc_clk = FIELD_EX32(value, RTC_CNTL_CLK_CONF, SOC_CLK_SEL);
        s->rtc_fastclk = FIELD_EX32(value, RTC_CNTL_CLK_CONF, FAST_CLK_RTC_SEL);
        s->rtc_slowclk = FIELD_EX32(value, RTC_CNTL_CLK_CONF, ANA_CLK_RTC_SEL);
        esp32s3_rtc_update_clk(s);
        break;

    case A_RTC_CNTL_SW_CPU_STALL:
        s->sw_cpu_stall_reg = value;
        esp32s3_rtc_update_cpu_stall(s);
        break;

    case A_RTC_CNTL_STORE4:
    case A_RTC_CNTL_STORE5:
    case A_RTC_CNTL_STORE6:
    case A_RTC_CNTL_STORE7:
        s->scratch_reg[(addr - A_RTC_CNTL_STORE4) / 4 + 4] = value;
        break;
    case A_RTC_CNTL_WAKEUP_STATE:
        s->wakeup_state_reg = value;
        break;
    case A_RTC_CNTL_EXT_WAKEUP1:
        s->ext_wakeup1 = value;
        break;
    case A_RTC_CNTL_EXT_WAKEUP_CONF:
        if(DEBUG)
            printf("wakeup_conf %x\n",(uint32_t)value);
        s->wakeup_conf = value;
        break;
//    case A_RTC_MEM_CONF:
//        s->mem_conf = value;
//        break;
    case A_RTC_CNTL_INT_RAW:
        s->int_raw = value;
        break;
    case A_RTC_CNTL_INT_ENA:
        s->int_en = value;
        break;
    case A_RTC_CNTL_INT_CLR:
        s->int_raw &= ~value;
        if(!(s->int_raw & s->int_en)) qemu_irq_lower(s->irq);
        break;
    case A_RTC_CNTL_SDIO_CONF:
        s->sdio_conf = value;
        break;
    case A_RTC_CNTL_DIG_PWC:
        s->dig_pwc = value;
        break;
    case A_RTC_CNTL_RTC_SLP_WAKEUP_CAUSE:
        s->wakeup_cause = value;
        break;
    case A_RTC_CNTL_ULP_CP_CRTL:
        if(!FIELD_EX32(value,RTC_CNTL_ULP_CP_CRTL,START_TOP)) {
            s->ulp_cp_timer1=200<<8;
            qemu_set_irq(s->enable_ulp_timer, 0);
        }
        
        break;
    }
}

static void esp32s3_rtc_update_cpu_stall(Esp32s3RtcCntlState* s)
{
    uint32_t procpu_stall = (FIELD_EX32(s->sw_cpu_stall_reg, RTC_CNTL_SW_CPU_STALL, PROCPU_C1) << 2) |
                            (FIELD_EX32(s->options0_reg, RTC_CNTL_OPTIONS0, SW_STALL_PROCPU_C0));

    uint32_t appcpu_stall = (FIELD_EX32(s->sw_cpu_stall_reg, RTC_CNTL_SW_CPU_STALL, APPCPU_C1) << 2) |
                            (FIELD_EX32(s->options0_reg, RTC_CNTL_OPTIONS0, SW_STALL_APPCPU_C0));

    const uint32_t stall_magic_val = 0x86;

    s->cpu_stall_state[0] = procpu_stall == stall_magic_val;
    s->cpu_stall_state[1] = appcpu_stall == stall_magic_val;

    qemu_set_irq(s->cpu_stall_req[0], s->cpu_stall_state[0]);
    qemu_set_irq(s->cpu_stall_req[1], s->cpu_stall_state[1]);
}

static void esp32s3_rtc_update_clk(Esp32s3RtcCntlState* s)
{
    const uint32_t slowclk_freq[] = {150000, 32768, 8000000/256};
    const uint32_t fastclk_freq[] = {s->xtal_apb_freq / 4, 8000000};
    s->rtc_slowclk_freq = slowclk_freq[s->rtc_slowclk];
    s->rtc_fastclk_freq = fastclk_freq[s->rtc_fastclk];
    qemu_irq_pulse(s->clk_update);
}

static const MemoryRegionOps esp32s3_rtc_cntl_ops = {
    .read =  esp32s3_rtc_cntl_read,
    .write = esp32s3_rtc_cntl_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void esp32s3_rtc_cntl_reset_hold(Object *obj, ResetType type)
{
    Esp32s3RtcCntlState *s = ESP32S3_RTC_CNTL(obj);

    s->time_base_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
}

static void esp32s3_rtc_cntl_realize(DeviceState *dev, Error **errp)
{
}

static void esp32s3_rtc_cntl_init(Object *obj)
{
    Esp32s3RtcCntlState *s = ESP32S3_RTC_CNTL(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &esp32s3_rtc_cntl_ops, s,
                          TYPE_ESP32S3_RTC_CNTL, ESP32S3_RTC_CNTL_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->dig_reset_req, ESP32S3_RTC_DIG_RESET_GPIO, 1);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->cpu_reset_req[0], ESP32S3_RTC_CPU_RESET_GPIO, ESP32S3_CPU_COUNT);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->cpu_stall_req[0], ESP32S3_RTC_CPU_STALL_GPIO, ESP32S3_CPU_COUNT);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->clk_update, ESP32S3_RTC_CLK_UPDATE_GPIO, 1);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->enable_ulp_timer, ESP32S3_ULP_TIMER_GPIO, 1);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->rtc_wakeup, ESP32S3_RTC_WAKEUP_GPIO, 1);
    qdev_init_gpio_out_named(DEVICE(sbd), &s->set_ulp_pc, ESP32S3_SET_ULP_PC_GPIO, 1);

    for (int i = 0; i < ESP32S3_CPU_COUNT; ++i) {
        s->reset_cause[i] = ESP32_POWERON_RESET;
        s->stat_vector_sel[i] = true;
    }

    s->rtc_slowclk = ESP32_SLOW_CLK_RC;
    s->rtc_fastclk = ESP32_FAST_CLK_8M;
    s->soc_clk = ESP32_SOC_CLK_XTAL;
    s->xtal_apb_freq = 40000000;
    s->pll_apb_freq = 80000000;
    s->low_power_state_reg = 0;//0x92d;
    s->ulp_cp_timer1 = 200<<8;
    timer_init_ns(&s->sleep_timer, QEMU_CLOCK_REALTIME, sleep_timer_cb, s);
    esp32s3_rtc_update_clk(s);
}

static Property esp32s3_rtc_cntl_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void esp32s3_rtc_cntl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = esp32s3_rtc_cntl_reset_hold;
    dc->realize = esp32s3_rtc_cntl_realize;
    device_class_set_props(dc, esp32s3_rtc_cntl_properties);
}

static const TypeInfo esp32s3_rtc_cntl_info = {
    .name = TYPE_ESP32S3_RTC_CNTL,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Esp32s3RtcCntlState),
    .instance_init = esp32s3_rtc_cntl_init,
    .class_init = esp32s3_rtc_cntl_class_init
};

static void esp32s3_rtc_cntl_register_types(void)
{
    type_register_static(&esp32s3_rtc_cntl_info);
}

type_init(esp32s3_rtc_cntl_register_types)
