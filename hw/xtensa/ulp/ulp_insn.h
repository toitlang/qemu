#pragma once
#include <stdint.h>

/*
 * ESP32 ULP instructions are always:
 * - 32-bit
 * - little-endian
 * - opcode in bits [31:28]
 */
#define OPCODE_WR_REG 1         /*!< Instruction: write peripheral register (RTC_CNTL/RTC_IO/SARADC) */

#define OPCODE_RD_REG 2         /*!< Instruction: read peripheral register (RTC_CNTL/RTC_IO/SARADC)  */

#define OPCODE_I2C 3            /*!< Instruction: read/write I2C  */
#define OPCODE_I2C_RD 0
#define OPCODE_I2C_WR 1


#define OPCODE_WAIT 4            /*!< Instruction: delay (nop) for a given number of cycles */
#define OPCODE_ADC 5            /*!< Instruction: SAR ADC measurement  */

#define OPCODE_ST 6             /*!< Instruction: store indirect to RTC memory */
#define SUB_OPCODE_ST 4         /*!< Store 32 bits, 16 MSBs contain PC, 16 LSBs contain value from source register */

#define OPCODE_ALU 7            /*!< Arithmetic instructions */
#define SUB_OPCODE_ALU_REG 0    /*!< Arithmetic instruction, both source values are in register */
#define SUB_OPCODE_ALU_IMM 1    /*!< Arithmetic instruction, one source value is an immediate */
#define SUB_OPCODE_ALU_CNT 2    /*!< Arithmetic instruction between counter register and an immediate */
#define ALU_SEL_ADD 0           /*!< Addition */
#define ALU_SEL_SUB 1           /*!< Subtraction */
#define ALU_SEL_AND 2           /*!< Logical AND */
#define ALU_SEL_OR  3           /*!< Logical OR */
#define ALU_SEL_MOV 4           /*!< Copy value (immediate to destination register or source register to destination register */
#define ALU_SEL_LSH 5           /*!< Shift left by given number of bits */
#define ALU_SEL_RSH 6           /*!< Shift right by given number of bits */
#define ALU_SEL_SINC  0
#define ALU_SEL_SDEC  1
#define ALU_SEL_SRST  2

#define OPCODE_BRANCH 8         /*!< Branch instructions */
#define SUB_OPCODE_BX  0        /*!< Branch to absolute PC (immediate or in register) */
#define SUB_OPCODE_BR  1        /*!< Branch to relative PC */
#define SUB_OPCODE_BS  2        /*!< Branch to relative PC */

#define OPCODE_EXIT 9            /*!< Stop executing the program  */
#define SUB_OPCODE_WAKEUP 0        /*!< Stop executing the program and optionally wake up the chip */
#define SUB_OPCODE_SLEEP  1 /*!< Stop executing the program and run it again after selected interval */

#define OPCODE_TSENS 10         /*!< Instruction: temperature sensor measurement (not implemented yet) */

#define OPCODE_HALT 11             /*!< Instruction: store indirect to RTC memory */

#define OPCODE_LD 13             /*!< Instruction: store indirect to RTC memory */
#define SUB_OPCODE_LD 0         /*!< Store 32 bits, 16 MSBs contain PC, 16 LSBs contain value from source register */

typedef union ULPUInt {
    uint32_t raw;

    struct {
        uint32_t imm : 28;
        uint32_t op  : 4;
    } generic;

    struct {
        unsigned int dreg : 2;          /*!< Destination register */
        unsigned int sreg : 2;          /*!< Register with operand A */
        unsigned int treg : 2;          /*!< Register with operand B */
        unsigned int unused : 15;       /*!< Unused */
        unsigned int sel : 4;           /*!< Operation to perform, one of ALU_SEL_xxx */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_ALU_REG) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_ALU) */
    } alu_reg;                      /*!< Format of ALU instruction (both sources are registers) */

    struct {
        unsigned int dreg : 2;          /*!< Destination register */
        unsigned int sreg : 2;          /*!< Register with operand A */
        unsigned int imm  : 16;          /*!< imm value */
        unsigned int unused : 1;       /*!< Unused */
        unsigned int sel : 4;           /*!< Operation to perform, one of ALU_SEL_xxx */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_ALU_REG) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_ALU) */
    } alu_reg_i;                      /*!< Format of ALU instruction (both sources are registers) */

    struct {
        unsigned int unused1 : 4;          /*!< Destination register */
        unsigned int imm : 8;          /*!< imm value */
        unsigned int unused2 : 9;       /*!< Unused */
        unsigned int sel : 4;           /*!< Operation to perform, one of ALU_SEL_xxx */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_ALU_REG) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_ALU) */
    } alu_reg_s;                      /*!< Format of ALU instruction (both sources are registers) */


    struct {
        unsigned int dreg : 2;          /*!< Register which contains target PC, expressed in words (used if .reg == 1) */
        unsigned int addr : 11;         /*!< Target PC, expressed in words (used if .reg == 0) */
        unsigned int unused : 8;        /*!< Unused */
        unsigned int reg : 1;           /*!< Target PC in register (1) or immediate (0) */
        unsigned int type : 3;          /*!< Jump condition (BX_JUMP_TYPE_xxx) */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_BX) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_BRANCH) */
    } jump_alu_ri;                      /*!< Format of ALU instruction (both sources are registers) */

    struct {
        unsigned int threshold : 16;
        unsigned int judge : 1;         
        unsigned int step: 8;        
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_BX) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_BRANCH) */
    } jump_alu_relr;                      /*!< Format of ALU instruction  */

    struct {
        unsigned int threshold : 8;
        unsigned int unused : 7;
        unsigned int judge : 2;
        unsigned int step : 8;
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_BX) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_BRANCH) */
    } jump_alu_rels;                      /*!< Format of ALU instruction  */

    struct {
        unsigned int sreg : 2;
        unsigned int dreg : 2;
        unsigned int unused1 : 6;
        int offset : 11;
        unsigned int unused2 : 4;
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_BX) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_BRANCH) */
    } wr_mem;                      /*!< Format of ALU instruction  */

    struct {
        unsigned int dreg : 2;
        unsigned int sreg : 2;
        unsigned int unused1 : 6;
        int offset : 11;
        unsigned int unused2 : 4;
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_BX) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_BRANCH) */
    } rd_mem;                      /*!< Format of ALU instruction  */

    struct {
        unsigned int cycle_sel : 4;     /*!< Select which one of SARADC_ULP_CP_SLEEP_CYCx_REG to get the sleep duration from */
        unsigned int unused : 21;       /*!< Unused */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_SLEEP) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_END) */
    } cmd_sleep;                        /*!< Format of END instruction with wakeup */

    struct{
        unsigned int addr : 10;          /*!< Address within either RTC_CNTL, RTC_IO, or SARADC */
        unsigned int data : 8;          /*!< 8 bits of data to write */
        unsigned int low : 5;           /*!< Low bit */
        unsigned int high : 5;          /*!< High bit */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_WR_REG) */
    } cmd_wr_reg;                       /*!< Format of WR_REG instruction */

    struct {
        unsigned int addr : 10;          /*!< Address within either RTC_CNTL, RTC_IO, or SARADC */
        unsigned int unused : 8;        /*!< Unused */
        unsigned int low : 5;           /*!< Low bit */
        unsigned int high : 5;          /*!< High bit */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_WR_REG) */
    } cmd_rd_reg;                       /*!< Format of RD_REG instruction */

    struct {
        unsigned int i2c_addr : 8;      /*!< I2C slave address */
        unsigned int data : 8;          /*!< Data to read or write */
        unsigned int low_bits : 3;      /*!< TBD */
        unsigned int high_bits : 3;     /*!< TBD */
        unsigned int i2c_sel : 4;       /*!< TBD, select reg_i2c_slave_address[7:0] */
        unsigned int unused : 1;        /*!< Unused */
        unsigned int rw_bit : 1;            /*!< Write (1) or read (0) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_I2C) */
    } cmd_i2c;                          /*!< Format of I2C instruction */

    struct {
        unsigned int wakeup : 1;        /*!< Set to 1 to wake up chip */
        unsigned int unused : 24;       /*!< Unused */
        unsigned int sub_opcode : 3;    /*!< Sub opcode (SUB_OPCODE_WAKEUP) */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_END) */
    } cmd_wakeup;                          /*!< Format of END instruction with wakeup */

    struct {
        unsigned int wait : 16;        /*!< Set to 1 to wake up chip */
        unsigned int unused : 12;       /*!< Unused */
        unsigned int opcode : 4;        /*!< Opcode (OPCODE_WAIT) */
    } cmd_wait;                          /*!< Format of END instruction with wakeup */

    

    struct {
        unsigned int dreg : 2;          /*!< Register where to store temperature measurement result */
        unsigned int wait_delay : 14;    /*!< Cycles to wait after measurement is done */
        unsigned int cycles : 12;        /*!< Cycles used to perform measurement */
        unsigned int opcode : 4;         /*!< Opcode (OPCODE_TSENS) */
    } cmd_tsens;                        /*!< Format of TSENS instruction */


    struct{
        unsigned int dreg : 2;          /*!< Register where to store ADC result */
        unsigned int sar_mux : 4;           /*!< Select SARADC pad (mux + 1) */
        unsigned int sar_sel : 1;       /*!< Select SARADC0 (0) or SARADC1 (1) */
        unsigned int unused1 : 1;       /*!< Unused */
        unsigned int cycles : 16;       /*!< TBD, cycles used for measurement */
        unsigned int unused2 : 4;       /*!< Unused */
        unsigned int opcode : 4;         /*!< Opcode (OPCODE_ADC) */
    } cmd_adc;

} ULPInsn;
