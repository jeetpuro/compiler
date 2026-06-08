#include "CppCodeGen.h"
#include <iostream>

using namespace std;


void CppCodeGen::generate(const ProgramIR& ir, const string& outputFile) {
    out.open(outputFile);
    if (!out.is_open()) {
        cerr << "Error: Could not open output file " << outputFile << endl;
        return;
    }

    genHeaders();

    // Identify functions that return void (no Return with a value)
    voidFunctions.clear();
    for (const auto& kv : ir.functions) {
        if (kv.first == "main") continue;
        bool hasValueReturn = false;
        for (const BasicBlock& block : kv.second.blocks)
            for (const TAC& tac : block.code)
                if (tac.op == IROp::Return && !tac.src1.empty()) hasValueReturn = true;
        if (!hasValueReturn) voidFunctions.insert(kv.first);
    }

    // Collect and emit class fields as globals (accessible across all methods)
    classFieldVars.clear();
    classArrayFieldVars.clear();
    for (const auto& kv : ir.classFields) {
        for (const string& f : kv.second) classFieldVars.insert(f);
    }
    for (const auto& kv : ir.classArrayFields) {
        for (const string& f : kv.second) classArrayFieldVars.insert(f);
    }
    for (const string& f : classFieldVars) {
        out << "double " << f << " = 0;\n";
    }
    for (const string& f : classArrayFieldVars) {
        out << "std::vector<double> " << f << ";\n";
    }
    if (!classFieldVars.empty() || !classArrayFieldVars.empty()) out << "\n";

    // Iterate through all functions in the IR
    for (const auto& kv : ir.functions) {
        genFunction(kv.second);
    }

    out.close();
}

void CppCodeGen::genHeaders() {
    out << "// --- Generated C++ Source Code ---" << "\n";
    out << "#include <iostream>\n";
    out << "#include <cmath>\n";
    out << "#include <string>\n\n"; // TODO lägga till headers
    out << "#include <vector>\n";
}


void CppCodeGen::genFunction(const FunctionIR& func) {
    // Pre-scan: which params are array types (used in array ops)?
    set<string> paramSet(func.params.begin(), func.params.end());
    set<string> arrayParams;
    bool hasValueReturn = false;
    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            if ((tac.op == IROp::ArrayLoad || tac.op == IROp::Length) && paramSet.count(tac.src1))
                arrayParams.insert(tac.src1);
            if (tac.op == IROp::ArrayStore && paramSet.count(tac.dst))
                arrayParams.insert(tac.dst);
            if (tac.op == IROp::Return && !tac.src1.empty())
                hasValueReturn = true;
        }
    }

    // Determine return type and emit signature
    if (func.name == "main") {
        out << "int main(";
    } else if (hasValueReturn) {
        out << "double " << func.name << "(";
    } else {
        out << "void " << func.name << "(";
    }

    for (size_t i = 0; i < func.params.size(); ++i) {
        if (arrayParams.count(func.params[i]))
            out << "std::vector<double> " << func.params[i];
        else
            out << "double " << func.params[i];
        if (i < func.params.size() - 1) out << ", ";
    }
    out << ") {\n";

    // 1. Scan and declare all variables at the top
    callArgs.clear();

    // Find temps/vars that hold object references — useless in the flat model
    objectVars.clear();
    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            if (tac.op == IROp::NewObject && !tac.dst.empty())
                objectVars.insert(tac.dst);
        }
    }
    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            if (tac.op == IROp::Assign && objectVars.count(tac.src1))
                objectVars.insert(tac.dst);
        }
    }

    collectVariables(func);

    // Write out the declarations
    for (const string& var : declaredVariables) {
        // Only declare valid identifiers (avoid declaring plain numbers like "2")
        if (!var.empty() && !isdigit(var[0]) && var[0] != '"') {
            out << "    double " << var << ";\n";
        }
    }
    for (const string& var : arrayVariables) {
        out << "    std::vector<double> " << var << ";\n";
    }

    out << "\n";

    // 2. Iterate through and emit every basic block
    for (const BasicBlock& block : func.blocks) {
        genBlock(block);
    }

    out << "}\n\n";
}


void CppCodeGen::collectVariables(const FunctionIR& func) {
    declaredVariables.clear();
    arrayVariables.clear();
    set<string> paramSet;

    for (const string& p : func.params) {
        paramSet.insert(p);  // track params but don't declare them again
    }

    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            if (!tac.dst.empty() && !paramSet.count(tac.dst)
                && !classFieldVars.count(tac.dst) && !classArrayFieldVars.count(tac.dst)
                && !objectVars.count(tac.dst)) {
                if (tac.op == IROp::NewArray) {
                    arrayVariables.insert(tac.dst);
                } else if (tac.op == IROp::Assign && arrayVariables.count(tac.src1)) {
                    arrayVariables.insert(tac.dst);
                } else if (!arrayVariables.count(tac.dst)) {
                    declaredVariables.insert(tac.dst);
                }
            }
        }
    }
}


void CppCodeGen::genBlock(const BasicBlock& block) {
    if (block.code.empty()) return;
    out << "B" << block.id << ":\n";
    for (const TAC& tac : block.code) {
        genInstruction(tac);
    }
}

void CppCodeGen::genInstruction(const TAC& tac) {
    // Handle silent instructions before emitting indent
    if (tac.op == IROp::NewObject) return;
    if (tac.op == IROp::Assign && objectVars.count(tac.src1)) return;
    if (tac.op == IROp::Param) { callArgs.push_back(tac.src1); return; }

    out << "    "; // Indent inside the block

    switch (tac.op) {
        case IROp::Assign:
            out << tac.dst << " = " << tac.src1 << ";\n";
            break;

        case IROp::Add:
            out << tac.dst << " = " << tac.src1 << " + " << tac.src2 << ";\n";
            break;
        case IROp::Sub:
            out << tac.dst << " = " << tac.src1 << " - " << tac.src2 << ";\n";
            break;
        case IROp::Mul:
            out << tac.dst << " = " << tac.src1 << " * " << tac.src2 << ";\n";
            break;
        case IROp::Div:
            out << tac.dst << " = " << tac.src1 << " / " << tac.src2 << ";\n";
            break;
        case IROp::Pow:
            // Use C++ math library std::pow
            out << tac.dst << " = std::pow(" << tac.src1 << ", " << tac.src2 << ");\n";
            break;

         // Logical Operations
        case IROp::CmpLT:
            out << tac.dst << " = (" << tac.src1 << " < " << tac.src2 << ");\n";
            break;
        case IROp::CmpEQ:
            out << tac.dst << " = (" << tac.src1 << " == " << tac.src2 << ");\n";
            break;

        case IROp::CmpLE:
            out << tac.dst << " = (" << tac.src1 << " <= " << tac.src2 << ");\n";
            break;

        case IROp::CmpGT:
            out << tac.dst << " = (" << tac.src1 << " > " << tac.src2 << ");\n";
            break;            

        case IROp::CmpGE:
            out << tac.dst << " = (" << tac.src1 << " >= " << tac.src2 << ");\n";
            break;            
            
        case IROp::CmpNE:
            out << tac.dst << " = (" << tac.src1 << " != " << tac.src2 << ");\n";
            break;
            
        case IROp::And:
            out << tac.dst << " = (" << tac.src1 << " && " << tac.src2 << ");\n";
            break;

        case IROp::Or:
            out << tac.dst << " = (" << tac.src1 << " || " << tac.src2 << ");\n";
            break;

        case IROp::Not:
            out << tac.dst << " = !" << tac.src1 << ";\n";
            break;


        //function calls

        case IROp::Call: {
            int argCount = stoi(tac.src2);
            int startIdx = (int)callArgs.size() - argCount;

            if (voidFunctions.count(tac.extra))
                out << tac.extra << "(";
            else
                out << tac.dst << " = " << tac.extra << "(";

            for (int i = startIdx; i < (int)callArgs.size(); ++i) {
                if (i > startIdx) out << ", ";
                out << callArgs[i];
            }
            out << ");\n";

            callArgs.erase(callArgs.begin() + startIdx, callArgs.end());
            break;
        }

        // Arrays
        case IROp::NewArray:
            out << tac.dst << " = {" << tac.extra << "};\n";
            break;
        
        case IROp::ArrayLoad:
            out << tac.dst << " = " << tac.src1 << "[static_cast<int>(" << tac.src2 << ")];\n";
            break;

        case IROp::ArrayStore:
            out << tac.dst << "[static_cast<int>(" << tac.src1 << ")] = " << tac.src2 << ";\n";
            break;

        case IROp::Length:
            out << tac.dst << " = " << tac.src1 << ".size();\n";
            break;


        // I/O Operations
        case IROp::Print:
            out << "std::cout << " << tac.src1 << " << std::endl;\n";
            break;
        case IROp::Read:
            out << "std::cin >> " << tac.dst << ";\n";
            break;

        // Control Flow
        case IROp::Goto:
            out << "goto B" << tac.extra << ";\n";
            break;
        case IROp::IfFalseGoto:
            // If the condition is false (0), jump to the branch target
            out << "if (!" << tac.src1 << ") goto B" << tac.extra << ";\n";
            break;
        
        // Functions and Return
        case IROp::Return:
            if (tac.src1.empty()) {
                out << "return;\n";
            } else {
                out << "return " << tac.src1 << ";\n";
            }
            break;

        default:
            out << "// ToDo: Implement CppCodeGen for IROp code " << static_cast<int>(tac.op) << "\n";
            break;
    }
}





