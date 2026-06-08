#ifndef ASM_CODEGEN_H
#define ASM_CODEGEN_H

#include "IR.h"
#include <fstream>
#include <map>
#include <string>

class AsmCodeGen {
public:
    void generate(const ProgramIR& ir, const std::string& outputFile);

private:
    std::ofstream out;
    std::map<std::string, int> varOffset;           // variable name -> stack offset (negative, from %rbp)
    std::map<std::string, std::string> constLabel;  // numeric string -> .rodata label
    int nextConst = 0;
    int stackBytes = 0;

    bool isNumeric(const std::string& s) const;
    std::string getConstLabel(const std::string& val);

    void collectConstants(const ProgramIR& ir);
    void emitRodata();
    void genFunction(const FunctionIR& func);
    void collectOffsets(const FunctionIR& func);

    // Load src (variable or constant) into %xmm<reg>
    void loadXmm(int reg, const std::string& src);
    // Store %xmm0 to dst variable
    void storeXmm0(const std::string& dst);

    void genInstruction(const TAC& tac);
};

#endif
