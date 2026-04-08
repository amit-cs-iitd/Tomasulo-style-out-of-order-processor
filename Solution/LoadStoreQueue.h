#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <deque>
#include "Basics.h"

class LoadStoreQueue
{
public:
    // LSQ reservation station
    int latency;
    int capacity = 0;
    std::deque<RSEntry> q;
    std::deque<std::pair<int, BroadcastEvent>> inflight;
    std::vector<BroadcastEvent> ready_broadcasts;
    std::vector<RSEntry> executing_info;

    bool has_result = false;    // result flag
    bool has_exception = false; // exception flag
    int store_data = 0;

    LoadStoreQueue() = default;
    LoadStoreQueue(int lat, int cap) : latency(lat), capacity(cap) {}

    bool hasSpace() const { return static_cast<int>(q.size()) < capacity; }
    int rsSize() const { return static_cast<int>(q.size()); }

    void enqueue(const RSEntry &entry) { q.push_back(entry); }

    void capture(int tag, int val)
    {
        for (auto &e : q)
        {
            if (!e.src1_ready && e.src1_tag == tag)
            {
                e.src1_ready = true;
                e.src1_value = val;
            }
            if (!e.src2_ready && e.src2_tag == tag)
            {
                e.src2_ready = true;
                e.src2_value = val;
            }
        }
    }

    void executeCycle(std::vector<int> &Memory, const std::vector<ROBEntry> &ROB, int rob_head, int rob_count)
    {
        has_result = false;
        has_exception = false;
        ready_broadcasts.clear();

        // Issue Stage
        for (auto &e : q)
        {
            if (!e.is_issued)
            {
                if (e.src1_ready && e.src2_ready)
                {
                    if (e.is_load)
                    {
                        int load_addr = e.src1_value + e.imm;
                        bool delay_load = false;
                        for (const auto &older : q)
                        {
                            if (older.rob_tag == e.rob_tag)
                                break;
                            if (!older.is_store || !older.is_issued || !older.src1_ready)
                                continue;
                            int store_addr = older.src1_value + older.imm;
                            if (store_addr != load_addr)
                                continue;

                            for (int k = 0; k < static_cast<int>(executing_info.size()) && k < static_cast<int>(inflight.size()); k++)
                            {
                                if (executing_info[k].rob_tag == older.rob_tag && inflight[k].first == 2)
                                {
                                    delay_load = true;
                                    break;
                                }
                            }
                            if (delay_load)
                                break;
                        }
                        if (delay_load)
                            break;
                    }

                    e.is_issued = true;
                    BroadcastEvent out;
                    out.valid = true;
                    out.rob_tag = e.rob_tag;
                    out.has_memory = true;

                    int addr = e.src1_value + e.imm;
                    out.mem_address = addr;
                    if (addr < 0 || addr >= static_cast<int>(Memory.size()))
                    {
                        out.has_exception = true;
                    }
                    else if (e.is_store)
                    {
                        out.store_value = e.src2_value;
                        store_data = e.src2_value;
                    }

                    inflight.push_back({latency, out});
                    executing_info.push_back(e);
                }
                break; // only issue the oldest unissued instruction.
            }
        }

        // Advance Pipeline
        for (auto &p : inflight)
            p.first--;

        // Complete Stage
        while (!inflight.empty() && inflight.front().first <= 0)
        {
            BroadcastEvent out = inflight.front().second;
            const RSEntry &origin = executing_info.front();

            if (out.has_memory && !out.has_exception && origin.is_load)
            {
                int load_tag = out.rob_tag;
                int forwarded_val = Memory[out.mem_address];
                bool has_forward_source = false;

                for (int i = 0; i < rob_count; i++)
                {
                    int idx = (rob_head + i) % static_cast<int>(ROB.size());
                    const auto &entry = ROB[idx];

                    if (entry.tag == load_tag)
                        break;
                    if (entry.op == OpCode::SW && entry.valid && entry.mem_address == out.mem_address)
                    {
                        forwarded_val = entry.store_value;
                        has_forward_source = true;
                    }
                }
                if (has_forward_source && !out.lsq_forward_delay_applied)
                {
                    out.lsq_forward_delay_applied = true;
                    inflight.front().second = out;
                    inflight.front().first = 1;
                    continue;
                }
                out.value = forwarded_val;
            }

            ready_broadcasts.push_back(out);
            has_result = true;
            has_exception = has_exception || out.has_exception;

            int r_tag = out.rob_tag;
            auto it = std::find_if(q.begin(), q.end(), [r_tag](const RSEntry &rq)
                                   { return rq.rob_tag == r_tag; });
            if (it != q.end())
                q.erase(it);

            inflight.pop_front();
            executing_info.erase(executing_info.begin());
        }
    }

    std::vector<BroadcastEvent> takeBroadcasts()
    {
        std::vector<BroadcastEvent> out = ready_broadcasts;
        ready_broadcasts.clear();
        return out;
    }
};
