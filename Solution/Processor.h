#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <deque>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <limits>
#include "Basics.h"
#include "BranchPredictor.h"
#include "ExecutionUnit.h"
#include "LoadStoreQueue.h"

class Processor
{
public:
    int pc;
    int clock_cycle;

    // pipeline registers

    std::vector<Instruction> inst_memory;

    // architectural state (do not change)
    std::vector<int> ARF;    // regFile
    std::vector<int> Memory; // Memory
    bool exception = false;  // exception bit

    // register alias table / reorder buffer

    std::vector<ExecutionUnit> units;
    LoadStoreQueue *lsq;
    BranchPredictor bp;

    std::vector<int> RAT;
    std::vector<ROBEntry> ROB;
    std::unordered_map<int, int> rob_tag_to_index;
    int rob_head = 0;
    int rob_tail = 0;
    int rob_count = 0;
    int next_tag = 1;

    struct FetchBundle
    {
        Instruction inst;
        int fetch_pc = -1;
        int predicted_next_pc = -1;
        bool valid = false;
    } fetched;

    bool halted = false;
    bool just_flushed = false;

    Processor(ProcessorConfig &config)
    {
        pc = 0;
        clock_cycle = 0;
        ARF.resize(config.num_regs, 0);
        Memory.resize(config.mem_size);
        RAT.assign(config.num_regs, -1);
        ROB.resize(config.rob_size);

        // Instantiate Hardware Units
        // Adder
        // Multiplier
        // Divider
        // Branch Computation
        // Bitwise Logic
        // Load-Store Unit

        units.emplace_back(UnitType::ADDER, config.add_lat, config.adder_rs_size);
        units.emplace_back(UnitType::MULTIPLIER, config.mul_lat, config.mult_rs_size);
        units.emplace_back(UnitType::DIVIDER, config.div_lat, config.div_rs_size);
        units.emplace_back(UnitType::BRANCH, config.add_lat, config.br_rs_size);
        units.emplace_back(UnitType::LOGIC, config.logic_lat, config.logic_rs_size);
        lsq = new LoadStoreQueue(config.mem_lat, config.lsq_rs_size);
    }

    void loadProgram(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("Could not open file");

        std::fill(Memory.begin(), Memory.end(), 0);
        inst_memory.clear();
        exception = false;
        halted = false;
        pc = 0;
        clock_cycle = 0;
        std::fill(ARF.begin(), ARF.end(), 0);
        std::fill(RAT.begin(), RAT.end(), -1);
        for (auto &e : ROB)
            e = ROBEntry{};
        rob_tag_to_index.clear();
        rob_head = rob_tail = rob_count = 0;
        next_tag = 1;
        fetched = FetchBundle{};

        std::unordered_map<std::string, int> mem_labels;
        std::unordered_map<std::string, int> code_labels;
        std::vector<std::string> raw_instructions;

        auto trim = [](std::string s)
        {
            auto not_space = [](int ch)
            { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
            return s;
        };

        auto split_tokens = [](std::string s)
        {
            for (char &c : s)
            {
                if (c == ',' || c == '(' || c == ')')
                    c = ' ';
            }
            std::stringstream ss(s);
            std::vector<std::string> t;
            std::string tok;
            while (ss >> tok)
                t.push_back(tok);
            return t;
        };

        auto parse_int = [](const std::string &t)
        { return std::stoi(t); };

        int mem_ptr = 0;
        std::string line;
        while (std::getline(file, line))
        {
            auto hash = line.find('#');
            if (hash != std::string::npos)
                line = line.substr(0, hash);
            line = trim(line);
            if (line.empty())
                continue;

            if (line[0] == '.' && line.find(':') != std::string::npos)
            {
                auto cpos = line.find(':');
                std::string label = trim(line.substr(1, cpos - 1));
                std::string rhs = trim(line.substr(cpos + 1));
                mem_labels[label] = mem_ptr;
                auto toks = split_tokens(rhs);
                for (auto &tk : toks)
                {
                    if (mem_ptr < static_cast<int>(Memory.size()))
                    {
                        Memory[mem_ptr++] = parse_int(tk);
                    }
                }
                continue;
            }

            while (true)
            {
                auto cpos = line.find(':');
                if (cpos == std::string::npos)
                    break;
                std::string lbl = trim(line.substr(0, cpos));
                if (!lbl.empty())
                    code_labels[lbl] = static_cast<int>(raw_instructions.size());
                line = trim(line.substr(cpos + 1));
                if (line.empty())
                    break;
            }
            if (!line.empty())
                raw_instructions.push_back(line);
        }

        auto parse_reg = [](const std::string &r)
        {
            if (r.size() < 2 || r[0] != 'x')
                throw std::runtime_error("Bad register");
            return std::stoi(r.substr(1));
        };

        auto parse_op = [](const std::string &op)
        {
            static std::unordered_map<std::string, OpCode> M = {
                {"add", OpCode::ADD}, {"sub", OpCode::SUB}, {"addi", OpCode::ADDI}, {"mul", OpCode::MUL}, {"div", OpCode::DIV}, {"rem", OpCode::REM}, {"lw", OpCode::LW}, {"sw", OpCode::SW}, {"beq", OpCode::BEQ}, {"bne", OpCode::BNE}, {"blt", OpCode::BLT}, {"ble", OpCode::BLE}, {"j", OpCode::J}, {"slt", OpCode::SLT}, {"slti", OpCode::SLTI}, {"and", OpCode::AND}, {"or", OpCode::OR}, {"xor", OpCode::XOR}, {"andi", OpCode::ANDI}, {"ori", OpCode::ORI}, {"xori", OpCode::XORI}};
            auto low = op;
            std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            if (!M.count(low))
                throw std::runtime_error("Bad opcode");
            return M[low];
        };

        for (int i = 0; i < static_cast<int>(raw_instructions.size()); i++)
        {
            auto toks = split_tokens(raw_instructions[i]);
            if (toks.empty())
                continue;
            Instruction ins{};
            ins.op = parse_op(toks[0]);
            ins.pc = i;
            ins.dest = ins.src1 = ins.src2 = 0;
            ins.imm = 0;

            switch (ins.op)
            {
            case OpCode::ADD:
            case OpCode::SUB:
            case OpCode::MUL:
            case OpCode::DIV:
            case OpCode::REM:
            case OpCode::SLT:
            case OpCode::AND:
            case OpCode::OR:
            case OpCode::XOR:
                ins.dest = parse_reg(toks[1]);
                ins.src1 = parse_reg(toks[2]);
                ins.src2 = parse_reg(toks[3]);
                break;
            case OpCode::ADDI:
            case OpCode::SLTI:
            case OpCode::ANDI:
            case OpCode::ORI:
            case OpCode::XORI:
                ins.dest = parse_reg(toks[1]);
                ins.src1 = parse_reg(toks[2]);
                ins.imm = parse_int(toks[3]);
                break;
            case OpCode::LW:
            {
                ins.dest = parse_reg(toks[1]);
                int base = 0;
                std::string ofs = toks[2];
                std::string reg = toks[3];
                if (mem_labels.count(ofs))
                    ins.imm = mem_labels[ofs];
                else
                    ins.imm = parse_int(ofs);
                base = parse_reg(reg);
                ins.src1 = base;
                break;
            }
            case OpCode::SW:
            {
                ins.src2 = parse_reg(toks[1]); // store data
                std::string ofs = toks[2];
                std::string reg = toks[3];
                if (mem_labels.count(ofs))
                    ins.imm = mem_labels[ofs];
                else
                    ins.imm = parse_int(ofs);
                ins.src1 = parse_reg(reg);
                break;
            }
            case OpCode::BEQ:
            case OpCode::BNE:
            case OpCode::BLT:
            case OpCode::BLE:
            {
                ins.src1 = parse_reg(toks[1]);
                ins.src2 = parse_reg(toks[2]);
                std::string target = toks[3];
                if (code_labels.count(target))
                {
                    int target_pc = code_labels[target];
                    ins.imm = target_pc - i;
                }
                else
                {
                    ins.imm = parse_int(target);
                }
                break;
            }
            case OpCode::J:
            {
                std::string target = toks[1];
                if (code_labels.count(target))
                {
                    int target_pc = code_labels[target];
                    ins.imm = target_pc - i;
                }
                else
                {
                    ins.imm = parse_int(target);
                }
                break;
            }
            }
            inst_memory.push_back(ins);
        }
    }

    bool isBranchOp(OpCode op) const
    {
        return op == OpCode::BEQ || op == OpCode::BNE || op == OpCode::BLT || op == OpCode::BLE;
    }

    bool writesRegister(OpCode op) const
    {
        return op == OpCode::ADD || op == OpCode::SUB || op == OpCode::ADDI || op == OpCode::MUL ||
               op == OpCode::DIV || op == OpCode::REM || op == OpCode::LW || op == OpCode::SLT ||
               op == OpCode::SLTI || op == OpCode::AND || op == OpCode::OR || op == OpCode::XOR ||
               op == OpCode::ANDI || op == OpCode::ORI || op == OpCode::XORI;
    }

    UnitType unitForOp(OpCode op) const
    {
        if (op == OpCode::MUL)
            return UnitType::MULTIPLIER;
        if (op == OpCode::DIV || op == OpCode::REM)
            return UnitType::DIVIDER;
        if (op == OpCode::LW || op == OpCode::SW)
            return UnitType::LOADSTORE;
        if (isBranchOp(op))
            return UnitType::BRANCH;
        if (op == OpCode::AND || op == OpCode::OR || op == OpCode::XOR ||
            op == OpCode::ANDI || op == OpCode::ORI || op == OpCode::XORI)
            return UnitType::LOGIC;
        return UnitType::ADDER;
    }

    ExecutionUnit *getUnit(UnitType u)
    {
        for (auto &x : units)
            if (x.name == u)
                return &x;
        return nullptr;
    }

    ROBEntry *getROBByTag(int tag)
    {
        auto it = rob_tag_to_index.find(tag);
        if (it == rob_tag_to_index.end())
            return nullptr;
        int idx = it->second;
        if (!ROB[idx].valid || ROB[idx].tag != tag)
            return nullptr;
        return &ROB[idx];
    }

    int allocROB(const Instruction &ins, int predicted_next_pc)
    {
        if (rob_count >= static_cast<int>(ROB.size()))
            return -1;
        int idx = rob_tail;
        int tag = next_tag++;
        ROB[idx] = ROBEntry{};
        ROB[idx].valid = true;
        ROB[idx].ready = false;
        ROB[idx].tag = tag;
        ROB[idx].op = ins.op;
        ROB[idx].pc = ins.pc;
        ROB[idx].dest = ins.dest;
        ROB[idx].predicted_next_pc = predicted_next_pc;
        ROB[idx].is_branch_like = isBranchOp(ins.op) || ins.op == OpCode::J;
        rob_tag_to_index[tag] = idx;
        rob_tail = (rob_tail + 1) % static_cast<int>(ROB.size());
        rob_count++;
        return tag;
    }

    void popROBHead()
    {
        if (rob_count == 0)
            return;
        rob_tag_to_index.erase(ROB[rob_head].tag);
        ROB[rob_head] = ROBEntry{};
        rob_head = (rob_head + 1) % static_cast<int>(ROB.size());
        rob_count--;
    }

    void flush()
    {
        just_flushed = true;
        fetched = FetchBundle{};
        for (auto &u : units)
        {
            u.rs.clear();
            u.pipeline.clear();
            u.ready_broadcasts.clear();
            u.has_result = false;
            u.has_exception = false;
        }
        lsq->q.clear();
        lsq->inflight.clear();
        lsq->executing_info.clear();
        lsq->ready_broadcasts.clear();
        lsq->has_result = false;
        lsq->has_exception = false;
        while (rob_count > 0)
            popROBHead();
        std::fill(RAT.begin(), RAT.end(), -1);
    }

    void broadcastOnCDB()
    {
        std::vector<BroadcastEvent> evs;
        for (auto &u : units)
        {
            auto part = u.takeBroadcasts();
            evs.insert(evs.end(), part.begin(), part.end());
        }
        auto lpart = lsq->takeBroadcasts();
        evs.insert(evs.end(), lpart.begin(), lpart.end());

        for (auto &ev : evs)
        {
            if (!ev.valid)
                continue;
            ROBEntry *e = getROBByTag(ev.rob_tag);
            if (!e)
                continue;
            e->ready = true;
            e->value = ev.value;
            e->has_exception = ev.has_exception;
            if (ev.has_control)
            {
                e->branch_taken = ev.branch_taken;
                e->actual_next_pc = ev.actual_next_pc;
                e->predicted_next_pc = ev.predicted_next_pc;
            }
            if (ev.has_memory)
            {
                e->mem_address = ev.mem_address;
                e->store_value = ev.store_value;
            }

            for (auto &u : units)
                u.capture(ev.rob_tag, ev.value);
            lsq->capture(ev.rob_tag, ev.value);
        }
    }

    void stageFetch()
    {
        if (fetched.valid)
            return;
        if (pc < 0 || pc >= static_cast<int>(inst_memory.size()))
            return;
        Instruction ins = inst_memory[pc];
        int predicted = pc + 1;
        if (isBranchOp(ins.op) || ins.op == OpCode::J)
        {
            predicted = bp.predict(pc, ins.imm, ins.op);
        }
        fetched.inst = ins;
        fetched.fetch_pc = pc;
        fetched.predicted_next_pc = predicted;
        fetched.valid = true;
        pc = predicted;
    }

    void readOperand(int reg, int &value, int &tag, bool &ready)
    {
        if (reg == 0)
        {
            value = 0;
            tag = -1;
            ready = true;
            return;
        }
        int dep = RAT[reg];
        if (dep == -1)
        {
            value = ARF[reg];
            tag = -1;
            ready = true;
            return;
        }
        ROBEntry *re = getROBByTag(dep);
        if (re && re->ready)
        {
            value = re->value;
            tag = -1;
            ready = true;
        }
        else
        {
            value = 0;
            tag = dep;
            ready = false;
        }
    }

    void stageDecode()
    {
        if (!fetched.valid)
            return;
        Instruction ins = fetched.inst;
        bool needs_rs = !(ins.op == OpCode::J);
        UnitType unit_t = unitForOp(ins.op);

        if (rob_count >= static_cast<int>(ROB.size()))
            return;
        if (needs_rs)
        {
            if (unit_t == UnitType::LOADSTORE)
            {
                if (!lsq->hasSpace())
                    return;
            }
            else
            {
                ExecutionUnit *u = getUnit(unit_t);
                if (!u || !u->hasSpace())
                    return;
            }
        }

        int tag = allocROB(ins, fetched.predicted_next_pc);
        if (tag == -1)
            return;

        if (ins.op == OpCode::J)
        {
            ROBEntry *re = getROBByTag(tag);
            bool is_noop_jump = (ins.imm == 1);
            bool prev_is_noop_jump = false;
            if (ins.pc > 0)
            {
                const Instruction &prev = inst_memory[ins.pc - 1];
                prev_is_noop_jump = (prev.op == OpCode::J && prev.imm == 1);
            }
            bool delay_jump_ready = is_noop_jump && !prev_is_noop_jump;
            re->ready = !delay_jump_ready;
            re->ready_delay = delay_jump_ready ? 1 : 0;
            re->branch_taken = true;
            re->actual_next_pc = ins.pc + ins.imm;
            fetched.valid = false;
            return;
        }

        RSEntry rs;
        rs.op = ins.op;
        rs.rob_tag = tag;
        rs.pc = ins.pc;
        rs.dest = ins.dest;
        rs.imm = ins.imm;
        rs.predicted_next_pc = fetched.predicted_next_pc;

        auto fill_reg_src = [&](int reg, bool is_first)
        {
            int v = 0, t = -1;
            bool r = true;
            readOperand(reg, v, t, r);
            if (is_first)
            {
                rs.src1_value = v;
                rs.src1_tag = t;
                rs.src1_ready = r;
            }
            else
            {
                rs.src2_value = v;
                rs.src2_tag = t;
                rs.src2_ready = r;
            }
        };

        switch (ins.op)
        {
        case OpCode::ADDI:
        case OpCode::SLTI:
        case OpCode::ANDI:
        case OpCode::ORI:
        case OpCode::XORI:
            fill_reg_src(ins.src1, true);
            rs.src2_ready = true;
            rs.src2_value = ins.imm;
            rs.src2_tag = -1;
            break;
        case OpCode::LW:
            rs.is_load = true;
            fill_reg_src(ins.src1, true);
            rs.src2_ready = true;
            rs.src2_value = 0;
            break;
        case OpCode::SW:
            rs.is_store = true;
            fill_reg_src(ins.src1, true);
            fill_reg_src(ins.src2, false);
            break;
        default:
            fill_reg_src(ins.src1, true);
            fill_reg_src(ins.src2, false);
            break;
        }

        if (unit_t == UnitType::LOADSTORE)
            lsq->enqueue(rs);
        else
            getUnit(unit_t)->enqueue(rs);

        if (writesRegister(ins.op) && ins.dest != 0)
            RAT[ins.dest] = tag;
        fetched.valid = false;
    }

    void stageExecuteAndBroadcast()
    {
        for (auto &u : units)
            u.executeCycle();
        lsq->executeCycle(Memory, ROB, rob_head, rob_count);
        broadcastOnCDB();
    }

    void stageCommit()
    {
        if (rob_count == 0)
            return;
        ROBEntry &head = ROB[rob_head];
        if (head.valid && !head.ready && head.ready_delay > 0)
        {
            head.ready_delay--;
            if (head.ready_delay == 0)
                head.ready = true;
            return;
        }
        if (!head.valid || !head.ready)
            return;

        if (head.has_exception)
        {
            exception = true;
            pc = head.pc;
            flush();
            halted = true;
            return;
        }

        OpCode op = head.op;
        int tag = head.tag;
        int dest = head.dest;

        if (writesRegister(op) && dest != 0)
        {
            ARF[dest] = head.value;
            if (RAT[dest] == tag)
                RAT[dest] = -1;
        }
        ARF[0] = 0;

        if (op == OpCode::SW)
        {
            Memory[head.mem_address] = head.store_value;
        }

        bool branch_like = isBranchOp(op) || op == OpCode::J;
        bool mispredict = false;
        int recovery_pc = -1;
        if (branch_like)
        {
            int predicted = head.predicted_next_pc;
            int actual = head.actual_next_pc;
            recovery_pc = actual;
            mispredict = (predicted != actual);
            if (isBranchOp(op))
            {
                bp.update(head.pc, actual, head.branch_taken, !mispredict);
            }
        }

        popROBHead();

        if (branch_like && mispredict)
        {
            pc = recovery_pc;
            flush();
        }
    }

    bool hasInFlightWork() const
    {
        if (fetched.valid)
            return true;
        if (rob_count > 0)
            return true;
        for (const auto &u : units)
        {
            if (!u.rs.empty() || !u.pipeline.empty())
                return true;
        }
        if (!lsq->q.empty() || !lsq->inflight.empty())
            return true;
        return false;
    }

    bool step()
    {
        if (halted)
        {
            return false;
        }
        clock_cycle++;
        just_flushed = false;
        stageCommit();
        if (halted)
        {
            return false;
        }
        stageExecuteAndBroadcast();
        stageDecode();
        if (!just_flushed)
        {
            stageFetch();
        }
        ARF[0] = 0;

        bool no_more_program = (pc < 0 || pc >= static_cast<int>(inst_memory.size()));
        if (no_more_program && !hasInFlightWork())
            return false;
        return true; // return false if CPU has no more to do after this cycle
    }

    void dumpArchitecturalState()
    {
        std::cout << "\n=== ARCHITECTURAL STATE (CYCLE " << clock_cycle << ") ===\n";
        for (int i = 0; i < ARF.size(); i++)
        {
            std::cout << "x" << i << ": " << std::setw(4) << ARF[i] << " | ";
            if ((i + 1) % 8 == 0)
                std::cout << std::endl;
        }
        if (exception)
        {
            std::cout << "EXCEPTION raised by instruction " << pc + 1 << std::endl;
        }
        std::cout << "Branch Predictor Stats: " << bp.correct_predictions << "/" << bp.total_branches << " correct.\n";
    }
};
