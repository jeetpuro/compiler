# Semantic Analysis — Learning Guide

This guide explains **what** we built, **why** each piece exists, **where** it
lives, and **how** to keep extending it incrementally.

---

## Table of Contents

1. [What is Semantic Analysis?](#1-what-is-semantic-analysis)
2. [Where We Were Before (the starting point)](#2-where-we-were-before)
3. [The Big Picture — What We Added](#3-the-big-picture--what-we-added)
4. [File-by-File Walkthrough](#4-file-by-file-walkthrough)
   - [SymbolTable.h — The Three Classes](#41-symboltableh--the-three-classes)
   - [SemanticAnalyzer.h — The Traversal](#42-semanticanalyzerh--the-traversal)
   - [main.cc — Wiring It All Together](#43-maincc--wiring-it-all-together)
   - [Makefile — New Targets](#44-makefile--new-targets)
5. [How the Symbol Table Tree Works](#5-how-the-symbol-table-tree-works)
6. [How the AST Traversal Works](#6-how-the-ast-traversal-works)
7. [How the Visual Debugger Works](#7-how-the-visual-debugger-works)
8. [Incremental Implementation Order](#8-incremental-implementation-order)
9. [How to Keep Adding Semantic Checks](#9-how-to-keep-adding-semantic-checks)
10. [Common Pitfalls](#10-common-pitfalls)
11. [Quick Reference: Node Types and Their Children](#11-quick-reference-node-types-and-their-children)

---

## 1. What is Semantic Analysis?

The compiler has three phases so far:

```
Source Code  →  [Lexer]  →  Tokens  →  [Parser]  →  AST  →  [Semantic Analysis]  →  ???
```

- **Lexer** checks: are the characters valid tokens? (lexical errors)
- **Parser** checks: are the tokens in valid order? (syntax errors)
- **Semantic Analysis** checks: does the program **make sense**?

Semantic analysis answers questions like:
- "Is this variable declared before it is used?"
- "Is this class name declared twice?" (duplicate identifier)
- "Does this method return the correct type?"
- "Are you adding an `int` to a `boolean`?" (type mismatch)

**Part 1** (what we built) focuses on: **constructing the symbol table** and
**detecting duplicate identifiers**. Everything else (type checking, undeclared
identifiers, etc.) builds on top of this foundation.

---

## 2. Where We Were Before

Before our changes, the compiler did this:

```
Parse input  →  Build AST  →  print_tree()  →  generate_tree() (tree.dot)
```

The key files were:

| File | Purpose |
|------|---------|
| `lexer.flex` | Tokenizer — converts characters to tokens |
| `parser.yy` | Grammar — converts tokens to AST nodes |
| `Node.h` | The AST node class (pure data structure — `type`/`value`/`children`/`errorMsg`) |
| `main.cc` | Entry point — calls parser, prints tree, exits |
| `Makefile` | Build rules |

The `errCodes` enum in `main.cc` already had `SEMANTIC_ERROR = 4`, but nothing
ever set it. That was the placeholder waiting for us.

---

## 3. The Big Picture — What We Added

We added **one new file** and **modified three existing files**:

```
NEW:     SymbolTable.h      ← Record, Scope, SymbolTable classes
NEW:     SemanticAnalyzer.h ← buildSymbolTable() traversal + all semantic checks
CHANGED: Node.h             ← added errorMsg field + generate_tree_semantic()
CHANGED: main.cc            ← call semantic analysis after parsing
CHANGED: Makefile            ← new test/debug targets
```

Zero new `.cc` files. Everything is header-only (inline), so the Makefile
compile line didn't change at all — it still just compiles `main.cc` +
`parser.tab.o` + `lex.yy.c`.

**Why a separate `SemanticAnalyzer.h`?** The AST node (`Node.h`) should be
the data structure — it shouldn't know about semantic rules. The analyzer walks
the tree *externally*, keeping analysis logic cleanly separated from the data.
`SymbolTable.h` is `#include`-d by `SemanticAnalyzer.h`, which is `#include`-d
by `main.cc`. `Node.h` stays lightweight with zero knowledge of the symbol table.

---

## 4. File-by-File Walkthrough

### 4.1 SymbolTable.h — The Three Classes

This file implements the tree-based symbol table from the lecture slides. It
has exactly three classes, mirroring the slide design:

#### Record (lines 14–28)

```cpp
class Record {
    string id;     // the identifier name, e.g. "x", "MyClass", "foo"
    string kind;   // what kind of thing: "class", "method", "variable", "parameter"
    string type;   // the type: "int", "boolean", "float[]", "MyClass", etc.
};
```

**Why a Record?** Every symbol in the program (a variable, a method, a class,
a parameter) needs to be stored somewhere. The Record is that "somewhere" — it
is the value stored in the symbol table's map.

**What goes in `kind`?** This tells us *what category* the symbol belongs to.
When we detect a duplicate, we use it to print the right error message:
`"Already Declared variable: 'x'"` vs `"Already Declared Function: 'foo'"`.

**What goes in `type`?** The data type. For variables it's `"int"`, `"float"`,
`"boolean"`, `"int[]"`, or a class name. For methods it's the return type. For
classes it's just `"class"`.

#### Scope (lines 33–82)

```cpp
class Scope {
    string label;                   // human-readable name for debugging
    Scope* parent;                  // link to enclosing scope (nullptr for global)
    vector<Scope*> children;        // nested scopes (class > method > ...)
    map<string, Record*> records;   // symbols declared HERE
    int next;                       // counter for tree traversal
};
```

**Why a tree of scopes?** Because scopes nest. Global scope contains class
scopes. Class scopes contain method scopes. The tree mirrors this:

```
global
├── class:DuplicateIdentifiers
│   ├── method:func
│   └── method:func  (duplicate!)
├── class:MyClass
│   ├── method:Pen
│   └── method:Pen2
└── main
```

**Key methods:**

| Method | What it does | When to use it |
|--------|-------------|----------------|
| `containsKey(key)` | Checks ONLY this scope's map | Duplicate detection — "is `x` already declared *here*?" |
| `lookup(key)` | Checks this scope, then parent, then grandparent... | Use-site lookup — "has `x` been declared *anywhere visible*?" (Part 2) |
| `nextChild()` | Returns (or creates) the next child scope | Called by `enterScope()` |
| `resetScope()` | Resets the `next` counter for a fresh traversal | Called by `resetTable()` for multi-pass |
| `printScope(depth)` | Indented console dump | Debugging |

**Why `containsKey` vs `lookup`?** This is the core distinction:
- `containsKey` = "is it declared **in this exact scope**?" → for catching duplicates
- `lookup` = "is it declared **anywhere I can see**?" → for checking if a variable exists

For Part 1 (duplicates only), we only use `containsKey`. `lookup` is ready
for Part 2 when we check if variables are declared before use.

#### SymbolTable (lines 88–135)

```cpp
class SymbolTable {
    Scope* root;      // the outermost (global) scope
    Scope* current;   // the scope we are currently inside
};
```

**Why separate from Scope?** The `SymbolTable` is the *manager* — it tracks
where we are in the tree (`current`). `Scope` is just a node in the tree. You
walk the tree by calling `enterScope()`/`exitScope()` on the SymbolTable, and
it moves `current` up and down the tree for you.

**Key methods:**

| Method | What it does |
|--------|-------------|
| `enterScope(label)` | Move `current` DOWN to a new child scope |
| `exitScope()` | Move `current` UP to the parent scope |
| `put(key, Record*)` | Check for duplicate in current scope → insert or reject |
| `lookup(key)` | Delegates to `current->lookup(key)` — walks up the tree |
| `printTable()` | Prints the whole tree (all scopes, all records) |
| `resetTable()` | Resets all counters for a second traversal pass |

**The `put` method returns `bool`:** `true` = inserted OK, `false` = key
already exists (duplicate). This is the mechanism that drives duplicate
detection:

```cpp
if (!st.put("x", new Record("x", "variable", "int"))) {
    // x is already declared in this scope → semantic error!
}
```

---

### 4.2 SemanticAnalyzer.h — The Traversal

This is a **separate class** that walks the AST and performs semantic checks.
The `Node` class knows nothing about semantic analysis — the analyzer takes
a `Node*` and inspects it from outside.

We added three things to the existing `Node` class in `Node.h`:

#### a) `errorMsg` field

```cpp
string errorMsg = "";   // set by SemanticAnalyzer when a semantic error is found
```

**Why?** When the analyzer finds a semantic error at a node (e.g., a duplicate
`VarDecl`), it stores the error message on that node. Later, the visual debugger
reads this field to color the node red in the Graphviz output. It separates
"detecting the error" from "displaying the error".

#### b) `getTypeStr()` helper (in SemanticAnalyzer.h)

```cpp
static string getTypeStr(const Node* node) {
    if (node->type == "ArrayType" && !node->children.empty())
        return node->children.front()->value + "[]";  // e.g. "int[]"
    return node->value;                                 // e.g. "int", "boolean"
}
```

**Why?** Types in the AST come in two forms:
- `Type` node with `value = "int"` → just return `"int"`
- `ArrayType` node with child `Type:int` → we need to compose `"int[]"`

This helper handles both cases so we don't repeat the logic everywhere.

#### c) `buildSymbolTable(Node* node)` — the core method

This is the **core of Part 1**. It is a recursive method in `SemanticAnalyzer`
that walks the AST left-to-right and populates the symbol table. It branches
on `node->type`:

| Node type | What we do |
|-----------|-----------|
| `"Program"` | Just recurse all children (no scope change at program level) |
| `"Class"` | `put` class record → `enterScope` → recurse children → `exitScope` |
| `"Method"` | `put` method record → `enterScope` → recurse children → `exitScope` |
| `"Param"` | `put` parameter record (no recursion — its only child is a Type) |
| `"VarDecl"` (non-empty value) | `put` variable record (skip container VarDecl nodes with empty value) |
| `"MainStatement"` | `enterScope` → recurse children → `exitScope` |
| Everything else | Recurse all children (pass-through) |

**Why branch on `this->type`?** Because our AST uses a single flat `Node`
class — there's no `ClassNode` subclass, no `MethodNode` subclass. The `type`
string is the only way to know what kind of node we're looking at.

**Why "pass-through" for everything else?** Nodes like `"Statements"`,
`"Methods"`, `"VarDecl"` (container), `"IfStatement"`, `"ForStatement"`, etc.
don't declare any new symbols and don't create new scopes (in Part 1). We just
need to recurse through them to reach the nodes that *do* matter.

**The pattern for scope-creating nodes is always:**

```
1. Register the symbol in the CURRENT (parent) scope  ← put()
2. Enter a new scope                                    ← enterScope()
3. Recurse all children                                 ← buildSymbolTable() on each child
4. Exit back to the parent scope                        ← exitScope()
```

This is exactly the pattern from the lecture slide (Generating the ST - example):
```
symbolTable.put(mName, currentMethod);  // step 1
symbolTable.enterScope();               // step 2
execute(child[2]);  // parameterList    // step 3
execute(child[3]);  // methodBody       // step 3
symbolTable.exitScope();                // step 4
```

#### d) `generate_tree_semantic()` and `generate_tree_semantic_content()` (lines 154–201)

These mirror the existing `generate_tree()` / `generate_tree_content()` methods
but add visual styling based on `errorMsg`:

- **Normal nodes** → white box (default style)
- **Error nodes** (`errorMsg != ""`) → red double-octagon with the error
  message printed below the node label

**Why a separate method instead of modifying `generate_tree()`?** So you can
compare them. `make tree` gives you the plain AST. `make tree_semantic` gives
you the same tree with error highlighting. Having both lets you see exactly
what the semantic pass added.

---

### 4.3 main.cc — Wiring It All Together

We added two things:

1. `#include "SymbolTable.h"` at the top (line 3)
2. The semantic analysis block after the tree generation (lines 162–175):

```cpp
// ── Semantic Analysis: build symbol table ──
int semantic_errors = 0;
SymbolTable st;
root->buildSymbolTable(st, semantic_errors);
st.printTable();
root->generate_tree_semantic();

if (semantic_errors > 0) {
    printf("\nSemantic errors found: %d\n", semantic_errors);
    errCode = errCodes::SEMANTIC_ERROR;
} else {
    printf("\nSemantic analysis passed with no errors.\n");
}
```

**Why after `generate_tree()`?** The plain tree (`tree.dot`) shows the pure
AST as the parser built it — no semantic annotations. Then we run the semantic
pass which may set `errorMsg` on some nodes. Then `generate_tree_semantic()`
writes the annotated version. This way you always have both views.

**Why `semantic_errors` as a separate counter?** So the semantic pass can
increment it independently. If it's > 0 at the end, we set the exit code to
`SEMANTIC_ERROR = 4`. This integrates cleanly with the existing error code
system that already had `LEXICAL_ERROR = 1` and `SYNTAX_ERROR = 2`.

---

### 4.4 Makefile — New Targets

| Target | What it does |
|--------|-------------|
| `make tree_semantic` | Converts `tree_semantic.dot` → `tree_semantic.pdf` (Graphviz) |
| `make test_semantic` | Runs `./compiler` on every `semantic_errors/*.cpm` file |
| `make clean` | Now also removes `tree_semantic.dot` and `tree_semantic.pdf` |

The actual compile line (`g++ ... main.cc`) did NOT change because everything
is header-only.

---

## 5. How the Symbol Table Tree Works

Imagine this C+- program:

```
class Foo {
  x: int
  bar(a: int): boolean {
    y: float
    return true
  }
}
main(): int {
  z: int
  return 0
}
```

After `buildSymbolTable` runs, the symbol table tree looks like:

```
global
├── [class] Foo : class
│
├── Scope [class:Foo]
│   ├── [variable] x : int
│   ├── [method] bar : boolean
│   │
│   └── Scope [method:bar]
│       ├── [parameter] a : int
│       └── [variable] y : float
│
└── Scope [main]
    └── [variable] z : int
```

**Reading this tree:**
- `Foo` is registered in the **global** scope (because classes live at the top level)
- `x` and `bar` are registered in the **class:Foo** scope
- `a` and `y` are registered in the **method:bar** scope (nested inside class:Foo)
- `z` is registered in the **main** scope

**Why does this matter?** When you later do `lookup("x")` from inside
`method:bar`, the lookup walks: method:bar → class:Foo → global. It finds `x`
in class:Foo. If you write `lookup("z")` from inside `method:bar`, it walks
all the way up and doesn't find it — undeclared identifier error.

---

## 6. How the AST Traversal Works

The traversal is a single recursive walk. Here's the order of operations for
the program above:

```
1. Visit Program node
   2. Visit Class node (value="Foo")
      → st.put("Foo", class record)      ← register in global scope
      → st.enterScope("class:Foo")        ← move into class scope
      3. Visit VarDecl node (value="x")
         → st.put("x", variable record)  ← register in class scope
      4. Visit Method node (value="bar")
         → st.put("bar", method record)  ← register in class scope
         → st.enterScope("method:bar")    ← move into method scope
         5. Visit Param node (value="a")
            → st.put("a", parameter)     ← register in method scope
         6. Visit VarDecl node (value="y")
            → st.put("y", variable)      ← register in method scope
         7. Visit ReturnStatement
            → pass-through, recurse children (nothing to register)
         → st.exitScope()                 ← back to class scope
      → st.exitScope()                    ← back to global scope
   8. Visit MainStatement
      → st.enterScope("main")
      9. Visit VarDecl (value="z")
         → st.put("z", variable)
      → st.exitScope()
```

**Key insight:** We always `put` a symbol *before* `enterScope`. This is
because the symbol belongs to the **enclosing** scope. A method declared in a
class is visible from the class scope, not just from inside the method itself.

---

## 7. How the Visual Debugger Works

When `buildSymbolTable` finds a duplicate, it does two things:

1. Prints an error to `cerr` (terminal output)
2. Sets `this->errorMsg = "Already Declared variable: 'x'"` on the AST node

Then `generate_tree_semantic()` writes a `.dot` file where:
- Normal nodes are white boxes: `[label="VarDecl:x"]`
- Error nodes are red double-octagons: `[label="VarDecl:a\nAlready Declared variable: 'a'", fillcolor=red, fontcolor=white, shape=doubleoctagon]`

**How to use it:**

```bash
./compiler semantic_errors/DuplicateIdentifier.cpm   # runs semantic analysis
make tree_semantic                                     # generates PDF from .dot
# open tree_semantic.pdf — error nodes are bright red
```

**Extending it for future checks:** Any node where you detect a semantic error,
just set `errorMsg` on that node. The visual debugger will automatically render
it in red. You can also add different colors for different error types later
(e.g., yellow for warnings, orange for type mismatches).

---

## 8. Incremental Implementation Order

If you need to rebuild this from scratch or understand the order, here is how
to add things **one step at a time**, testing after each:

### Stage 1 — Empty infrastructure
- Create `SymbolTable.h` with empty `Record`, `Scope`, `SymbolTable` stubs
- Add `buildSymbolTable` to `SemanticAnalyzer.h` that just prints every node type
- Wire it into `main.cc`
- **Test:** `./compiler valid/test1.cpm` — prints every node, no crash

### Stage 2 — Scope entry/exit for Program, Class, MainStatement
- In `buildSymbolTable`, add `enterScope`/`exitScope` for Class and Main
- Print debug messages: "Entering class scope: Foo"
- **Test:** scope messages appear at the right depth

### Stage 3 — Scope entry/exit for Method
- Add `enterScope`/`exitScope` for Method nodes
- **Test:** method scopes nest inside class scopes

### Stage 4 — Register Class records
- Implement `Record`, `Scope::put`, `SymbolTable::put`, `printTable`
- In `"Class"` branch: `st.put(value, new Record(value, "class", "class"))`
- **Test:** `st.printTable()` shows class names

### Stage 5 — Register Method records
- In `"Method"` branch: `st.put(value, new Record(value, "method", retType))`
- **Test:** methods appear under their class in `printTable`

### Stage 6 — Register Parameter records
- In `"Param"` branch: `st.put(value, new Record(value, "parameter", typeStr))`
- **Test:** params appear under their method

### Stage 7 — Register Variable records
- In `"VarDecl"` branch (non-empty value only): `st.put(value, new Record(...))`
- **Test:** variables appear at correct scope levels

### Stage 8 — Duplicate detection
- Add `containsKey` check in `SymbolTable::put`
- Set `errorMsg` on duplicate nodes
- **Test:** `DuplicateIdentifier.cpm` reports all annotated errors

### Stage 9 — Visual debugger
- Add `errorMsg` field to Node
- Add `generate_tree_semantic()` method
- Add `make tree_semantic` target
- **Test:** red nodes in PDF for duplicate errors

### Stage 10 — (Part 2) Undeclared identifier lookup
- In `"ID"` branch: `st.lookup(value)` → if null, error
- **Test:** undeclared identifiers are caught

---

## 9. How to Keep Adding Semantic Checks

The symbol table + traversal is the **foundation**. Every future check follows
the same pattern:

### Check: Undeclared identifiers (Part 2)
**Where:** Add an `else if (type == "ID")` branch in `buildSymbolTable`
**Logic:** `Record* r = st.lookup(value); if (!r) → error`
**Why it works:** By the time we visit an `ID` node in an expression, all
declarations above it in the AST have already been processed (left-to-right
traversal).

### Check: Type mismatches in expressions (Part 2)
**Where:** Add branches for `"AddExpression"`, `"SubExpression"`, etc.
**Logic:** Recurse children, get their types, check compatibility
**Hint:** You'll want `buildSymbolTable` to return a string (the type of the
expression) instead of void. Refactor it to `string buildSymbolTable(...)`.

### Check: Return type mismatches (Part 2)
**Where:** In `"ReturnStatement"` branch
**Logic:** Get the expected return type from the enclosing method's Record,
compare with the actual expression type

### Check: Missing return statements (Part 2)
**Where:** In `"Method"` branch, after recursing children
**Logic:** Check if the method body contains at least one `ReturnStatement`
(walk children of the Statements node)

### Check: Statement after return (Part 2)
**Where:** In `"Statements"` branch
**Logic:** After seeing a ReturnStatement child, any subsequent statement
children are unreachable → warning/error

### General pattern for any new check:

1. **Identify which AST node type** the check applies to
2. **Add an `else if (type == "...")` branch** in `buildSymbolTable`
3. **Use `st.lookup()` or `st.put()`** to query/update the symbol table
4. **Set `errorMsg`** on the node if there's an error
5. **Increment `errors`** and print to `cerr`
6. The visual debugger picks up the `errorMsg` automatically — no extra work

---

## 10. Common Pitfalls

### "My VarDecl has value empty — why?"
The parser creates TWO kinds of `VarDecl` nodes:
- **Container** VarDecl (value = `""`) — holds a list of individual VarDecl children
- **Leaf** VarDecl (value = identifier name, e.g. `"x"`) — the actual declaration

We only register leaf nodes. The check `!value.empty()` skips containers.

### "My Method node has different numbers of children"
The parser has three variants:
- `Method(params, returnType, body)` — 3 children when params exist
- `Method(returnType, body)` — 2 children when no params
- `Method(returnType, body)` — 2 children on error recovery

That's why we **search** for the first `Type`/`ArrayType` child instead of
assuming `children[1]` is the return type.

### "Volatile variables have an extra child"
When `volatile` is present, the first child of VarDecl is a `Keyword:volatile`
node. The type node comes after. Our code searches for `Type`/`ArrayType` by
checking each child's `type` string, so it skips the `Keyword` automatically.

### "Some semantic_errors/*.cpm files have syntax errors"
Files like `DuplicateIdentifier.cpm` may not parse cleanly with the current
grammar. This is expected — some test files may need grammar adjustments, or
they test errors that require both syntax and semantic awareness. Focus on
testing with `valid/*.cpm` first, then tackle the semantic error files one by
one.

### "Why don't we create scopes for if/for blocks?"
In Part 1, we keep it simple: global → class → method scopes only. If a
variable declared inside an `if` block conflicts with one in the method scope,
we catch it at the method level. Adding block-level scopes is a refinement
you can add later by handling `"IfStatement"`, `"ForStatement"`, `"Statements"`
(inside stmtBl) with `enterScope`/`exitScope`.

---

## 11. Quick Reference: Node Types and Their Children

Use `make tree` and open `tree.pdf` to verify these. This is the ground truth
from `parser.yy`:

| Node `type` | `value` | Children (in order) |
|---|---|---|
| `Program` | `""` | vars?, classes?, entry |
| `Class` | class name | vars?, methods? |
| `Classes` | `""` | Class, Class, ... |
| `VarDecl` (container) | `""` | VarDecl, VarDecl, ... |
| `VarDecl` (leaf, volatile) | identifier | Keyword("volatile"), Type, expr? |
| `VarDecl` (leaf, normal) | identifier | Type, expr? |
| `Method` (with params) | method name | Params, Type (return), Statements (body) |
| `Method` (no params) | method name | Type (return), Statements (body) |
| `Params` | `""` | Param, Param, ... |
| `Param` | param name | Type |
| `Type` | `"int"` / `"float"` / `"boolean"` / `"void"` / className | — |
| `ArrayType` | `""` | Type (base type) |
| `MainStatement` | `""` | Statements (body) |
| `Statements` | `""` | stmt, stmt, ... |
| `IfStatement` | `""` | condition, then-body |
| `IfElseStatement` | `""` | condition, then-body, else-body |
| `ForStatement` | `""` | init, condition, update, body |
| `PrintStatement` | `""` | expr |
| `ReturnStatement` | `""` | expr |
| `AssignmentStatement` | `""` | lhs-expr, rhs-expr |
| `FunctionCall` | method name | object-expr?, args (Expression node)? |
| `ID` | identifier name | — |
| `Int` | integer string | — |
| `Float` | float string | — |
| `Bool` | `"true"` / `"false"` | — |

**Pro tip:** When in doubt about any node's children, run:
```bash
./compiler valid/test2.cpm   # build the tree
make tree                    # generate PDF
```
Open `tree.pdf` and find the node you're interested in. The children are
exactly what `parser.yy` pushed with `children.push_back(...)`.
