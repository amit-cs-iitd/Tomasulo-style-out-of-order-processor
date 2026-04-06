#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <deque>
#include "Basics.h"

class LoadStoreQueue {
public:
    // LSQ reservation station
    int latency;
    int capacity = 0;
    std::deque<RSEntry> q;
    std::deque<std::pair<int, BroadcastEvent>> inflight;
    std::vector<BroadcastEvent> ready_broadcasts;
    std::vector<RSEntry> executing_info;
    
    bool has_result = false; // result flag
    bool has_exception = false; // exception flag
    int store_data = 0;

    LoadStoreQueue() = default;
    LoadStoreQueue(int lat, int cap) : latency(lat), capacity(cap) {}

    bool hasSpace() const { return static_cast<int>(q.size()) < capacity; }
    int rsSize() const { return static_cast<int>(q.size()); }

    void enqueue(const RSEntry& entry) { q.push_back(entry); }

    void capture(int tag, int val) {
        for (auto& e : q) {
            if (!e.src1_ready && e.src1_tag == tag) {
                e.src1_ready = true;
                e.src1_value = val;
            }
            if (!e.src2_ready && e.src2_tag == tag) {
                e.src2_ready = true;
                e.src2_value = val;
            }
        }
    }

    void executeCycle(std::vector<int>& Memory) {
        has_result = false;
        has_exception = false;
        ready_broadcasts.clear();

        for (auto& p : inflight) p.first--;
        while (!inflight.empty() && inflight.front().first <= 0) {
            BroadcastEvent out = inflight.front().second;
            const RSEntry& origin = executing_info.front();
            if (out.has_memory && out.mem_address >= 0 &&
                out.mem_address < static_cast<int>(Memory.size()) &&
                origin.is_load) {
                out.value = Memory[out.mem_address];
            }
            ready_broadcasts.push_back(out);
            has_result = true;
            has_exception = has_exception || out.has_exception;
            inflight.pop_front();
            executing_info.erase(executing_info.begin());
        }

        if (!q.empty() && inflight.empty()) {
            RSEntry e = q.front();
            if (e.src1_ready && e.src2_ready) {
                q.pop_front();

                BroadcastEvent out;
                out.valid = true;
                out.rob_tag = e.rob_tag;
                out.has_memory = true;

                int addr = e.src1_value + e.imm;
                out.mem_address = addr;
                if (addr < 0 || addr >= static_cast<int>(Memory.size())) {
                    out.has_exception = true;
                } else if (e.is_store) {
                    out.store_value = e.src2_value;
                    store_data = e.src2_value;
                }

                inflight.push_back({latency, out});
                executing_info.push_back(e);
            }
        }
    }

    std::vector<BroadcastEvent> takeBroadcasts() {
        std::vector<BroadcastEvent> out = ready_broadcasts;
        ready_broadcasts.clear();
        return out;
    }
};
