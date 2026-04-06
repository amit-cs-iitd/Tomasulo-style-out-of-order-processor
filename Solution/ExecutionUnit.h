#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <deque>
#include <limits>
#include "Basics.h"

class ExecutionUnit
{
public:
    // per-unit reservation station
    UnitType name;
    int latency;
    int rs_capacity = 0;
    std::vector<RSEntry> rs;

    bool has_result = false;    // result flag
    bool has_exception = false; // exception flag

    struct InFlight
    {
        int remaining = 0;
        BroadcastEvent event;
        int rob_tag = -1;
    };

    std::deque<InFlight> pipeline;
    std::vector<BroadcastEvent> ready_broadcasts;

    ExecutionUnit() = default;
    ExecutionUnit(UnitType unit, int lat, int capacity) : name(unit), latency(lat), rs_capacity(capacity) {}

    bool hasSpace() const { return static_cast<int>(rs.size()) < rs_capacity; }
    int rsSize() const { return static_cast<int>(rs.size()); }

    void enqueue(const RSEntry &entry) { rs.push_back(entry); }

    void capture(int tag, int val)
    {
        for (auto &entry : rs)
        {
            if (!entry.src1_ready && entry.src1_tag == tag)
            {
                entry.src1_ready = true;
                entry.src1_value = val;
            }
            if (!entry.src2_ready && entry.src2_tag == tag)
            {
                entry.src2_ready = true;
                entry.src2_value = val;
            }
        }
    }

    static bool overflowsInt32(long long x)
    {
        return x > static_cast<long long>(std::numeric_limits<int>::max()) ||
               x < static_cast<long long>(std::numeric_limits<int>::min());
    }

    BroadcastEvent compute(const RSEntry &e)
    {
        BroadcastEvent out;
        out.valid = true;
        out.rob_tag = e.rob_tag;
        out.predicted_next_pc = e.predicted_next_pc;

        long long tmp = 0;
        switch (e.op)
        {
        case OpCode::ADD:
        case OpCode::ADDI:
            tmp = static_cast<long long>(e.src1_value) + static_cast<long long>(e.src2_value);
            if (overflowsInt32(tmp))
                out.has_exception = true;
            else
                out.value = static_cast<int>(tmp);
            break;
        case OpCode::SUB:
            tmp = static_cast<long long>(e.src1_value) - static_cast<long long>(e.src2_value);
            if (overflowsInt32(tmp))
                out.has_exception = true;
            else
                out.value = static_cast<int>(tmp);
            break;
        case OpCode::MUL:
            tmp = static_cast<long long>(e.src1_value) * static_cast<long long>(e.src2_value);
            if (overflowsInt32(tmp))
                out.has_exception = true;
            else
                out.value = static_cast<int>(tmp);
            break;
        case OpCode::DIV:
            if (e.src2_value == 0)
                out.has_exception = true;
            else
                out.value = e.src1_value / e.src2_value;
            break;
        case OpCode::REM:
            if (e.src2_value == 0)
                out.has_exception = true;
            else
                out.value = e.src1_value % e.src2_value;
            break;
        case OpCode::SLT:
        case OpCode::SLTI:
            out.value = (e.src1_value < e.src2_value) ? 1 : 0;
            break;
        case OpCode::AND:
        case OpCode::ANDI:
            out.value = e.src1_value & e.src2_value;
            break;
        case OpCode::OR:
        case OpCode::ORI:
            out.value = e.src1_value | e.src2_value;
            break;
        case OpCode::XOR:
        case OpCode::XORI:
            out.value = e.src1_value ^ e.src2_value;
            break;
        case OpCode::BEQ:
        case OpCode::BNE:
        case OpCode::BLT:
        case OpCode::BLE:
        {
            bool taken = false;
            if (e.op == OpCode::BEQ)
                taken = (e.src1_value == e.src2_value);
            if (e.op == OpCode::BNE)
                taken = (e.src1_value != e.src2_value);
            if (e.op == OpCode::BLT)
                taken = (e.src1_value < e.src2_value);
            if (e.op == OpCode::BLE)
                taken = (e.src1_value <= e.src2_value);
            out.has_control = true;
            out.branch_taken = taken;
            out.actual_next_pc = taken ? (e.pc + e.imm) : (e.pc + 1);
            break;
        }
        default:
            break;
        }
        return out;
    }

    void executeCycle()
    {
        has_result = false;
        has_exception = false;
        ready_broadcasts.clear();

        // issue stage
        int pick = -1;
        for (int i = 0; i < static_cast<int>(rs.size()); i++)
        {
            if (rs[i].src1_ready && rs[i].src2_ready && !rs[i].is_issued)
            {
                pick = i;
                break; // picking the oldest ready instruction
            }
        }

        if (pick != -1)
        {
            InFlight in;
            in.remaining = latency;
            in.event = compute(rs[pick]);
            in.rob_tag = rs[pick].rob_tag;
            pipeline.push_back(in);
            rs[pick].is_issued = true;
        }

        // Advance pipeline
        for (auto &stage : pipeline)
        {
            stage.remaining--;
        }

        // Complete stage
        while (!pipeline.empty() && pipeline.front().remaining <= 0)
        {
            auto &in = pipeline.front();
            ready_broadcasts.push_back(in.event);
            has_result = true;
            has_exception = has_exception || in.event.has_exception;

            int r_tag = in.rob_tag;
            auto it = std::find_if(rs.begin(), rs.end(), [r_tag](const RSEntry &e)
                                   { return e.rob_tag == r_tag; });
            if (it != rs.end())
            {
                rs.erase(it);
            }
            pipeline.pop_front();
        }
    }

    std::vector<BroadcastEvent> takeBroadcasts()
    {
        std::vector<BroadcastEvent> out = ready_broadcasts;
        ready_broadcasts.clear();
        return out;
    }
};
