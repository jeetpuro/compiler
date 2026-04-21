# Mini Lesson: Assignment 3 Option 2 - Code Generation (IR -> C++)

This is **Part 2** of the compiler backend lesson.

In Part 1, you successfully transformed the Abstract Syntax Tree (AST) into Three-Address Code (TAC) and organized it into Basic Blocks and Functions within a Control Flow Graph (CFG).

**Goal for this session:**
- Take the `ProgramIR` generated in Part 1.
- Traverse the IR and emit valid, compilable C++ code.
- Handle variable scope, mathematical operations, and explicit `goto` control flow in C++.
- Compile your generated C++ code with `g++` to create your final executable.

You will edit these files:
- `CppCodeGen.h`
- `CppCodeGen.cc`
- `main.cc`
- `Makefile`

---

## Part A: What You Are Building

Your compiler's final stage:

1. Lexer + parser build AST.
2. Semantic analyzer validates the AST.
3. IR generator builds TAC/CFG.
4. **Code Generator translates TAC to a `.cpp` text file.**
5. You compile the resulting `.cpp` file with `g++` (or `clang++`) to run the user's program.

Since TAC breaks down complex expressions into simple `t0 := a + b` operations and explicit `goto B3` branches, translating it to C++ is a direct 1-to-1 mapping!

---

## Part B: The Output Shape

If the user provides this CPM code:

```cpm
main(): int {
  volatile a : int := 2
  volatile b : int := 3
  volatile c : int := a + b
  print(c)
  return 0
}
```

Its TAC from Part 1 is:

```text
B0 (main_entry):
  a := 2
  b := 3
  t0 := a + b
  c := t0
  print c
  return 0
```

You are going to write a class that prints this exact C++ code (`output.cpp`):

```cpp
#include <iostream>
#include <cmath>

int main() {
  double a;
  double b;
  double t0;
  double c;

B0:
  a = 2;
  b = 3;
  t0 = a + b;
  c = t0;
  std::cout << c << std::endl;
  return 0;
}
```

Notice how every temporary (`t0`) and variable (`a`, `b`, `c`) is declared at the top of the function. C++ requires all variables to be declared before they are used.

---

## Part C: The CppCodeGen Skeleton (`CppCodeGen.h`)

Create or update `CppCodeGen.h`. You need a class that walks the `ProgramIR` and writes to an `std::ofstream`.

```cpp
#ifndef CPP_CODEGEN_H
#define CPP_CODEGEN_H

#include "IR.h"
#include <string>
#include <fstream>
#include <set>

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
```

### Why do we need `declaredVariables`?
TAC does not explicitly declare variables—it just uses them (`t0 := 4 * 2`). Because C++ is strictly typed, we must find every unique `.dst` string in our TAC instructions and emit a `double t0;` declaration at the top of the generated C++ function before we begin emitting the actual TAC logic.

---

## Part D: Implementing the Core Loop (`CppCodeGen.cc`)

Create or open `CppCodeGen.cc`. Add your includes:

```cpp
#include "CppCodeGen.h"
#include <iostream>

using namespace std;
```

### Step 1: `generate()` and `genHeaders()`

This is the entry point. Open the file, write `#include`s, and iterate through all TAC functions.

```cpp
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
    out << "#include <string>\n\n";
}
```

### Step 2: `genFunction()` and `collectVariables()`

When we enter a function, we must reset our declared variables, find all locals/temps by scanning every `TAC` instruction inside every `BasicBlock`, and then write the block logic.

```cpp
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
    for (size_t i = 0; i < func.params.size(); ++i) {
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
```

**Implementation Detail for `collectVariables()`:**
You need to write logic that populates `declaredVariables`:

```cpp
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
```

---

## Part E: Basic Blocks and Labels

Translating a basic block is straightforward. You write out the block's ID as a C++ label (like `B0:`), and then loop through its instructions.

```cpp
void CppCodeGen::genBlock(const BasicBlock& block) {
    out << "B" << block.id << ":\n";
    for (const TAC& tac : block.code) {
        genInstruction(tac);
    }
}
```

Why do we need `B0:` labels? Because TAC branch instructions map directly to C++ `goto` statements!

---

## Part F: Translating Individual Instructions (`genInstruction`)

This is the heart of code generation. Every `IROp` maps to a C++ equivalent.

```cpp
void CppCodeGen::genInstruction(const TAC& tac) {
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
```

### Important Operations you MUST add:
You should extend this switch statement to handle *all* operators defined in `IR.h` that we added in Part 1 (such as `CmpLE`, `CmpGT`, `CmpNE`, `And`, `Or`, `Not`, `ArrayLoad`, etc). 
* `ArrayLoad`: `out << tac.dst << " = " << tac.src1 << "[" << tac.src2 << "];\n";`
* `Not`: `out << tac.dst << " = !" << tac.src1 << ";\n";`

---

## Part G: Wiring it up in `main.cc`

Now we just plug the C++ Code Generator into the end of our compilation pipeline.

Open your `main.cc`. Locate where you trigger `IRGenerator` (right after semantics). Change it to this:

```cpp
#include "CppCodeGen.h" // Add this at the top!

// [... inside main after semantic pass success ...]
    
    // 1. Generate IR
    IRGenerator ir;
    ProgramIR pir = ir.generate(root);
    ir.writeCFGDot(pir, "cfg.dot");
    
    // 2. Generate C++ Executable Code
    std::string outFileName = "output.cpp";
    CppCodeGen cpg;
    cpg.generate(pir, outFileName);

    std::cout << "C++ Source successfully generated at: " << outFileName << std::endl;
```

---

## Part H: Building and Testing Output (`Makefile`)

You need your compiler to compile the `CppCodeGen.cc` file. 

### Modify your Makefile:

Find your compiler target line and add `CppCodeGen.cc` to the source list:

```makefile
compiler: lex.yy.c parser.tab.o main.cc IRGenerator.cc CppCodeGen.cc
	g++ -g -w -ocompiler parser.tab.o lex.yy.c main.cc IRGenerator.cc CppCodeGen.cc -std=c++14
```

Then add an automatic `run` target that builds your compiler, runs it on a target file, and immediately compiles the output generated `output.cpp`.

```makefile
run_codegen: compiler
	./compiler valid/test1.cpm
	g++ output.cpp -o program_exec
	./program_exec
```

---

## Part I: Execution Trace (Debug Workflow)

Here is what exactly happens when you test your compiler on `test2.cpm`:

1. Commands: 
   ```bash
   make
   ./compiler valid/test2.cpm
   ```
2. **Terminal Output:** "C++ Source successfully generated at: output.cpp"
3. Open `output.cpp`. You should see `t0` variables initialized at the top, a `B0:` label, and `print` translated perfectly into `std::cout << ...`.
4. Command:
   ```bash
   g++ output.cpp -o my_program
   ./my_program
   ```
5. You should see actual numbers printed out by the terminal natively! Your custom program language has officially run natively on your machine!

### Common Bugs to look out for:
1. **Error: "t0 previously declared here"**
   - *Fix:* Ensure `declaredVariables` is utilizing a `std::set`. This ensures variables only print `double t0;` exactly once per function regardless of how many times they appear in TAC.
2. **Error: "undefined variable '2'"**
   - *Fix:* TAC often records strings and integers natively (`src1 = "2"`). Ensure the `collectVariables` loop skips pure numbers (`if(!isdigit(tac.dst[0]))`) when populating the variables!
3. **Error: "std::pow was not declared"**
   - *Fix:* Ensure you printed `#include <cmath>` inside `genHeaders()`!