# Mini Lesson: Assignment 3 Option 2 - IR Generation Code-Along (AST -> IR)

This lesson is the IR counterpart of the semantic lesson.

Goal for this session:
- Build IR data structures (TAC + BasicBlock + FunctionIR)
- Traverse AST and emit IR for a working subset
- Export CFG to cfg.dot and render cfg.pdf

You will edit:
- IR.h
- IRGenerator.h
- IRGenerator.cc
- CFGDotWriter.h
- main.cc
- Makefile

This guide is intentionally detailed and trace-based so you can implement, run, and debug one layer at a time.

---

## Part A: What You Are Building

Current compiler pipeline:

1. Lexer + parser build AST (already done)
2. Semantic analyzer validates symbols/types (already done)
3. IR generator traverses AST and emits TAC into basic blocks (this lesson)
4. CFG writer prints blocks/edges to dot (this lesson)
5. Next lesson: lower TAC to C/C++

For this lesson, assume semantic analysis already passed.

---

## Part B: First Test Program and Expected AST Shape

Create test_lesson.txt:

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

In AST output, you should see nodes like:
- MainStatement
- Statements
- VarDecl:a
- VarDecl:b
- VarDecl:c
- AddExpression
- PrintStatement
- ReturnStatement

These are the exact node types your IR traversal must match.

---

## Part C: File Responsibilities (Know This Before Coding)

| File | Role | Edit now? |
|------|------|-----------|
| IR.h | IR model types (ops, TAC, blocks, function/program containers) | YES |
| IRGenerator.h | Class state + traversal method declarations | YES |
| IRGenerator.cc | Core AST -> TAC/CFG generation logic | YES |
| CFGDotWriter.h | Print ProgramIR as Graphviz dot | YES |
| main.cc | Wire semantic pass -> IR generation -> dot output | YES |
| Makefile | Build includes IRGenerator.cc, plus cfg target | YES |

---

## Part D: Build Core IR Types in IR.h

Implement these core structures:
- IROp enum
- TAC struct
- BasicBlock struct
- FunctionIR struct
- ProgramIR struct

Minimum ops to include now:
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

TAC fields:
- op
- dst
- src1
- src2
- extra
- line

BasicBlock fields:
- id
- name
- code (vector<TAC>)
- succ (vector<int>)

Add this helper in BasicBlock:
- addSuccessor(int id)

Behavior:
- Only append id if it is not already in succ
- This prevents duplicate CFG edges when branches are emitted from multiple paths

Why this matters:
- CFG readability in dot improves immediately
- Future passes (liveness, codegen) rely on clean edge sets

### D1. How TAC is actually represented in memory

TAC is not a string-first representation. It is a typed record:

```cpp
struct TAC {
   IROp op;
   std::string dst;
   std::string src1;
   std::string src2;
   std::string extra;
   int line;
};
```

Read this as a fixed layout for many instruction shapes:

- Binary math/comparison:
   - op = Add/Sub/Mul/Div/Cmp*
   - dst = result temp or variable
   - src1 = left operand
   - src2 = right operand
   - extra = empty

- Assignment:
   - op = Assign
   - dst = lhs
   - src1 = rhs

- Conditional branch:
   - op = IfFalseGoto
   - src1 = condition value
   - extra = target block id as string

- Unconditional branch:
   - op = Goto
   - extra = target block id as string

- Calls:
   - op = Call
   - dst = return temp (or empty for void-style)
   - src1 = receiver (for method calls)
   - src2 = argument count
   - extra = callee name

This is why TAC scales well: different instructions reuse the same field layout.

### D2. How a TAC instruction gets emitted (full path)

For any expression/statement, the runtime path is:

1. AST node enters genExpr/genStmt
2. logic decides an IROp + operands
3. emit(op, dst, src1, src2, extra, line) is called
4. emit appends a TAC object into currentBlock().code
5. CFG writer later converts TAC to text for dot/console using tacToString

So TAC implementation has two layers:
- Construction layer: IRGenerator emit/genExpr/genStmt
- Pretty-print layer: CFGDotWriter::tacToString

If one layer is updated without the other, you get either missing logic or ugly/unknown text output.

---

## Part E: IRGenerator Skeleton in IRGenerator.h

Add fields:
- ProgramIR program
- int tempCounter
- int blockCounter
- FunctionIR* currentFunc
- int currentBlockId

Add methods:
- ProgramIR generate(Node* root)
- void genProgram(Node* root)
- void genMain(Node* mainNode)
- void genStmt(Node* stmt)
- string genExpr(Node* expr)
- int newBlock(const string& name)
- string newTemp()
- void emit(...)
- void writeCFGDot(...)

Contracts:
- genExpr returns a value name: literal, identifier, or temporary
- genStmt emits TAC and may create/switch basic blocks
- emit always appends into current function + current block

State model to remember:
- currentFunc points to the function currently being filled
- currentBlockId is the "insertion cursor" block
- newBlock allocates and returns an id, but does not auto-switch unless you do it explicitly

---

## Part F: Implement genExpr First (Do This Before Statements)

This is the most reusable part. Start here.

### F1. Atom expressions

Cases:
- ID -> return node->value
- Int / Float / Bool -> return node->value
- String -> return quoted literal

No TAC emitted in these trivial cases.

### F2. Binary operators

Support at least:
- AddExpression
- SubExpression
- MultExpression
- DivExpression

Then add comparison forms if your grammar emits them:
- LT/LE/GT/GE/EQ/NE forms

Canonical emission pattern:

```text
left  = genExpr(lhs)
right = genExpr(rhs)
t     = newTemp()
emit(op, t, left, right)
return t
```

Example for a + b:

```text
t0 := a + b
```

### F3. Unary/other expression forms

Implement as available in your AST:
- Negation (logical not): t := !x
- ArrayExpression/indexing: t := arr[i] (ArrayLoad)
- Length expression: t := arr.length (Length)

### F4. Debugging rule for genExpr

Every non-atom expression should usually return a temp name.

If you accidentally return an empty string, later statements will emit broken TAC.
Add defensive checks while developing.

### F5. TAC construction examples from real node patterns

For AddExpression:

```text
lhs = genExpr(left)
rhs = genExpr(right)
t = newTemp()
emit(Add, t, lhs, rhs)
return t
```

For NegationExpression:

```text
v = genExpr(child)
t = newTemp()
emit(Not, t, v)
return t
```

For ArrayExperssion read arr[i]:

```text
base = genExpr(arr)
idx = genExpr(i)
t = newTemp()
emit(ArrayLoad, t, base, idx)
return t
```

For constructor-style list expression new int[3]{1,2,3} style:

```text
values serialized into "1, 2, 3"
emit(NewArray, t, elementType, size, serializedValues)
```

These patterns show why TAC is "3-address code": most data-producing operations become one new temp assignment.

---

## Part G: Implement genStmt Working Subset

Add handlers in this order.

### G1. Statements container

If node type is Statements:
- recurse all children in order

This preserves statement order in emitted TAC.

### G2. VarDecl

If declaration has initializer:
1. rhs = genExpr(initializer)
2. emit Assign(varName, rhs)

Example for:

```cpm
volatile c : int := a + b
```

Possible TAC:

```text
t0 := a + b
c := t0
```

If declaration has no initializer:
- emit nothing for now

### G3. AssignmentStatement

If lhs is ID:
- rhs = genExpr(rhsNode)
- emit Assign(lhsName, rhs)

If lhs is array access arr[i]:
- idx = genExpr(indexNode)
- rhs = genExpr(rhsNode)
- emit ArrayStore(arrName, idx, rhs)

### G4. PrintStatement

- value = genExpr(arg)
- emit Print(value)

### G5. readStatement

- identify target variable
- emit Read(target)

### G6. ReturnStatement

- value = genExpr(expr)
- emit Return(value)

After G1-G6, test_lesson.txt should produce a single basic block with TAC lines.

---

## Part H: Add Control Flow Blocks (If / IfElse / For)

Only add this after expression and simple statements are stable.

### H1. IfStatement (no else)

Create blocks:
- thenBlock
- joinBlock

From current block:
1. cond = genExpr(condition)
2. emit IfFalseGoto(cond, joinBlock)
3. add CFG edges:
   - current -> thenBlock (true)
   - current -> joinBlock (false)
4. switch to thenBlock and emit body
5. if thenBlock not terminated by Return/Goto, emit Goto(joinBlock)
6. set currentBlockId = joinBlock

Termination rule:
- A block ending in Return or Goto should not emit extra jump.

### H2. IfElseStatement

Create blocks:
- thenBlock
- elseBlock
- joinBlock

From current block:
1. cond = genExpr(condition)
2. emit IfFalseGoto(cond, elseBlock)
3. add edges current -> thenBlock and current -> elseBlock
4. emit then branch in thenBlock
5. emit else branch in elseBlock
6. if then block not terminated, emit Goto(joinBlock)
7. if else block not terminated, emit Goto(joinBlock)
8. continue in joinBlock

### H3. ForStatement

Use 4 blocks:
- for_cond
- for_body
- for_step
- for_exit

Flow:
1. emit init in current block
2. emit Goto(for_cond)
3. switch to for_cond, emit condition
4. emit IfFalseGoto(cond, for_exit)
5. add true edge to for_body
6. emit body in for_body, then goto for_step if not terminated
7. emit step in for_step, then goto for_cond
8. continue at for_exit

Common bug here:
- Forgetting one edge in succ makes CFG look disconnected even if TAC seems right.

---

## Part I: Entry Points and Function Discovery

### I1. generate(root)

Should:
1. reset counters and state
2. call genProgram(root)
3. return full ProgramIR

### I2. genProgram(root)

Traverse Program children:
- MainStatement -> genMain(mainNode)
- Class/Classes -> inspect methods and create one FunctionIR per method

Method naming suggestion:
- ClassName_methodName

### I3. genMain(mainNode)

Steps:
1. create FunctionIR named main
2. set currentFunc to it
3. create entry block main_entry via newBlock
4. set currentBlockId to that block
5. traverse Statements child and emit TAC

---

## Part J: Emission Helpers (newTemp/newBlock/emit)

### J1. newTemp

Pattern:
- t0, t1, t2, ...

Implementation idea:
- return "t" + to_string(tempCounter++)

### J2. newBlock

Pattern:
- B0, B1, B2...

Store both:
- numeric id
- human label like main_entry / if_then / if_join

### J3. emit

emit should:
1. locate current block in current function
2. append TAC record
3. optionally stamp source line from AST node when available

Having line numbers now helps error reporting in later backend stages.

---

## Part K: CFG Dot Output in CFGDotWriter.h

Write a helper that prints:
- one subgraph cluster per function
- one node per basic block
- block label containing TAC lines
- directed edges from block.succ

Output file:
- cfg.dot

Rendering:

```bash
make cfg
```

Open cfg.pdf.

Dot formatting tips:
- Escape quotes in TAC text
- Use \l line-break style in Graphviz labels for left-aligned multiline output
- Include block id and block name in node label

---

## Part L: Wire in main.cc and Makefile

In main.cc, after semantic pass succeeds:

```cpp
IRGenerator ir;
ProgramIR pir = ir.generate(root);
ir.writeCFGDot(pir, "cfg.dot");
```

In Makefile:
1. Ensure IRGenerator.cc is part of compiler build sources
2. Add cfg target:

```make
cfg:
	dot -Tpdf cfg.dot -ocfg.pdf
```

---

## Part M: Step-by-Step Trace for test_lesson.txt

Given:

```cpm
main(): int {
  volatile a : int := 2
  volatile b : int := 3
  volatile c : int := a + b
  print(c)
  return 0
}
```

Expected traversal trace:

1. Enter main
2. Create block B0(main_entry)
3. VarDecl a with initializer 2 -> emit a := 2
4. VarDecl b with initializer 3 -> emit b := 3
5. VarDecl c with initializer a+b:
   - genExpr(a+b) emits t0 := a + b
   - then emit c := t0
6. Print(c) -> emit print c
7. Return 0 -> emit return 0

Expected TAC shape:

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

Notes:
- Temp names may differ (t1 instead of t0 is still fine)
- Function listing order may vary

---

## Part N: Validation Commands and What to Check

Build + run + render:

```bash
make
./compiler test_lesson.txt
make cfg
```

Verify:
- compiler exits successfully
- terminal prints TAC for main
- cfg.dot exists
- cfg.pdf opens
- block labels show TAC lines
- CFG edges match control flow

Smoke test all valid programs:

```bash
for f in valid/*.cpm; do echo "$f"; ./compiler "$f" >/dev/null || break; done
```

---

## Part O: Common Mistakes and Fast Fixes

1. Symptom: Empty TAC rhs like x :=
   - Cause: genExpr returned empty string for an unhandled node type
   - Fix: Add branch and return valid value/temp

2. Symptom: CFG node exists but has no incoming edge
   - Cause: forgot addSuccessor during branch emission
   - Fix: add both true and false edges explicitly

3. Symptom: Infinite loop in for CFG
   - Cause: missing goto for_exit or wrong block switch order
   - Fix: verify H3 order exactly

4. Symptom: Duplicate CFG edges
   - Cause: succ pushes same id multiple times
   - Fix: dedupe in addSuccessor

5. Symptom: TAC emitted into wrong function
   - Cause: currentFunc not switched/restored correctly while traversing methods
   - Fix: set currentFunc at function entry and keep generation scoped

6. Symptom: TAC prints as <unknown tac>
   - Cause: new IROp added, but CFGDotWriter::tacToString missing switch case
   - Fix: add formatting case for that op

7. Symptom: operation node appears in AST but no TAC line emitted
   - Cause: node type missing in isBinaryNode/opForNode (or missing branch in genExpr)
   - Fix: wire node type into dispatch table and emit logic

---

## Part R: How to Add a New TAC Operation End-to-End

Use this checklist every time.

### R1. Add op enum value

File: IR.h

Add the new opcode in IROp, for example Mod.

### R2. Teach expression dispatch to map AST node -> opcode

File: IRGenerator.cc

Update:
- isBinaryNode(...): include your AST node type (example: ModExpression)
- opForNode(...): map that node type to IROp::Mod

If the operation is unary, skip isBinaryNode and add a dedicated branch in genExpr.

### R3. Emit TAC in genExpr or genStmt

File: IRGenerator.cc

For expression op:

```text
lhs = genExpr(left)
rhs = genExpr(right)
t = newTemp()
emit(IROp::<NewOp>, t, lhs, rhs, "", expr->lineno)
return t
```

For statement-only op, emit inside genStmt branch.

### R4. Add text formatting so debug output and cfg labels are readable

File: CFGDotWriter.h

Add switch case in tacToString for your new op.

Example shape:

```text
case IROp::Mod:
  return tac.dst + " := " + tac.src1 + " % " + tac.src2;
```

### R5. Validate with a tiny source program

Create the smallest program that uses only the new operation.

Check:
- compiler runs
- TAC contains expected instruction line
- cfg.dot contains that line in the block label
- cfg.pdf renders

### R6. Optional: when operation affects control flow

If your new logic introduces branch behavior (short-circuit forms, loop-like node, etc.), also update:
- genStmt block creation/switch order
- addEdge calls for every successor
- blockTerminates-sensitive fallthrough gotos

Missing any one of these causes malformed CFG even if TAC lines look plausible.

---

## Part P: Development Order That Minimizes Debug Time

Use this exact order:

1. IR.h structs and enums
2. IRGenerator skeleton + counters + emit helpers
3. genExpr atoms and arithmetic
4. genStmt for declarations/assign/print/return
5. main.cc wiring and quick TAC print
6. CFG writer + make cfg
7. if/if-else blocks
8. for-loop blocks
9. run smoke tests in valid/

If something breaks, roll back one step and re-run the same test before moving forward.

---

## Part Q: Quick Reference

Commands:

```bash
make
./compiler test_lesson.txt
make cfg
```

Key methods:
- genExpr(Node*): returns value/temp name
- genStmt(Node*): emits TAC and may alter CFG
- emit(...): append TAC into current block
- newTemp(): create temporary variable names
- newBlock(name): allocate block and return id

Core invariant:
- Every TAC instruction belongs to exactly one function and one basic block.

---

## Next Session (Part 2)

You will consume this IR and generate C/C++:
- TAC -> C/C++ statements
- block ids -> labels and gotos
- preserve control flow from CFG
- compile generated C++ and compare runtime behavior against source program
