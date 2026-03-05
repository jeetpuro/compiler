# Mini Lesson: Adding Semantic Checks — Hands-On Code-Along

This lesson teaches you **how to add semantic checks** by actually doing it.
We start from the working bare-bones symbol table, add checks one at a time,
compile, test, and see them light up in the visual debugger.

**You will edit ONE file:** `SemanticAnalyzer.h` → the `buildSymbolTable()` method.
That's the only place semantic rules live.

**Files overview — what does what:**

| File | Role | Do you edit it? |
|------|------|-----------------|
| `SymbolTable.h` | Data structure (Record, Scope, SymbolTable classes) | Rarely — only if you need to store new data |
| `Node.h` | AST node (pure data structure + tree printing) | No — Node is just data |
| `SemanticAnalyzer.h` | `buildSymbolTable()` — all semantic rules live here | **YES — this is where you add checks** |
| `main.cc` | Creates `SemanticAnalyzer`, calls `analyze()`, prints results | No (already wired up) |
| `Makefile` | Build + debug targets | No (already set up) |

---

## Part A: Understanding the Test Program

Save this as `test_lesson.txt`:

```cpm
main(): int {
    x : int
    y : int
    y := 5
    x := 7
    return 0
}
```

Run it:

```bash
./compiler test_lesson.txt
```

### What you'll see:

**1. The AST (print_tree output):**
```
Program:
  MainStatement:
    Statements:
      VarDecl:x          ← declares x
        Type:int
      VarDecl:y          ← declares y
        Type:int
      AssignmentStatement:  ← uses y
        ID:y
        Int:5
      AssignmentStatement:  ← uses x
        ID:x
        Int:7
      ReturnStatement:
        Int:0
```

This is the tree the parser built. **No semantic checking has happened yet.**

**2. The Symbol Table (printTable output):**
```
=== Symbol Table ===
--- Scope [global] ---
  --- Scope [main] ---
    [variable] x : int
    [variable] y : int
====================
```

The traversal walked the tree and registered `x` and `y` in the `main` scope.

**3. The result:**
```
Semantic analysis passed with no errors.
```

Now generate the visual tree:

```bash
make tree_semantic
```

Open `tree_semantic.pdf` — all nodes are white boxes. No errors to highlight.

---

## Part B: How the Traversal Works on This File

Here's what `buildSymbolTable` does step by step. Follow along in
`SemanticAnalyzer.h` (the `buildSymbolTable` method):

```
Step 1: Visit Program:
        → type == "Program"
        → Action: recurse all children
        → No scope change, no record added

Step 2: Visit MainStatement:
        → type == "MainStatement"
        → Action: st.enterScope("main")
        → Now current scope = main (a child of global)
        → Recurse children...

Step 3: Visit Statements:
        → type == "Statements"
        → Falls into the "else" default → just recurse children
        → This is a "pass-through" node

Step 4: Visit VarDecl:x
        → type == "VarDecl" && value == "x" (not empty → leaf node)
        → Find Type child → "int"
        → Call st.put("x", new Record("x", "variable", "int"))
        → Inside put(): containsKey("x") → NOT FOUND → insert → return true
        → Symbol table: main { x: int }

Step 5: Visit VarDecl:y
        → Same logic → st.put("y", ...) → insert OK
        → Symbol table: main { x: int, y: int }

Step 6: Visit AssignmentStatement: (y := 5)
        → Falls into "else" default → recurse children
        → Visits ID:y → "else" default (leaf, nothing to do)
        → Visits Int:5 → "else" default (leaf, nothing to do)

Step 7: Visit AssignmentStatement: (x := 7)
        → Same as Step 6

Step 8: Visit ReturnStatement:
        → Falls into "else" default → recurse children
        → Visits Int:0 → leaf, nothing

Step 9: Back in MainStatement → st.exitScope()
        → current moves back to global
```

**Key insight:** Steps 6 and 7 visit `ID:y` and `ID:x` but do NOTHING with
them. They fall into the default branch. The compiler doesn't check if those 
variables actually exist. That's our first check to add!

---

## Part C: Adding Check #1 — Undeclared Identifiers

### The problem

Change `test_lesson.txt` to use a variable that doesn't exist:

```cpm
main(): int {
    x : int
    y := 5
    z := 7
    return 0
}
```

Here `y` was never declared but is used in `y := 5`. Same for `z`. Right now 
the compiler says "no errors" — that's wrong!

### Where to add the check

Open `SemanticAnalyzer.h`, find the `buildSymbolTable` method. Look for this section:

```cpp
        } else if (type == "MainStatement") {
            st.enterScope("main");
            for (auto* child : children)
                if (child) child->buildSymbolTable(st, errors);
            st.exitScope();

        } else {
            // Default pass-through: recurse all children
```

### What to add

Insert a NEW `else if` branch **between** `MainStatement` and the `else`
default. This is where ALL new checks go — right before the default:

```cpp
        } else if (type == "MainStatement") {
            st.enterScope("main");
            for (auto* child : node->children)
                if (child) buildSymbolTable(child);
            st.exitScope();

        // ──────── NEW CHECK: Undeclared identifiers ────────
        } else if (type == "ID") {
            // This node is an identifier being USED (not declared).
            // Check: has it been declared in any visible scope?
            if (!st.lookup(value)) {
                reportError(node, "Undeclared identifier: '" + value + "'");
            }

        } else {
            // Default pass-through: recurse all children
```

### What changed in the code?

Before:
```
ID:y → falls into "else" default → does nothing
```

After:
```
ID:y → matches "type == ID" → calls st.lookup("y")
     → lookup walks: main scope → NOT FOUND → global → NOT FOUND → nullptr!
     → reportError(node, "Undeclared identifier: 'y'")
     → errors++ and errorMsg set on this node → visual debugger turns it red
```

### How `st.lookup()` works (in SymbolTable.h)

The call chain is:

```
st.lookup("y")                          // in SymbolTable class
  → current->lookup("y")               // in Scope class (current = main)
    → records.find("y") → not here
    → parent->lookup("y")              // walks up to global scope
      → records.find("y") → not here
      → parent == nullptr → return nullptr   // reached the top, not found
```

This is the `Scope::lookup()` method in SymbolTable.h:
```cpp
Record* lookup(const string& key) const {
    auto it = records.find(key);          // check THIS scope
    if (it != records.end()) return it->second;  // found it!
    if (parent) return parent->lookup(key);      // not here → ask parent
    return nullptr;                               // no parent → not found
}
```

### How to test it

1. Save `SemanticAnalyzer.h` with the new branch added
2. Rebuild:
```bash
make
```
3. Run with the broken test file:
```bash
./compiler test_lesson.txt
```

Expected output:
```
    @error at line 3. Undeclared identifier: 'y'
    @error at line 4. Undeclared identifier: 'z'

=== Symbol Table ===
--- Scope [global] ---
  --- Scope [main] ---
    [variable] x : int
====================

Semantic errors found: 2
```

4. Generate the visual tree:
```bash
make tree_semantic
```

5. Open `tree_semantic.pdf`:
   - `VarDecl:x` → white box (valid declaration)
   - `ID:y` → **red double-octagon** with "Undeclared identifier: 'y'"
   - `ID:z` → **red double-octagon** with "Undeclared identifier: 'z'"
   - `Int:5`, `Int:7` → white boxes (literals, no check needed)

### Verify no false positives

Restore the clean test file:
```cpm
main(): int {
    x : int
    y : int
    y := 5
    x := 7
    return 0
}
```

Run `./compiler test_lesson.txt` → "Semantic analysis passed with no errors."

The `ID:y` node now hits `st.lookup("y")` → walks to main scope → FOUND 
(we registered `y` in step 5 of the traversal) → no error. Correct!

---

## Part D: Adding Check #2 — Duplicate Variables

This check is **already implemented** in the current code. But let's 
understand exactly how it works by reading the code and triggering it.

### The test file

Change `test_lesson.txt` to:

```cpm
main(): int {
    x : int
    y : int
    x : float
    y := 5
    x := 7
    return 0
}
```

`x` is declared twice: once as `int`, once as `float`.

### Where this check lives (already in SemanticAnalyzer.h)

Find the `VarDecl` branch in `buildSymbolTable` (in `SemanticAnalyzer.h`):

```cpp
} else if (type == "VarDecl" && !value.empty()) {
    // Leaf VarDecl (value = identifier name). Find type node.
    string typeStr = "unknown";
    for (auto* child : node->children) {
        if (child && (child->type == "Type" || child->type == "ArrayType")) {
            typeStr = getTypeStr(child);
            break;
        }
    }
    if (!st.put(value, new Record(value, "variable", typeStr))) {
        reportError(node, "Already Declared variable: '" + value + "'");
    }
}
```

### What happens step by step

**Visit VarDecl:x (first time, type "int"):**
```
1. Find Type child → typeStr = "int"
2. st.put("x", new Record("x", "variable", "int"))
3. Inside put():
   → current->containsKey("x")
   → records.find("x") → NOT FOUND
   → Insert records["x"] = Record("x", "variable", "int")
   → return true
4. st.put returned true → no error
```

**Visit VarDecl:x (second time, type "float"):**
```
1. Find Type child → typeStr = "float"
2. st.put("x", new Record("x", "variable", "float"))
3. Inside put():
   → current->containsKey("x")
   → records.find("x") → FOUND! (inserted above)
   → delete record (throw away the new Record)
   → return false  ← DUPLICATE!
4. st.put returned false → enters the if block:
   → errors++ (0 → 1)
   → errorMsg = "Already Declared variable: 'x'" (stored on THIS node)
   → cerr prints the error to the terminal
```

### How `st.put()` works (in SymbolTable.h)

```cpp
bool put(const string& key, Record* record) {
    if (current->containsKey(key)) {    // ← check ONLY current scope
        delete record;                   // ← throw away the duplicate
        return false;                    // ← signal: duplicate!
    }
    current->records[key] = record;      // ← insert into the map
    return true;                         // ← signal: success
}
```

And `containsKey` (in Scope class):
```cpp
bool containsKey(const string& key) const {
    return records.find(key) != records.end();  // ← simple map lookup
}
```

**The key difference between `containsKey` and `lookup`:**
- `containsKey` checks ONLY this scope → used for duplicate detection
- `lookup` checks this scope AND all parents → used for "does it exist anywhere?"

### Run it

```bash
make
./compiler test_lesson.txt
```

Output:
```
    @error at line 4. Already Declared variable: 'x'

=== Symbol Table ===
--- Scope [global] ---
  --- Scope [main] ---
    [variable] x : int     ← first declaration wins
    [variable] y : int
====================

Semantic errors found: 1
```

Notice: symbol table has `x : int` (not `float`). The duplicate was rejected.

```bash
make tree_semantic
```

Open `tree_semantic.pdf`:
- First `VarDecl:x` → white box (valid, it was inserted)
- `VarDecl:y` → white box (no duplicate)
- Second `VarDecl:x` → **red double-octagon** with "Already Declared variable: 'x'"

---

## Part E: Adding Check #3 — Duplicate Classes (with scope nesting)

Let's test with a bigger program that has classes. Save as `test_lesson.txt`:

```cpm
class Foo {
    x : int
    bar(a : int) : int {
        return a
    }
}
class Foo {
    y : float
}
main() : int {
    return 0
}
```

This should catch: "Already Declared Class: 'Foo'"

### Where this check lives (already in SemanticAnalyzer.h)

```cpp
} else if (type == "Class") {
    // Register class name in the current (global) scope
    if (!st.put(value, new Record(value, "class", "class"))) {
        reportError(node, "Already Declared Class: '" + value + "'");
    }
    st.enterScope("class:" + value);       // ← create class scope
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
    st.exitScope();                         // ← back to global
}
```

### Understanding the scope pattern

This is the pattern that repeats for **every scope-creating node** (Class,
Method, MainStatement):

```
1. st.put(name, record)      ← register in the PARENT scope
2. st.enterScope(label)      ← create/enter child scope
3. recurse children           ← everything inside registers in child scope
4. st.exitScope()            ← back to parent
```

**Why `put` BEFORE `enterScope`?** Because the class name `Foo` belongs to
the **global** scope (other code needs to see it). The fields and methods
*inside* Foo belong to the class scope. The same applies to methods — the
method name `bar` belongs to the **class** scope, while its parameters belong
to the method scope.

### Tracing the full scope tree

```
Step 1: Visit Class:Foo (first)
        → st.put("Foo", class record) in global → OK
        → st.enterScope("class:Foo") → current moves into class scope
        
Step 2:   Visit VarDecl:x
          → st.put("x", variable, "int") in class:Foo scope → OK
          
Step 3:   Visit Method:bar
          → st.put("bar", method, "int") in class:Foo scope → OK
          → st.enterScope("method:bar") → current moves into method scope
          
Step 4:     Visit Param:a
            → st.put("a", parameter, "int") in method:bar scope → OK
            
Step 5:     Visit ReturnStatement → pass-through → visits ID:a
          → st.exitScope() → back to class:Foo
          
        → st.exitScope() → back to global

Step 6: Visit Class:Foo (second)
        → st.put("Foo", class record) in global
        → containsKey("Foo") → FOUND! → return false
        → ERROR: "Already Declared Class: 'Foo'"
        → Still enters scope (processes body), then exits
```

### Run it

```bash
make
./compiler test_lesson.txt
```

Output:
```
    @error at line 7. Already Declared Class: 'Foo'

=== Symbol Table ===
--- Scope [global] ---
  [class] Foo : class
  --- Scope [class:Foo] ---
    [variable] x : int
    [method] bar : int
    --- Scope [method:bar] ---
      [parameter] a : int
  --- Scope [class:Foo] ---
    [variable] y : float
  --- Scope [main] ---
====================

Semantic errors found: 1
```

Look at the symbol table tree — you can see the nesting:
- Global has the class record `[class] Foo`
- class:Foo has variable `x` and method `bar`
- method:bar has parameter `a`
- The second class:Foo also got its own scope (with `y`), but the class 
  record `Foo` was not added again (duplicate rejected)

```bash
make tree_semantic
```

Open `tree_semantic.pdf` — the second `Class:Foo` node is a red double-octagon.

---

## Part F: The Pattern for Adding ANY New Check

Every check you'll ever add follows the same 4-step recipe:

### Step 1: Identify the AST node type

Run `./compiler yourfile.txt` and look at the `print_tree` output. Or run 
`make tree` and open the PDF. Find the node where the check should trigger.

Examples:
- Undeclared identifier → node type is `"ID"`
- Duplicate variable → node type is `"VarDecl"` (already done)
- Type mismatch in `x + y` → node type is `"AddExpression"`
- Missing return → node type is `"Method"`
- Statement after return → node type is `"Statements"`

### Step 2: Add an `else if` branch in Node.h

Open `SemanticAnalyzer.h` → find `buildSymbolTable()` → add your branch **before the
`else` default**:

```cpp
        // ──────── YOUR NEW CHECK ────────
        } else if (type == "YOUR_NODE_TYPE") {
            // Your check logic here
            // Use st.lookup(name)  → find if something is declared
            // Use st.put(name, record) → register new things

            if (/* error condition */) {
                reportError(node, "Your error message");
            }

        } else {
            // Default pass-through (must stay LAST!)
```

### Step 3: Compile and test

```bash
make                           # rebuild
./compiler test_lesson.txt     # run semantic analysis
make tree_semantic             # generate visual tree with error highlighting
```

- Terminal shows error messages
- Symbol table printout shows what got registered
- `tree_semantic.pdf` shows red nodes where errors were found

### Step 4: Verify no false positives

```bash
for f in valid/*.cpm; do echo "$f:"; ./compiler "$f" 2>&1 | grep "error\|passed"; done
```

All should say "passed with no errors."

---

## Part G: The Three Functions You Use in Checks

Everything in `buildSymbolTable` (inside `SemanticAnalyzer.h`) comes down to three calls on the analyzer's `st` member:

### 1. `st.put(name, new Record(...))` — Register a declaration

```cpp
if (!st.put("x", new Record("x", "variable", "int"))) {
    // "x" already exists in this scope → duplicate error!
}
```

**Lives in:** `SymbolTable.h` → `SymbolTable::put()` → calls `Scope::containsKey()`

**Flow:**
```
st.put("x", record)
  → current->containsKey("x")     // check ONLY current scope's map
    → records.find("x")           // std::map lookup
    → found? → delete record, return false (DUPLICATE)
    → not found? → records["x"] = record, return true (OK)
```

### 2. `st.lookup(name)` — Check if something exists anywhere visible

```cpp
Record* r = st.lookup("y");
if (!r) {
    // "y" not declared anywhere → undeclared error!
}
```

**Lives in:** `SymbolTable.h` → `SymbolTable::lookup()` → calls `Scope::lookup()`

**Flow:**
```
st.lookup("y")
  → current->lookup("y")          // start at current scope
    → records.find("y")           // check here
    → not found? → parent->lookup("y")  // check parent
      → records.find("y")         // check there
      → not found? → parent->lookup("y")  // keep going up
        → parent == nullptr → return nullptr  // top of tree, NOT FOUND
```

### 3. `st.enterScope()` / `st.exitScope()` — Move into/out of a scope

```cpp
st.enterScope("method:foo");   // creates child scope, moves current DOWN
// ... process everything inside ...
st.exitScope();                // moves current back UP to parent
```

**Lives in:** `SymbolTable.h` → `SymbolTable::enterScope/exitScope()`

**Flow:**
```
enterScope("method:foo")
  → current = current->nextChild()   // get or create child scope
  → current->label = "method:foo"    // label it for debugging

exitScope()
  → current = current->parent        // move up one level
```

---

## Part H: What to Add Next — The Roadmap

Now that you know the pattern, here are the checks to add for Part 2. Each
one is an `else if` branch in `buildSymbolTable` in `SemanticAnalyzer.h`:

### H1. Undeclared identifiers ✓ (done in Part C)
Already added the `"ID"` branch using `st.lookup()`.

### H2. Type mismatches in assignments
**Node type:** `"AssignmentStatement"` (children: lhs-expr, rhs-expr)
**Logic:** Get the type of the left side (look up the ID in the symbol table),
get the type of the right side (literal type or looked-up type), compare them.
**Hint:** You'll want `buildSymbolTable` to return a `string` (the expression
type) instead of `void`. Change the signature to:
```cpp
string buildSymbolTable(SymbolTable& st, int& errors)
```
Then each branch returns the type of the expression it represents.

### H3. Missing return statement
**Node type:** `"Method"`
**Logic:** After recursing children, check if the method body `Statements`
node contains at least one `ReturnStatement` child. Walk the children of the
Statements node looking for it.

### H4. Statement after return
**Node type:** `"Statements"` (the container for a block body)
**Logic:** Loop through children. After you see a `ReturnStatement`, flag
any remaining siblings as unreachable.

### H5. Return type mismatch
**Node type:** `"ReturnStatement"`
**Logic:** Get the expected return type from the enclosing method's Record
(you'll need to track "current method name" as you traverse). Compare with 
the actual expression type.

### H6. Invalid operations (int + boolean, etc.)
**Node type:** `"AddExpression"`, `"SubExpression"`, etc.
**Logic:** Recurse both children to get their types. Check compatibility
rules (e.g., `int + int → OK`, `int + boolean → error`).

---

## Quick Reference

### Commands
```bash
make                           # build the compiler
./compiler test_lesson.txt     # run lex → parse → semantic analysis
make tree                      # plain AST → tree.pdf
make tree_semantic             # AST with errors highlighted → tree_semantic.pdf
make test_semantic             # run all semantic_errors/*.cpm test files
```

### Where things live
```
SemanticAnalyzer.h             ← ALL semantic checks go here (else if branches)
SymbolTable.h                  ← Data structure (Record, Scope, SymbolTable)
Node.h                         ← AST node (pure data structure, don't touch)
main.cc                        ← Wiring (already done, don't touch)
```

### The three calls
```cpp
st.put(key, record)            // register a declaration (returns false = duplicate)
st.lookup(key)                 // find a declaration (returns nullptr = not found)
st.enterScope(label)           // open a new scope
st.exitScope()                 // close current scope, go back to parent
```

### Adding a new check — recipe
```
1. Find the node type (use make tree or print_tree output)
2. Add else if (type == "...") branch in buildSymbolTable (SemanticAnalyzer.h), before the else default
3. Use st.lookup() / st.put() / reportError(node, "...")  
4. make && ./compiler test_lesson.txt && make tree_semantic
5. Check tree_semantic.pdf for red error nodes
```
