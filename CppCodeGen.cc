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
}


void CppCodeGen::genFunction(const FunctionIR& func) {
    // Determine the C++ function signature. 
    // CPM's main() becomes C++ int main()
    if (func.name == "main") {
        out << "int main(";
    } else {
        // For standard methods (assuming returning double for simplicity)
        out << "double " << func.name << "(";
    }

    // Add parameters (assuming all are doubles)
    for (size_t i = 0; i < func.params.size(); ++i) { //TODO: main ska inte ha  parameters
        out << "double " << func.params[i];
        if (i < func.params.size() - 1) out << ", ";
    }
    out << ") {\n";

    // 1. Scan and declare all variables at the top
    collectVariables(func);

    // Write out the declarations
    for (const string& var : declaredVariables) {
        // Only declare valid identifiers (avoid declaring plain numbers like "2")
        if (!var.empty() && !isdigit(var[0]) && var[0] != '"') {
            out << "    double " << var << ";\n";
        }
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
    // Add parameters to the set so they don't get re-declared locally
    for (const string& p : func.params) {
        declaredVariables.insert(p);
    }

    for (const BasicBlock& block : func.blocks) {
        for (const TAC& tac : block.code) {
            // The destination of any operation is a variable or temp we need to declare.
            if (!tac.dst.empty()) {
                declaredVariables.insert(tac.dst);
            }
        }
    }
}


void CppCodeGen::genBlock(const BasicBlock& block) {
    out << "B" << block.id << ":\n";
    for (const TAC& tac : block.code) {
        genInstruction(tac);
    }
}

void CppCodeGen::genInstruction(const TAC& tac) { // TODO: lägga till operations t.ex CmpLE
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

        // Conditions
        case IROp::CmpLT:
            out << tac.dst << " = (" << tac.src1 << " < " << tac.src2 << ");\n";
            break;
        case IROp::CmpEQ:
            out << tac.dst << " = (" << tac.src1 << " == " << tac.src2 << ");\n";
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





