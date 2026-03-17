# DV1655/6 Assignment 3 Starter Guide

This guide gives you a practical way to start Task 1 (IR) and then continue into Option 2 (C/C++ generation + Assembly generation).

It is written for your current project structure and coding style.

---

## 1. Goal and Scope

You already have:
- Lexer
- Parser
- AST
- Symbol Table
- Semantic Analyzer

Now you will add the backend in two phases:

1. Task 1: AST -> IR (TAC + Basic Blocks + CFG)
2. Option 2: IR -> generated C/C++ code, then IR -> x86-64 assembly

---

## 2. Recommended New Files

Start with these files:

- IR.h
- IRGenerator.h
- IRGenerator.cc
- CFGDotWriter.h (optional)
- CppCodeGen.h
- CppCodeGen.cc
- AsmCodeGen.h
- AsmCodeGen.cc

If you want fewer files, merge helpers into IRGenerator and codegen files.

---

## 3. Task 1: Build IR First

## 3.1 IR Data Structures

Use a simple three-address style first. You can evolve later.

```cpp
// IR.h
#ifndef IR_H
#define IR_H

#include <string>
#include <vector>
#include <map>

enum class IROp {
    Assign,
    Add, Sub, Mul, Div, Pow,
    CmpLT, CmpLE, CmpGT, CmpGE, CmpEQ, CmpNE,
    And, Or, Not,
    Label,
    Goto,
    IfFalseGoto,
    Param,
    Call,
    Return,
    Print,
    Read,
    ArrayLoad,
    ArrayStore,
    Length,
    NewObject
};

struct TAC {
    IROp op;
    std::string dst;
    std::string src1;
    std::string src2;
    std::string extra; // method name, class name, label, etc.
    int line = 0;
};

struct BasicBlock {
    int id = -1;
    std::string name;
    std::vector<TAC> code;
    std::vector<int> succ; // successor block ids
};

struct FunctionIR {
    std::string name;
    std::vector<std::string> params;
    std::vector<BasicBlock> blocks;
    int entryBlock = -1;
};

struct ProgramIR {
    std::map<std::string, FunctionIR> functions;
};

#endif
```

Why this shape works:
- TAC is flat and easy to print/debug.
- BasicBlock gives you CFG boundaries.
- FunctionIR isolates per-method control-flow.

## 3.2 IRGenerator Skeleton

```cpp
// IRGenerator.h
#ifndef IRGENERATOR_H
#define IRGENERATOR_H

#include "Node.h"
#include "IR.h"

class IRGenerator {
public:
    ProgramIR generate(Node* root);
    void writeCFGDot(const ProgramIR& ir, const std::string& filename);

private:
    int tempCounter = 0;
    int blockCounter = 0;

    FunctionIR* currentFunc = nullptr;
    BasicBlock* currentBlock = nullptr;

    std::string newTemp();
    int newBlock(const std::string& name);
    BasicBlock& blockById(int id);

    void genProgram(Node* root);
    void genClass(Node* classNode);
    void genMethod(Node* methodNode, const std::string& ownerClass);
    void genMain(Node* mainNode);

    void genStmt(Node* stmt);
    std::string genExpr(Node* expr);
};

#endif
```

## 3.3 Core Generation Rules

Use these deterministic rules:

- Expression nodes produce a temp name (for example t0, t1, t2).
- Statement nodes append TAC into current basic block.
- Control-flow statements create new blocks and edges.

### Example: expression

Input expression:

x + y * 2

Possible TAC:

- t0 = y * 2
- t1 = x + t0

### Example: if statement

Pseudo IR shape:

- entry block computes condition
- IfFalseGoto cond, elseBlock
- thenBlock ... Goto joinBlock
- elseBlock ... Goto joinBlock
- joinBlock continues

### Example: for loop

Build 4 blocks:

- init/cond block
- body block
- step block
- exit block

Edges:
- cond true -> body
- cond false -> exit
- body -> step
- step -> cond

## 3.4 AST Traversal Order

Recommended order:

1. Program
2. Classes
3. Methods
4. Main

Inside each method/main body:
- Walk statements in sequence
- Recursively generate expressions

Important:
- For this assignment stage, assume semantic analysis already validated types.
- Keep types only if needed by codegen later.

## 3.5 First Milestone

Get this working before Option 2:

- IR generated for valid/test1.cpm
- CFG exported to a dot file
- Dot graph shows blocks and edges
- TAC printed for each block

---

## 4. CFG .dot Output

Use one subgraph per function.

```cpp
void IRGenerator::writeCFGDot(const ProgramIR& ir, const std::string& filename) {
    std::ofstream out(filename);
    out << "digraph CFG {\n";
    out << "  node [shape=box, fontname=\"Courier\"];\n";

    for (const auto& kv : ir.functions) {
        const auto& fn = kv.second;
        out << "  subgraph cluster_" << fn.name << " {\n";
        out << "    label=\"" << fn.name << "\";\n";

        for (const auto& b : fn.blocks) {
            out << "    B" << b.id << " [label=\"" << b.name << "\\n";
            for (const auto& i : b.code) {
                out << "... format TAC as one line ...\\l";
            }
            out << "\"];\n";
        }

        for (const auto& b : fn.blocks) {
            for (int s : b.succ) {
                out << "    B" << b.id << " -> B" << s << ";\n";
            }
        }

        out << "  }\n";
    }

    out << "}\n";
}
```

---

## 5. Option 2 Start Guide

Option 2 has two outputs:

1. Generated C/C++ source from IR
2. Generated x86-64 assembly from IR

Use the same ProgramIR for both generators.

## 5.1 Phase A: IR -> C/C++ (do this first)

This is your low-risk starting point.

### Mapping examples

TAC:

- t0 = a + b

Generated C++ line:

- double t0 = a + b;

TAC conditional:

- ifFalse t2 goto B7

Generated C++ line:

- if (!t2) goto B7;

TAC jump:

- goto B9

Generated C++ line:

- goto B9;

### Emitter skeleton

```cpp
// CppCodeGen.cc
void emitFunction(const FunctionIR& fn, std::ostream& out) {
    out << "int " << fn.name << "(...) {\n";

    // You can predeclare temporaries if you track them in IRGenerator.
    // out << "  int t0, t1, t2;\n";

    for (const auto& b : fn.blocks) {
        out << "B" << b.id << ":\n";
        for (const auto& ins : b.code) {
            out << "  " << emitTacAsCpp(ins) << "\n";
        }
    }

    out << "}\n\n";
}
```

Tip:
- Start with int/float only.
- Add arrays and objects after primitive flow works.

## 5.2 Phase B: IR -> x86-64 Assembly

Do this after C++ generation is correct.

### Practical approach

1. Implement only test1.cpm features first:
- arithmetic
- assignments
- print
- return

2. Then add:
- comparisons and jumps
- if/for
- function calls

3. Then add:
- arrays
- object method calls

### Minimal instruction mapping idea

For integer add TAC:

- t2 = t0 + t1

Assembly concept:

- mov rax, [t0]
- add rax, [t1]
- mov [t2], rax

For conditional jump TAC:

- ifFalse t3 goto B8

Assembly concept:

- cmp qword [t3], 0
- je B8

You can begin with stack slots for every temp/variable before optimizing registers.

---

## 6. Suggested Development Order

1. Implement IR structures and TAC pretty-printer.
2. Generate IR for expressions and assignments only.
3. Add if and for block construction.
4. Export CFG dot.
5. Generate C++ from IR and validate on valid/test1.cpm, valid/test3.cpm.
6. Expand C++ generator to all valid tests.
7. Start assembly generator with test1.cpm subset.
8. Incrementally support more language constructs.

---

## 7. Example End-to-End (Small)

Source:

```txt
main(): int {
  volatile a : int := 2
  volatile b : int := 3
  volatile c : int := a + b
  print(c)
  return 0
}
```

Possible TAC:

```txt
B0:
  a = 2
  b = 3
  t0 = a + b
  c = t0
  print c
  return 0
```

Generated C++ sketch:

```cpp
int main() {
B0:
  int a = 2;
  int b = 3;
  int t0 = a + b;
  int c = t0;
  std::cout << c << std::endl;
  return 0;
}
```

Assembly sketch (conceptual):

```asm
B0:
  mov qword [a], 2
  mov qword [b], 3
  mov rax, [a]
  add rax, [b]
  mov [t0], rax
  ; print(c) handled via runtime helper or libc call
  mov rax, 0
  ret
```

---

## 8. Makefile Integration (when you start coding)

After creating .cc/.h files, update build line to include new sources, for example:

```make
compiler: lex.yy.c parser.tab.o main.cc IRGenerator.cc CppCodeGen.cc AsmCodeGen.cc
	g++ -g -w -ocompiler parser.tab.o lex.yy.c main.cc IRGenerator.cc CppCodeGen.cc AsmCodeGen.cc -std=c++14
```

You can also add helper targets:

```make
cfg:
	dot -Tpdf cfg.dot -ocfg.pdf
```

---

## 9. Common Pitfalls

- Mixing expression generation with block-jump logic too early.
- Forgetting to avoid revisiting CFG blocks during code emission.
- Generating temporaries without unique naming.
- Trying to do full assembly support before C++ codegen is stable.

---

## 10. What To Build Next (Immediate)

Immediate coding target for this week:

- Build IR.h + IRGenerator.h + IRGenerator.cc
- Support these constructs first:
  - literals
  - variable references
  - assignment
  - +, -, *, /
  - print
  - return
  - if
  - for
- Produce cfg.dot and inspect with Graphviz

Once this is stable, begin C++ code generation from the same IR.
