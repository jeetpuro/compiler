#include "AsmCodeGen.h"
#include <cctype>
#include <iostream>
#include <set>

using namespace std;

// ── helpers ──────────────────────────────────────────────────────────────────

bool AsmCodeGen::isNumeric(const string& s) const {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-') ++i;
    if (i == s.size()) return false;
    bool dot = false;
    for (; i < s.size(); ++i) {
        if (s[i] == '.') { if (dot) return false; dot = true; }
        else if (!isdigit(s[i])) return false;
    }
    return true;
}

string AsmCodeGen::getConstLabel(const string& val) {
    auto it = constLabel.find(val);
    if (it != constLabel.end()) return it->second;
    string lbl = ".LC" + to_string(nextConst++);
    constLabel[val] = lbl;
    return lbl;
}

// ── constant collection ───────────────────────────────────────────────────────

void AsmCodeGen::collectConstants(const ProgramIR& ir) {
    constLabel.clear();
    nextConst = 0;
    // Pre-map boolean literals
    getConstLabel("1");   // true
    getConstLabel("0");   // false / return 0
    for (const auto& kv : ir.functions) {
        for (const BasicBlock& block : kv.second.blocks) {
            for (const TAC& tac : block.code) {
                if (isNumeric(tac.src1)) getConstLabel(tac.src1);
                if (isNumeric(tac.src2)) getConstLabel(tac.src2);
            }
        }
    }
}

void AsmCodeGen::emitRodata() {
    out << "    .section .rodata\n";
    for (const auto& kv : constLabel) {
        out << kv.second << ":\n";
        out << "    .double " << kv.first << "\n";
    }
    out << ".fmt:\n";
    out << "    .string \"%g\\n\"\n";
    out << "\n";
}

// ── variable → stack offset ───────────────────────────────────────────────────

void AsmCodeGen::collectOffsets(const FunctionIR& func) {
    varOffset.clear();
    set<string> seen;

    // Parameters occupy the first slots
    for (const string& p : func.params) {
        if (seen.count(p)) continue;
        seen.insert(p);
        int off = -(int)(seen.size() * 8);
        varOffset[p] = off;
    }

    // Then every destination that appears in the body
    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            if (tac.dst.empty() || seen.count(tac.dst)) continue;
            if (isNumeric(tac.dst)) continue;
            if (tac.dst == "true" || tac.dst == "false") continue;
            seen.insert(tac.dst);
            varOffset[tac.dst] = -(int)(seen.size() * 8);
        }
    }

    // Stack frame: round total bytes up to 16-byte alignment
    int total = (int)seen.size() * 8;
    stackBytes = (total + 15) & ~15;
    if (stackBytes == 0) stackBytes = 16;
}

// ── load / store helpers ──────────────────────────────────────────────────────

void AsmCodeGen::loadXmm(int reg, const string& src) {
    string xmm = "%xmm" + to_string(reg);
    if (src == "true") {
        out << "    movsd   " << constLabel.at("1") << "(%rip), " << xmm << "\n";
    } else if (src == "false") {
        out << "    movsd   " << constLabel.at("0") << "(%rip), " << xmm << "\n";
    } else if (isNumeric(src)) {
        out << "    movsd   " << constLabel.at(src) << "(%rip), " << xmm << "\n";
    } else {
        out << "    movsd   " << varOffset.at(src) << "(%rbp), " << xmm << "\n";
    }
}

void AsmCodeGen::storeXmm0(const string& dst) {
    out << "    movsd   %xmm0, " << varOffset.at(dst) << "(%rbp)\n";
}

// ── instruction emitter ───────────────────────────────────────────────────────

// Emit the memory operand (const label or stack slot) for src
static string memOperand(const string& src,
                         const map<string, string>& cLabel,
                         const map<string, int>& vOff,
                         bool numeric) {
    if (numeric) return cLabel.at(src) + "(%rip)";
    return to_string(vOff.at(src)) + "(%rbp)";
}

void AsmCodeGen::genInstruction(const TAC& tac) {
    switch (tac.op) {

        // ── arithmetic ────────────────────────────────────────────────────────
        case IROp::Add:
        case IROp::Sub:
        case IROp::Mul:
        case IROp::Div: {
            loadXmm(0, tac.src1);
            // Second operand can be memory directly (SSE allows mem source)
            string src2mem = memOperand(tac.src2, constLabel, varOffset, isNumeric(tac.src2));
            if      (tac.op == IROp::Add) out << "    addsd   " << src2mem << ", %xmm0\n";
            else if (tac.op == IROp::Sub) out << "    subsd   " << src2mem << ", %xmm0\n";
            else if (tac.op == IROp::Mul) out << "    mulsd   " << src2mem << ", %xmm0\n";
            else                          out << "    divsd   " << src2mem << ", %xmm0\n";
            storeXmm0(tac.dst);
            break;
        }

        case IROp::Pow:
            loadXmm(0, tac.src1);   // first arg  → %xmm0
            loadXmm(1, tac.src2);   // second arg → %xmm1
            out << "    call    pow@PLT\n";
            storeXmm0(tac.dst);     // result in %xmm0
            break;

        // ── assignment ────────────────────────────────────────────────────────
        case IROp::Assign:
            loadXmm(0, tac.src1);
            storeXmm0(tac.dst);
            break;
            
        // ── control flow ──────────────────────────────────────────────────────
        case IROp::Goto:
            out << "    jmp     B" << tac.extra << "\n";
            break;

        // ── I/O ───────────────────────────────────────────────────────────────
        case IROp::Print:
            out << "    leaq    .fmt(%rip), %rdi\n";
            loadXmm(0, tac.src1);
            out << "    movb    $1, %al\n";
            out << "    call    printf@PLT\n";
            break;

        // ── return ────────────────────────────────────────────────────────────
        case IROp::Return:
            if (tac.src1.empty() || tac.src1 == "0") {
                out << "    xorl    %eax, %eax\n";
            } else if (isNumeric(tac.src1)) {
                loadXmm(0, tac.src1);
                // non-main returning double: result already in %xmm0
            } else {
                loadXmm(0, tac.src1);
            }
            out << "    leave\n";
            out << "    ret\n";
            break;

        default:
            out << "    # TODO: IROp " << static_cast<int>(tac.op) << "\n";
            break;
    }
}

// ── function emitter ──────────────────────────────────────────────────────────

void AsmCodeGen::genFunction(const FunctionIR& func) {
    collectOffsets(func);

    out << "    .globl  " << func.name << "\n";
    out << "    .type   " << func.name << ", @function\n";
    out << func.name << ":\n";

    // Prologue
    out << "    pushq   %rbp\n";
    out << "    movq    %rsp, %rbp\n";
    if (stackBytes > 0)
        out << "    subq    $" << stackBytes << ", %rsp\n";

    // Store incoming register parameters onto the stack
    // System V AMD64 ABI: integer/pointer args in rdi,rsi,rdx,rcx,r8,r9
    //                     float args in xmm0..xmm7
    // For simplicity we assume all params are doubles (passed in xmm registers)
    static const char* xmmRegs[] = {"%xmm0","%xmm1","%xmm2","%xmm3",
                                    "%xmm4","%xmm5","%xmm6","%xmm7"};
    for (size_t i = 0; i < func.params.size() && i < 8; ++i) {
        out << "    movsd   " << xmmRegs[i] << ", "
            << varOffset.at(func.params[i]) << "(%rbp)\n";
    }

    // Basic blocks
    for (const BasicBlock& block : func.blocks) {
        if (block.code.empty()) continue;
        out << "B" << block.id << ":\n";
        for (const TAC& tac : block.code) {
            genInstruction(tac);
        }
    }

    // Ensure there's a return at the end (for void/missing return)
    out << "    leave\n";
    out << "    ret\n";
    out << "    .size   " << func.name << ", .-" << func.name << "\n\n";
}

// ── entry point ───────────────────────────────────────────────────────────────

void AsmCodeGen::generate(const ProgramIR& ir, const string& outputFile) {
    out.open(outputFile);
    if (!out.is_open()) {
        cerr << "Error: could not open " << outputFile << "\n";
        return;
    }

    collectConstants(ir);
    emitRodata();

    out << "    .text\n\n";
    for (const auto& kv : ir.functions) {
        genFunction(kv.second);
    }

    out.close();
}
