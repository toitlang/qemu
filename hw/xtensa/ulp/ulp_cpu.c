#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "hw/xtensa/ulp_cpu.h"
#include "exec/address-spaces.h"
#include "qemu/timer.h"
#include "hw/sysbus.h"
#include "ulp_insn.h"
#include "hw/misc/esp32_reg.h"
#include "hw/misc/esp32_rtc_cntl.h"
#include "hw/qdev-properties.h"

#define DEBUG 0

// fetch next instruction from RTC slow memory
static inline uint32_t ulp_fetch(ULPCPUState *env)
{
    unsigned memaddr = (0x50000000 | ((env->pc * 4) & 0x1fff));
    uint32_t val;
    address_space_read(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);
    return val;
}

/* ---------- REG_WR and REG_RD instructions ---------- */

static void ulp_exec_reg_wr(ULPCPUState *env, ULPInsn insn)
{
    unsigned memaddr = (0x3ff48000 | (insn.cmd_wr_reg.addr * 4 & 0xfff));
    if(env->v2) memaddr=memaddr-0x3ff40000+0x60000000;
    uint32_t val;
    address_space_read(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);

    uint32_t width = insn.cmd_wr_reg.high - insn.cmd_wr_reg.low + 1;
    if (width > 8) {
        width = 8;
    }

    uint32_t mask = ((1u << width) - 1) << insn.cmd_wr_reg.low;
    val = (val & ~mask) |
          ((insn.cmd_wr_reg.data << insn.cmd_wr_reg.low) & mask);

    address_space_write(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);
    if(DEBUG)
        printf("REG_WR %x %d %x %d %d\n",memaddr,insn.cmd_wr_reg.data, val, insn.cmd_wr_reg.low,insn.cmd_wr_reg.high);
}

static void ulp_exec_reg_rd(ULPCPUState *env, ULPInsn insn)
{
    unsigned memaddr = (0x3ff48000 | (insn.cmd_rd_reg.addr * 4 & 0xfff));
    if(env->v2) memaddr=memaddr-0x3ff40000+0x60000000;
    uint32_t val;
    address_space_read(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);


    uint32_t width = insn.cmd_rd_reg.high - insn.cmd_rd_reg.low + 1;
    if (width > 16) {
        width = 16;
    }

    env->r[0] =
        (val >> insn.cmd_rd_reg.low) & ((1u << width) - 1);
    if(DEBUG)
        printf("REG_RD %x %x %x %d %d\n",memaddr,env->r[0], val, insn.cmd_rd_reg.low,insn.cmd_rd_reg.high);
}

/* ---------- JUMP instructions ---------- */

static inline int ulp_signed_step(uint8_t step)
{
    return (step & 0x80) ? -(step & 0x7f)-1 : (step & 0x7f)-1;
}



static void ulp_exec_jump(ULPCPUState *env, ULPInsn insn)
{
    bool take =
        insn.jump_alu_ri.type == 0 ||
        (insn.jump_alu_ri.type == 1 && env->zero) ||
        (insn.jump_alu_ri.type == 2 && env->overflow);
    if(DEBUG) printf("JUMP %d %d\n",take,insn.jump_alu_ri.reg);
    if (!take) {
        return;
    }

    
    if (insn.jump_alu_ri.reg == 0) {
        env->pc = insn.jump_alu_ri.addr;
    } else {
        env->pc = env->r[insn.jump_alu_ri.dreg];
    }
}
// Conditions for JUMPR instructions (v2 only)
#define JUMPR_EQ 2 
#define JUMPR_GT 1
#define JUMPR_LT 0
#define JUMPR_LE 3
#define JUMPR_GE 4
static void ulp_exec_jumpr(ULPCPUState *env, ULPInsn insn)
{
    int step = ulp_signed_step(insn.jump_alu_relr.step);
    if(env->v2)
        step = ulp_signed_step(insn.jump_alu_relr_v2.step);

    bool take =
        (insn.jump_alu_relr.judge == 0 && env->r[0] < insn.jump_alu_relr.threshold) ||
        (insn.jump_alu_relr.judge == 1 && env->r[0] >= insn.jump_alu_relr.threshold);

    if(env->v2)
        take=(insn.jump_alu_relr_v2.judge == 0 && env->r[0] < insn.jump_alu_relr_v2.threshold) ||
        (insn.jump_alu_relr_v2.judge == 1 && env->r[0] > insn.jump_alu_relr_v2.threshold) ||
        (insn.jump_alu_relr_v2.judge == 2 && env->r[0] == insn.jump_alu_relr_v2.threshold) ;

    if(DEBUG) printf("JUMPR %d %d\n",take,step);
    if (take) {
        env->pc += step;
    }
}
// Conditions for JUMPS instructions
#define JUMPS_EQ 4 
#define JUMPS_GT 3
#define JUMPS_LT 1
#define JUMPS_LE 5
#define JUMPS_GE 7
static void ulp_exec_jumps(ULPCPUState *env, ULPInsn insn)
{
    if(env->v2) {
        int step = ulp_signed_step(insn.jump_alu_rels_v2.step);
        int judge = insn.jump_alu_rels_v2.judge;
        int threshold = insn.jump_alu_rels_v2.threshold;
        int take=(judge == JUMPS_LT && env->stage_cnt < threshold) ||
            (judge == JUMPS_EQ && env->stage_cnt == threshold) ||
            (judge == JUMPS_GT && env->stage_cnt > threshold) ||
            (judge == JUMPS_LE && env->stage_cnt <= threshold) ||
            (judge == JUMPS_GE && env->stage_cnt >= threshold);

        if(DEBUG) printf("JUMPS_V2 %d %d %d %d\n",take,step, judge, threshold);
        if (take) {
            env->pc += step;
        }
    } else {
        int step = ulp_signed_step(insn.jump_alu_rels.step);

        bool take =
            (insn.jump_alu_rels.judge == 0 && env->stage_cnt <  insn.jump_alu_rels.threshold) ||
            (insn.jump_alu_rels.judge == 1 && env->stage_cnt >= insn.jump_alu_rels.threshold) ||
            (insn.jump_alu_rels.judge >= 2 && env->stage_cnt <= insn.jump_alu_rels.threshold);

        if(DEBUG) printf("JUMPS %d %d\n",take,step);
        if (take) {
            env->pc += step;
        }
    }
}

// ALU instructions
static void ulp_exec_alu(ULPCPUState *env, ULPInsn insn)
{
    uint32_t lhs = env->r[insn.alu_reg.sreg];
    uint32_t rhs = 0;

    int sub_op=insn.alu_reg.sub_opcode;
    if(env->v2) sub_op=sub_op>>1;

    if(DEBUG) printf("ALU %d %d\n",sub_op,env->v2);
    
    switch (sub_op) {
        case SUB_OPCODE_ALU_REG:
            rhs=env->r[insn.alu_reg.treg];
        break;
        case SUB_OPCODE_ALU_IMM:
            rhs=insn.alu_reg_i.imm;
        break;
        case SUB_OPCODE_ALU_CNT:
            lhs = env->stage_cnt;
            rhs = insn.alu_reg_s.imm;
        break;
        default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "ULP: illegal ALU Opcode %u\n", insn.alu_reg.sub_opcode);
    }

    uint32_t res = 0;
    env->overflow = false;

    switch (insn.alu_reg_i.sel) {
    case 0: /* ADD */
        res = lhs + rhs;
        env->overflow = (res < lhs);
        break;
    case 1: /* SUB */
        res = lhs - rhs;
        env->overflow = (lhs < rhs);
        break;
    case 2: /* AND */
        res = lhs & rhs;
        break;
    case 3: /* OR */
        res = lhs | rhs;
        break;
    case 4: /* MOV */
        res = rhs;
        break;
    case 5: /* LSH */
        res = lhs << (rhs & 0x1f);
        break;
    case 6: /* RSH */
        res = lhs >> (rhs & 0x1f);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "ULP: illegal ALU sel %u\n", insn.alu_reg_i.sel);
        return;
    }

    if(sub_op==SUB_OPCODE_ALU_CNT)
        env->stage_cnt = res;
    else
        env->r[insn.alu_reg_i.dreg] = res;
    env->zero = (res == 0);
}

// LD and ST instructions
static void ulp_exec_ld(ULPCPUState *env, ULPInsn insn)
{
    unsigned memaddr = (0x50000000 | ((insn.rd_mem.offset+env->r[insn.rd_mem.sreg]) * 4 & 0xfff));
    uint32_t val;
    address_space_read(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);
    env->r[insn.rd_mem.dreg] = val & 0xffff;
    if(DEBUG)
        printf("LD r[%d],r[%d]+%d : %x %x\n",insn.rd_mem.dreg, insn.rd_mem.sreg, insn.rd_mem.offset, memaddr,val);
}

static void ulp_exec_st(ULPCPUState *env, ULPInsn insn)
{
    unsigned memaddr = (0x50000000 | ((insn.wr_mem.offset+env->r[insn.wr_mem.dreg]) * 4 & 0xfff));
    uint32_t val=env->r[insn.wr_mem.sreg] | (env->pc<<21) | (insn.wr_mem.dreg<<16);
    address_space_write(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);
    if(DEBUG)
        printf("ST %x %x\n",memaddr,val);
}

// fetch and execute a single instruciotn 
static void ulp_cpu_step(ULPCPU *cpu)
{
    ULPCPUState *env = &cpu->env;
    int sub_op;

    if (env->halted) {
        return;
    }
    if(env->wait_instructions>0) {
        env->wait_instructions--;
        return;
    }

    ULPInsn insn;
    insn.raw = ulp_fetch(env);
    if(DEBUG)
        printf("ulp_cpu_step: %x %x %x %x %x %x %x\n",env->pc,insn.raw, env->r[0],env->r[1],env->r[2],env->r[3],env->stage_cnt);
    env->pc++;

    switch (insn.generic.op) {

    case OPCODE_ALU:
        ulp_exec_alu(env, insn);
        break;
    case OPCODE_WR_REG:
        ulp_exec_reg_wr(env, insn);
        break;
    case OPCODE_RD_REG:
        ulp_exec_reg_rd(env, insn);
        break;
    case OPCODE_LD:
        ulp_exec_ld(env, insn);
        break;
     case OPCODE_ST:
        ulp_exec_st(env, insn);
        break;
    case OPCODE_BRANCH:
        sub_op=insn.jump_alu_ri.sub_opcode;
        if(env->v2) {
            sub_op=sub_op>>1;
            if(sub_op!=2) sub_op=1-sub_op;
        }
        switch (sub_op) {
        case 0: ulp_exec_jump(env, insn);  break;
        case 1: ulp_exec_jumpr(env, insn); break;
        case 2: ulp_exec_jumps(env, insn); break;
        }
        break;
    case OPCODE_HALT:
        env->halted = true;
        env->pc=0;
        if(DEBUG)
                printf("HALT\n");
        break;
    case OPCODE_EXIT:
        switch(insn.cmd_sleep.sub_opcode) {
        case SUB_OPCODE_SLEEP:
            env->timer_number=insn.cmd_sleep.cycle_sel;
            break;
        case SUB_OPCODE_WAKEUP:
            if(DEBUG)
                printf("WAKE\n");
            qemu_set_irq(env->rtc_wakeup,RTC_ULP_TRIG_EN);
            break;
        }
        break;
    case OPCODE_WAIT:
        env->wait_instructions=insn.cmd_wait.wait/100;
        if(DEBUG)
            printf("WAIT %d\n",env->wait_instructions);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "ULP: illegal instruction 0x%08x\n", insn.raw);
        env->halted = true;
        break;
    }
}

static void run_cpu(ULPCPU *cpu) {
    ULPCPUState *env = &cpu->env;
    for(int i=0;i<1000 && !env->halted;i++)
        ulp_cpu_step(cpu);
}

static void start_timer(ULPCPUState *env) {
    
//    if(env->v2)
//        memaddr = 0x60008000 + 0x134; 
    uint32_t val;
    if(!env->v2) {
        unsigned memaddr = 0x3FF48818+env->timer_number*4;
        address_space_read(&address_space_memory, memaddr,
                            MEMTXATTRS_UNSPECIFIED, &val, 4);
    } else {
        val=env->timer_on>>8;
    }

    // esp32 uses 150KHz RTC_SLOW_CLK for cycle counting
    // esp32s3 uses 136KHz RTC_SLOW_CLK
    int64_t v64;
    if(env->v2) v64=((int64_t)val*1e6)/136;
    else v64=((int64_t)val*1e6)/150;
    if(DEBUG)
        printf("Timer restart in %dns\n",(int)v64);                    
    timer_mod(&env->ulp_timer,qemu_clock_get_ns(QEMU_CLOCK_REALTIME)+v64);
}

static void ulp_timer_cb(void *v) {
    ULPCPU *cpu=(ULPCPU *)v;
    ULPCPUState *env = &cpu->env;
    env->halted=false;
//    printf("timer expire\n");
    run_cpu(cpu);
//    if(!env->halted)
    if(env->timer_on)
        start_timer(env);
}
static void set_ulp_pc(void *opaque, int n, int val) {
    ULPCPU *cpu=ULP_CPU(opaque);
    ULPCPUState *s = &cpu->env;
    if(DEBUG)
        printf("set_ulp_pc %x\n",val);
    s->start_pc=val;
 
 //   if(val & 0x30)
 //       run_cpu(cpu);
}
static void ulp_timer_start(void *opaque, int n, int val) {
    ULPCPU *cpu=ULP_CPU(opaque);
    ULPCPUState *s = &cpu->env;
    if(DEBUG)
        printf("timer_start %d\n",val);
    if(val==s->timer_on) return;
    if(val) {
        s->pc=s->start_pc;
//        run_cpu(cpu);
        start_timer(s);
        s->timer_on=val;
    } else {
        timer_del(&s->ulp_timer);
        s->timer_on=false;
    }
}

static void ulp_cpu_reset(DeviceState *dev)
{
    ULPCPU *cpu = ULP_CPU(dev);
    ULPCPUState *env = &cpu->env;
    if(DEBUG)
        printf("ulp reset\n");
//    memset(env, 0, sizeof(*env));
    env->halted = false;
    env->timer_number=0;
    ulp_timer_start(cpu,0,0);
}

static void ulp_cpu_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
   // SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    ULPCPUState *s = &(ULP_CPU(dev)->env);
    
    qdev_init_gpio_in_named(dev, ulp_timer_start, ULP_TIMER_GPIO, 1);
    qdev_init_gpio_out_named(dev, &s->rtc_wakeup, ULP_WAKEUP_GPIO, 1);
    qdev_init_gpio_in_named(dev, set_ulp_pc, ULP_SET_PC_GPIO, 1);
    timer_init_ns(&s->ulp_timer, QEMU_CLOCK_REALTIME, ulp_timer_cb,
                      (void *)cs);
//    cpu_reset(cs);
}

static Property ulp_properties[] = {
    DEFINE_PROP_BOOL("ulp_type",ULPCPUState,v2,false),
    DEFINE_PROP_END_OF_LIST(),
};

static void ulp_cpu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);

    dc->realize = ulp_cpu_realize;

    device_class_set_legacy_reset(dc,ulp_cpu_reset);
    device_class_set_props(dc, ulp_properties);
    cc->has_work = NULL;
}

static const TypeInfo ulp_cpu_type_info = {
    .name          = TYPE_ULP_CPU,
    .parent        = TYPE_CPU,
    .instance_size = sizeof(ULPCPU),
    .class_init    = ulp_cpu_class_init,
};

static void ulp_cpu_register_types(void)
{
    type_register_static(&ulp_cpu_type_info);
}

type_init(ulp_cpu_register_types)
