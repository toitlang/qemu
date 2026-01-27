#pragma once

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qom/object.h"
#include "qemu/timer.h"
#include "sysemu/runstate.h"
#include "hw/irq.h"

#define TYPE_ULP_CPU "esp32-ulp-cpu"

#define ULP_TIMER_GPIO "ulp_timer_start"
#define ULP_WAKEUP_GPIO "ulp_wakeup_gpio"
#define ULP_SET_PC_GPIO "ulp_set_pc_gpio"

OBJECT_DECLARE_SIMPLE_TYPE(ULPCPU, ULP_CPU)

typedef struct ULPCPUState {
    uint32_t pc;
    uint16_t r[4];

    uint32_t start_pc;
    bool zero;
    bool overflow;
    bool halted;
    uint32_t timer_on;
    int timer_number;
    int wait_instructions;
    bool v2;

    uint32_t stage_cnt;
    QEMUTimer ulp_timer;
    qemu_irq rtc_wakeup;

} ULPCPUState;

struct ULPCPU {
    CPUState parent_obj;
    ULPCPUState env;
};

