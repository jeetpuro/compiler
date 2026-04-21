#ifndef CPP_CODEGEN_H
#define CPP_CODEGEN_H

#include "IR.h"
#include <string>
#include <fstream>
#include <set>
#include <stdio.h>


class CppCodeGen {
public:
    // Main entry point for generation
    void generate(const ProgramIR& ir, const std::string& outputFile);

private:
    std::ofstream out;
    std::set<std::string> declaredVariables;

    // Helper methods to generate different parts of the C++ file
    void genHeaders();
    void genFunction(const FunctionIR& func);
    void collectVariables(const FunctionIR& func);
    void genBlock(const BasicBlock& block);
    void genInstruction(const TAC& tac);
};

#endif