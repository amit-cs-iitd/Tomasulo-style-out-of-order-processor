#pragma once
#include "Basics.h"
#include <iostream>
#include <vector>
#include <unordered_map>

class BranchPredictor {
public:
    int total_branches = 0;
    int correct_predictions = 0;
    std::unordered_map<int, int> state_by_pc; // 0..3, default 0

    int predict(int current_pc, int imm, OpCode op) {
        if (op == OpCode::J) return current_pc + imm;
        int state = 0;
        auto it = state_by_pc.find(current_pc);
        if (it != state_by_pc.end()) state = it->second;
        bool predict_taken = (state <= 1);
        return predict_taken ? (current_pc + imm) : (current_pc + 1);
    }

    void update(int pc, int actual_target, bool taken, bool was_correct) {
        (void)actual_target;
        total_branches++;
        if (was_correct) {
            correct_predictions++;
        }

        int state = 0;
        auto it = state_by_pc.find(pc);
        if (it != state_by_pc.end()) state = it->second;

        if (taken) {
            if (state == 1) state = 0;
            else if (state == 2) state = 1;
            else if (state == 3) state = 2;
        } else {
            if (state == 0) state = 1;
            else if (state == 1) state = 2;
            else if (state == 2) state = 3;
        }
        state_by_pc[pc] = state;
    }
};
