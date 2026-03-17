# Mini Lesson: Assignment 3 Option 2 - First Live Coding Step (AST -> IR)

This lesson is the IR version of your semantic mini lesson.

Goal for this first session:
- Build IR data structures (TAC + BasicBlock + FunctionIR)
- Traverse AST and generate IR for a working subset
- Export CFG as cfg.dot (and cfg.pdf with make cfg)

You will edit these files:
- IR.h
- IRGenerator.h
- IRGenerator.cc
- CFGDotWriter.h
- main.cc
- Makefile

---

## Part A - What We Are Building

Pipeline for this lab:

1. Parser builds AST (already done)
2. Semantic analyzer validates program (already done)
3. IR generator traverses AST and emits TAC inside basic blocks (this lesson)
4. CFG writer emits cfg.dot from IR (this lesson)
5. Next lesson: traverse IR and generate C/C++

For now, assume input is semantically correct.

---

## Part B - First Test Program

Save this as test_lesson.txt:

```cpm
main(): int {
  volatile a : int := 2
  volatile b : int := 3
  volatile c : int := a + b
  print(c)
  return 0
}
```

Run:

```bash
./compiler test_lesson.txt
```

AST should include nodes like:
- MainStatement
- Statements
- VarDecl:a
- VarDecl:b
- VarDecl:c
- AddExpression
- PrintStatement
- ReturnStatement

These node types are what your IR traversal will match.

---

## Part C - IR Data Structures (IR.h)

Implement these core structures:

- IROp enum
- TAC struct
- BasicBlock struct
- FunctionIR struct
- ProgramIR struct

Minimum ops to support this first lesson:
- Assign
- Add, Sub, Mul, Div
- CmpLT, CmpLE, CmpGT, CmpGE, CmpEQ, CmpNE
- Goto
- IfFalseGoto
- Return
- Print
- Read
- Call
- Param
- ArrayLoad
- ArrayStore
- Length
- NewObject
- NewArray

TAC should store:
- op
- dst
- src1
- src2
- extra
- line

BasicBlock should store:
- id
- name
- code (vector<TAC>)
- succ (vector<int>)

Tip: add a helper addSuccessor(id) that avoids duplicate edges.

---

## Part D - IR Generator Skeleton (IRGenerator.h)

Add these fields:
- ProgramIR program
- int tempCounter
- int blockCounter
- FunctionIR* currentFunc
- int currentBlockId

Add these key methods:
- ProgramIR generate(Node* root)
- void genProgram(Node* root)
- void genMain(Node* mainNode)
- void genStmt(Node* stmt)
- string genExpr(Node* expr)
- int newBlock(const string& name)
- string newTemp()
- void emit(...)
- void writeCFGDot(...)

Contract:
- genExpr returns the name of a value (literal, id, or temp)
- genStmt emits TAC and may create new blocks

---

## Part E - Implement genExpr First (IRGenerator.cc)

Start with these cases:

1. Atom nodes:
- ID -> return node->value
- Int / Float / Bool -> return node->value
- String -> return quoted literal

2. Binary operators:
- AddExpression, SubExpression, MultExpression, DivExpression
- Also comparison/logical ops if available

Pattern:
1. left = genExpr(child0)
2. right = genExpr(child1)
3. t = newTemp()
4. emit(op, t, left, right)
5. return t

Example for a + b:
- t0 = a + b

3. Negation:
- t = !x

4. Array access:
- t = arr[i]

5. Length:
- t = arr.length

---

## Part F - Implement genStmt (Working Subset)

Add statement handlers in this order:

1. Statements container
- Recurse all children

2. VarDecl
- If it has initializer, generate rhs and emit assignment
- Example: c := a + b
  - t0 := a + b
  - c := t0

3. AssignmentStatement
- lhs ID: emit Assign(lhs, rhs)
- lhs ArrayExperssion: emit ArrayStore(base, idx, rhs)

4. PrintStatement
- emit Print(value)

5. readStatement
- emit Read(targetId)

6. ReturnStatement
- emit Return(value)

At this point test_lesson.txt should produce one block with TAC lines.

---

## Part G - Add Control Flow Blocks

### IfStatement

Create:
- then block
- join block

From current block:
- emit IfFalseGoto(cond, join)
- true edge -> then
- false edge -> join

In then block:
- emit body TAC
- if not terminated, emit goto join

Continue in join block.

### IfElseStatement

Create:
- then block
- else block
- join block

From current block:
- emit IfFalseGoto(cond, else)
- true edge -> then
- false edge -> else

Then and else both end in goto join unless already terminated.

### ForStatement

Use 4 blocks:
- for_cond
- for_body
- for_step
- for_exit

Flow:
1. Emit init in current block
2. goto for_cond
3. in for_cond: evaluate condition
4. IfFalseGoto(cond, for_exit)
5. true edge -> for_body
6. body -> for_step
7. step -> for_cond
8. continue at for_exit

---

## Part H - Entry Points and Functions

In genProgram:
- Traverse Program children
- If MainStatement -> genMain
- If Class/Classes -> visit methods (one FunctionIR per method)

In genMain:
- Create function main
- Create entry block main_entry
- Traverse the Statements child

Method naming suggestion:
- ClassName_methodName

---

## Part I - CFG Dot Output (CFGDotWriter.h)

Write a helper that prints:
- one subgraph cluster per function
- one node per basic block
- block label includes TAC lines
- edges from block.succ

Output file:
- cfg.dot

Render:

```bash
make cfg
```

Open cfg.pdf.

---

## Part J - Wiring in main.cc and Makefile

In main.cc:
- After semantic pass succeeds:
  - IRGenerator ir;
  - ProgramIR pir = ir.generate(root);
  - ir.writeCFGDot(pir, "cfg.dot");

In Makefile:
- Add IRGenerator.cc to compiler build line
- Add target:

```make
cfg:
	dot -Tpdf cfg.dot -ocfg.pdf
```

---

## Part K - Expected TAC for test_lesson.txt

When you run:

```bash
./compiler test_lesson.txt
```

you should now see TAC directly in the terminal, like:

```txt
=== TAC for main ===
B0 (main_entry):
  a := 2
  b := 3
  t0 := a + b
  c := t0
  print c
  return 0
```

Note:
- Function order may vary if there are class methods (map iteration order).
- Temp names can vary.

A valid TAC shape is:

```txt
B0:
  a := 2
  b := 3
  t0 := a + b
  c := t0
  print c
  return 0
```

Exact temp names can vary.

---

## Part L - Quick Validation Checklist

Run:

```bash
make
./compiler test_lesson.txt
make cfg
```

Then verify:
- Compiler succeeds
- Terminal shows `=== TAC for main ===` (and other functions if present)
- cfg.dot exists
- cfg.pdf opens
- CFG has main block and TAC lines

Also smoke test all valid files:

```bash
for f in valid/*.cpm; do echo "$f"; ./compiler "$f" >/dev/null || break; done
```

---

## What You Build Next

Next live session (Part 2) will use this IR to generate C/C++:
- TAC -> C/C++ statements
- Emit labels and goto for CFG
- Compile generated C++ with g++ and compare output with source program
