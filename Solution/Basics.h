#pragma once
#include <string>
#include <vector>

enum class OpCode
{
    ADD,
    SUB,
    ADDI,
    MUL,
    DIV,
    REM,
    LW,
    SW,
    BEQ,
    BNE,
    BLT,
    BLE,
    J,
    SLT,
    SLTI,
    AND,
    OR,
    XOR,
    ANDI,
    ORI,
    XORI
};
enum class UnitType
{
    ADDER,
    MULTIPLIER,
    DIVIDER,
    LOADSTORE,
    BRANCH,
    LOGIC
};

struct Instruction
{
    OpCode op;
    int dest;
    int src1;
    int src2;
    int imm;
    int pc;
};

struct ProcessorConfig
{
    int num_regs = 32;
    int rob_size = 64;
    int mem_size = 1024;

    int logic_lat = 1;
    int add_lat = 2;
    int mul_lat = 4;
    int div_lat = 5;
    int mem_lat = 4;

    int logic_rs_size = 4;
    int adder_rs_size = 4;
    int mult_rs_size = 2;
    int div_rs_size = 2;
    int br_rs_size = 2;
    int lsq_rs_size = 32;
};

struct ROBEntry
{
    int tag = -1;
    bool ready = false;
    bool valid = false;

    OpCode op = OpCode::ADD;
    int pc = 0;
    int dest = -1; // architectural register

    int value = 0; // value for register-writing instructions
    bool has_exception = false;

    bool is_branch_like = false;
    int predicted_next_pc = -1;
    int actual_next_pc = -1;
    bool branch_taken = false;

    int mem_address = 0;
    int store_value = 0;
};

struct RSEntry
{
    OpCode op = OpCode::ADD;
    int rob_tag = -1;
    int pc = 0;

    int dest = -1;
    int imm = 0;

    int src1_value = 0;
    int src2_value = 0;
    int src1_tag = -1;
    int src2_tag = -1;
    bool src1_ready = true;
    bool src2_ready = true;

    int predicted_next_pc = -1;

    bool is_load = false;
    bool is_store = false;
    bool is_issued = false;
};

struct BroadcastEvent
{
    int rob_tag = -1;
    int value = 0;
    bool has_exception = false;
    bool valid = false;

    bool has_control = false;
    bool branch_taken = false;
    int actual_next_pc = -1;
    int predicted_next_pc = -1;

    bool has_memory = false;
    int mem_address = 0;
    int store_value = 0;

    // Internal LSQ timing bookkeeping.
    bool lsq_forward_delay_applied = false;
};
