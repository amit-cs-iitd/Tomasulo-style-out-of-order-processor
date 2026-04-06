# COL216 Assn2 Solution Notes

This solution implements a Tomasulo-style out-of-order processor with:

- per-unit reservation stations
- register alias table (RAT)
- reorder buffer (ROB)
- sequential load/store queue (LSQ)
- per-PC 2-bit branch predictor
- precise exceptions via in-order commit

## 1) Implementations

- `Processor.h`
  - full program loader/parser
  - pipeline stages:
    - `stageFetch`
    - `stageDecode`
    - `stageExecuteAndBroadcast`
    - `stageCommit`
  - RAT + ROB allocation/retirement
  - CDB-style multi-broadcast handling
  - branch mispredict flush + recovery PC
  - precise exception handling
  - halt condition when no more work is in-flight

- `ExecutionUnit.h`
  - pipelined execution units with configurable latency
  - separate RS per unit type
  - arithmetic/logic/branch computation
  - overflow detection for arithmetic ops
  - div/rem by zero exception

- `LoadStoreQueue.h`
  - sequential memory execution order
  - own latency pipeline
  - out-of-bounds memory exception
  - load values read at completion time
  - stores commit only in Commit stage

- `BranchPredictor.h`
  - per-instruction (per-PC) 2-bit saturating FSM
  - state 0 start, taken prediction for states 0/1
  - transition table exactly as given
  - prediction stats update at commit

- `compiler.py`
  - preprocessing for `make run FILE=...`
  - label resolution for branches/jumps
  - memory symbol replacement like `A(x1) -> <addr>(x1)`
  - writes preprocessed assembly back into same file

## 2) Stage-by-stage behavior

- Fetch:
  - fetches instruction at current `pc`
  - predicts next pc for branch/jump
  - stores fetched bundle for Decode

- Decode:
  - stalls if ROB full or target RS/LSQ full
  - allocates ROB for every instruction
  - allocates RS/LSQ entries except `j` (resolved as control in ROB directly)
  - sources operands from ARF/RAT/ready ROB value
  - updates RAT for register-writing instructions

- Execute + Broadcast:
  - each unit/LSQ advances pipelines
  - oldest ready RS entry starts execution each cycle
  - completed results broadcast to:
    - ROB (set ready/value/exception/control info)
    - all RS/LSQ operand waiters via tag capture

- Commit:
  - strictly in order, only ROB head
  - applies ARF writes and store-to-memory here only
  - updates branch predictor for conditional branches
  - on branch mispredict: flush younger pipeline state, set recovery PC
  - on exception: set `exception=true`, set `pc=faulting_pc`, flush, halt

## 3) Exception model

- Arithmetic overflow for `add/sub/addi/mul`
- `div/rem` by zero
- memory out-of-bounds in `lw/sw`

All exceptions are generated on execution completion, stored in ROB, and only become architectural at commit. This guarantees precise exceptions.

## 4) Important correctness points followed

- `x0` forced to 0 always
- architectural state updates only in commit
- all instructions allocate ROB entries
- LSQ executes memory ops in issue order
- branch predictor updated only at commit
- halt only when:
  - program is out of fetch range
  - and no fetched/decode/ROB/RS/LSQ in-flight work exists

## 5) Files included for submission

This directory includes:

- `Basics.h`
- `BranchPredictor.h`
- `ExecutionUnit.h`
- `LoadStoreQueue.h`
- `Processor.h`
- `Makefile`
- `compiler.py`
- `main.cpp`
- `README.md`
- `Solution.md`

## 6) Build and run

- Compile:

```bash
make compile FILE=<filename.cpp>
```

- Preprocess input assembly:

```bash
make run FILE=<filename.s>
```

- Execute:

```bash
./main <filename.s> [additional_args...]
```

## 7) Verification

The implementation was compiled and run on provided sample programs (`code1` to `code5`) and the output was as expected (as given in `ans1` to `ans5` respectively)

## 8) Formatting:

- Formatted `.h` files using default VSCode formatter extension: `C/C++`
- Formatted `compiler.py` using `Black` 
- Formatted `Solution.md` using `Prettier`
- `Makefile`, `README.md` and `main.cpp` are left untouched as given on `GitHub`