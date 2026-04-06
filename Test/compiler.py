import re
import sys
from typing import Dict, List


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].strip()


def tokenize(line: str) -> List[str]:
    return [t for t in re.split(r"[,\s()]+", line.strip()) if t]


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python3 compiler.py <filename.s>")  # inspired from main.cpp
        return 1

    path = sys.argv[1]
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    mem_labels: Dict[str, int] = {}
    code_labels: Dict[str, int] = {}
    cleaned: List[str] = []
    mem_ptr = 0
    pc = 0

    for raw in lines:
        line = strip_comment(raw)
        if not line:
            continue

        if line.startswith(".") and ":" in line:
            label, rhs = line.split(":", 1)
            name = label[1:].strip()
            vals = tokenize(rhs)
            mem_labels[name] = mem_ptr
            mem_ptr += len(vals)
            cleaned.append(line)
            continue

        rest = line
        while ":" in rest:
            left, right = rest.split(":", 1)
            maybe = left.strip()
            if maybe:
                code_labels[maybe] = pc
            rest = right.strip()
            if not rest:
                break
        if rest:
            cleaned.append(rest)
            pc += 1

    def repl_mem(line: str) -> str:
        for lbl, addr in mem_labels.items():
            line = re.sub(rf"\b{re.escape(lbl)}\b", str(addr), line)
        return line

    out: List[str] = []
    current_pc = 0
    for line in cleaned:
        line = repl_mem(line)
        toks = tokenize(line)
        if not toks:
            continue
        op = toks[0].lower()
        if op == "j" and len(toks) >= 2 and toks[1] in code_labels:
            target = code_labels[toks[1]]
            imm = target - current_pc
            out.append(f"j {imm}")
            current_pc += 1
            continue
        if (
            op in {"beq", "bne", "blt", "ble"}
            and len(toks) >= 4
            and toks[3] in code_labels
        ):
            target = code_labels[toks[3]]
            imm = target - current_pc
            out.append(f"{op} {toks[1]} {toks[2]} {imm}")
            current_pc += 1
            continue
        out.append(" ".join(toks))
        if not (line.startswith(".") and ":" in line):
            current_pc += 1

    with open(path, "w", encoding="utf-8") as f:
        for l in out:
            f.write(l + "\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
