# SymbolTable.h — Complete Explained Reference

This document explains **every line** of `SymbolTable.h`. It contains three
classes — `Record`, `Scope`, and `SymbolTable` — that together form the data
structure the compiler uses to track what names exist, where they exist, and
what types they have.

**What is a symbol table?** A symbol table is a dictionary the compiler builds
while reading your program. Every time you write `x : int` or
`bar(a : int) : int { ... }`, the compiler stores that name, its kind
(variable, method, class, parameter), and its type. Later, when the code says
`x := 5`, the compiler looks up `x` to verify it exists and check the type.

**Why a tree and not a flat dictionary?** Because programs have *scopes*.
A variable `x` inside method `bar` is different from a variable `x` at the
class level. The symbol table is a **tree of scopes**, where each scope holds
its own dictionary. When looking something up, you check the current scope
first, then walk up to the parent, then the grandparent, and so on. This is
called **scope chaining**.

---

## File Structure at a Glance

```
SymbolTable.h
├── Record     — one entry in the table (a single declared name)
├── Scope      — one scope node: holds a map of Records + parent/child links
└── SymbolTable — manager: wraps the root scope + a "current" pointer
```

---

## Class 1: Record — A Single Symbol Entry

```cpp
class Record {
public:
    string id;
    string kind;   // "class" | "method" | "variable" | "parameter"
    string type;

    Record(const string& id, const string& kind, const string& type)
        : id(id), kind(kind), type(type) {}

    void printRecord(int depth) const {
        string indent(depth * 2, ' ');
        cout << indent << "[" << kind << "] " << id << " : " << type << endl;
    }
};
```

### Fields

| Field  | What it stores | Example |
|--------|---------------|---------|
| `id`   | The identifier name as written in the source code | `"x"`, `"bar"`, `"Foo"` |
| `kind` | What category of declaration this is | `"variable"`, `"parameter"`, `"method"`, `"class"` |
| `type` | The data type of this symbol | `"int"`, `"float"`, `"boolean"`, `"int[]"`, `"class"`, `"void"` |

### Constructor

```cpp
Record(const string& id, const string& kind, const string& type)
    : id(id), kind(kind), type(type) {}
```

Uses a C++ **initializer list** (`: id(id), kind(kind), type(type)`) to set
all three fields at construction time. Nothing else happens.

**How it's called in practice:**
```cpp
new Record("x", "variable", "int")       // for:  x : int
new Record("bar", "method", "int")        // for:  bar(...) : int { ... }
new Record("Foo", "class", "class")       // for:  class Foo { ... }
new Record("a", "parameter", "int")       // for:  bar(a : int) ...
```

### printRecord()

```cpp
void printRecord(int depth) const {
    string indent(depth * 2, ' ');
    cout << indent << "[" << kind << "] " << id << " : " << type << endl;
}
```

Prints one line for debugging. `depth` controls indentation (2 spaces per
level). For a variable `x : int` at depth 2, the output is:

```
    [variable] x : int
```

This is called by `Scope::printScope()` to print every record in a scope.

---

## Class 2: Scope — One Node in the Scope Tree

This is the core of the whole data structure. Each `Scope` represents one
nesting level in the program (global, a class, a method, main).

```cpp
class Scope {
public:
    string label;                   // e.g. "class:Foo", "method:bar", "main"
    Scope* parent;
    vector<Scope*> children;
    map<string, Record*> records;   // symbols declared in this scope
    int next = 0;                   // child-visit index for tree traversal
```

### Fields

| Field      | Type | Purpose |
|-----------|------|---------|
| `label`    | `string` | Human-readable name for debugging: `"global"`, `"class:Foo"`, `"method:bar"`, `"main"` |
| `parent`   | `Scope*` | Points to the enclosing scope (one level up). `nullptr` for the global scope. |
| `children` | `vector<Scope*>` | All scopes directly nested inside this one. |
| `records`  | `map<string, Record*>` | The actual symbol dictionary: name → Record. |
| `next`     | `int` | Tracks which child scope to enter next (explained below). |

### The Scope Tree in Practice

For this program:
```cpm
class Foo {
    x : int
    bar(a : int) : int {
        return a
    }
}
main() : int {
    y : int
    return 0
}
```

The scope tree looks like:
```
Global                    ← root scope
├── records: { Foo: class }
├── class:Foo             ← child scope
│   ├── records: { x: variable/int, bar: method/int }
│   └── method:bar        ← grandchild scope
│       └── records: { a: parameter/int }
└── main                  ← child scope
    └── records: { y: variable/int }
```

### Constructor and Destructor

```cpp
explicit Scope(Scope* parent = nullptr) : parent(parent) {}
```

Creates a scope with a parent pointer. The global scope passes `nullptr`.

```cpp
~Scope() {
    for (auto& kv : records) delete kv.second;
    for (auto* child : children) delete child;
}
```

The destructor **recursively cleans up** the entire tree. When the root scope
is deleted, it deletes all its records, then deletes each child scope, which
in turn deletes their records and children, and so on. This prevents memory
leaks.

### containsKey() — Check THIS Scope Only

```cpp
bool containsKey(const string& key) const {
    return records.find(key) != records.end();
}
```

Checks if a name exists **only** in this specific scope's `records` map.
**Does NOT walk up to parent scopes.** This is crucial — it's used for
**duplicate detection**. If you declare `x : int` twice in the same method,
`containsKey("x")` returns `true` on the second attempt, and the analyzer
reports a duplicate.

**Step-by-step trace:**
```
containsKey("x")
  → records.find("x")           // std::map lookup in this scope
  → found? → return true        // "x" already declared HERE
  → not found? → return false   // "x" not in this scope (may exist in parent)
```

### lookup() — Walk Up the Scope Chain

```cpp
Record* lookup(const string& key) const {
    auto it = records.find(key);
    if (it != records.end()) return it->second;
    if (parent) return parent->lookup(key);
    return nullptr;
}
```

This is the **recursive scope-chain lookup**. It tries the current scope
first, then the parent, then the grandparent, until either the name is found
or we hit the global scope's `nullptr` parent.

**Step-by-step trace** (looking up `x` from inside `method:bar`):
```
Scope[method:bar].lookup("x")
  → records.find("x")  → NOT FOUND (bar only has "a")
  → parent exists (class:Foo) → parent->lookup("x")

Scope[class:Foo].lookup("x")
  → records.find("x")  → FOUND! (x is declared here)
  → return &Record("x", "variable", "int")
```

**If it's not found anywhere:**
```
Scope[method:bar].lookup("z")
  → NOT FOUND → parent->lookup("z")
Scope[class:Foo].lookup("z")
  → NOT FOUND → parent->lookup("z")
Scope[global].lookup("z")
  → NOT FOUND → parent == nullptr → return nullptr
```

**The key difference between `containsKey` and `lookup`:**

| Method | Checks | Used for |
|--------|--------|----------|
| `containsKey(key)` | Only THIS scope | Duplicate detection when declaring a new name |
| `lookup(key)` | This scope + all parents | Checking if a name is accessible when it's used |

### nextChild() — Navigate the Scope Tree

```cpp
Scope* nextChild() {
    if (next >= (int)children.size())
        children.push_back(new Scope(this));
    return children[next++];
}
```

This is how `enterScope` moves into child scopes. The `next` counter tracks
which child to enter. On the first traversal, `next` is 0 and `children` is
empty, so a **new child scope is created**. On subsequent traversals (after
`resetScope()`), the existing children are reused.

**Step-by-step trace** (first traversal, entering class:Foo from global):
```
global.nextChild()
  → next == 0, children.size() == 0
  → 0 >= 0? YES → push_back(new Scope(this))   // create child scope
  → children is now [Scope_0]
  → return children[0], next becomes 1
```

**Second child (entering main from global):**
```
global.nextChild()
  → next == 1, children.size() == 1
  → 1 >= 1? YES → push_back(new Scope(this))   // create second child
  → children is now [Scope_0, Scope_1]
  → return children[1], next becomes 2
```

### resetScope() — Reset for a Second Traversal

```cpp
void resetScope() {
    next = 0;
    for (auto* child : children) child->resetScope();
}
```

Resets the `next` counter to 0 in this scope and all children, recursively.
This lets you traverse the tree again from the beginning without creating
new scopes. Used by `SymbolTable::resetTable()`.

### printScope() — Debug Output

```cpp
void printScope(int depth) const {
    string indent(depth * 2, ' ');
    if (!label.empty())
        cout << indent << "--- Scope [" << label << "] ---" << endl;
    for (const auto& kv : records)
        kv.second->printRecord(depth + 1);
    for (auto* child : children)
        child->printScope(depth + 1);
}
```

Recursively prints the scope tree. Each scope prints its label, then its
records (indented one level deeper), then its children. For the example
program above:

```
--- Scope [global] ---
  [class] Foo : class
  --- Scope [class:Foo] ---
    [variable] x : int
    [method] bar : int
    --- Scope [method:bar] ---
      [parameter] a : int
  --- Scope [main] ---
    [variable] y : int
```

### generate_dot_content() — Graphviz Visualization

```cpp
void generate_dot_content(int& count, ofstream& out) const {
    int myId = count++;

    string title;
    if (label == "global")
        title = "Global";
    else if (label.size() > 6 && label.substr(0, 6) == "class:")
        title = "Class (" + label.substr(6) + ")";
    else if (label.size() > 7 && label.substr(0, 7) == "method:")
        title = "Method (" + label.substr(7) + ")";
    else if (label == "main")
        title = "Main";
    else
        title = "Inner Scope(" + label + ")";

    auto kindShort = [](const string& k) -> string {
        if (k == "variable" || k == "parameter") return "var";
        if (k == "method")  return "method";
        return k;
    };

    // ... HTML table output for Graphviz ...

    for (auto* child : children) {
        int childId = count;
        child->generate_dot_content(count, out);
        out << "  s" << myId << " -> s" << childId << ";" << endl;
    }
}
```

This generates a **Graphviz DOT** file for visual output. Each scope becomes
a table node with:
- A **red title** (e.g. "Global", "Class (Foo)", "Method (bar)")
- A **grid of records** showing id, kind (abbreviated), and type
- **Edges** connecting parent scopes to child scopes

The `count` variable (passed by reference) assigns unique IDs to each node.
The `kindShort` lambda abbreviates "variable" and "parameter" to "var" for
compact display.

**How to generate the PDF:**
```bash
make symtable    # runs: dot -Tpdf symtable.dot -o symtable.pdf
```

---

## Class 3: SymbolTable — The Manager

This is the class the `SemanticAnalyzer` interacts with. It wraps the scope
tree and provides the high-level API.

```cpp
class SymbolTable {
public:
    Scope* root;
    Scope* current;
```

### Fields

| Field     | What it is |
|----------|-----------|
| `root`    | The global scope — top of the tree. Never changes. |
| `current` | The scope we're currently "inside". Moves up and down as we enter/exit scopes. |

### Constructor

```cpp
SymbolTable() {
    root = new Scope(nullptr);        // create global scope with no parent
    root->label = "global";
    current = root;                    // start at the global scope
}
```

Creates the root scope labeled `"global"` and points `current` at it.
Everything starts here.

### Destructor

```cpp
~SymbolTable() { delete root; }
```

Deletes the root scope. Because `Scope`'s destructor recursively deletes all
children, this single `delete` cleans up the entire tree.

### enterScope() — Move Down the Tree

```cpp
void enterScope(const string& label = "") {
    current = current->nextChild();
    current->label = label;
}
```

When the analyzer encounters a class, method, or main block, it calls
`enterScope()`. This:
1. Calls `current->nextChild()` — either creates a new child scope or reuses
   an existing one
2. Updates `current` to point to that child
3. Sets the scope's label for debugging

**Trace (entering class:Foo from global):**
```
enterScope("class:Foo")
  → current is global
  → current->nextChild() → creates new child scope → returns it
  → current now points to the new child scope
  → current->label = "class:Foo"
```

After this call, any `put()` or `lookup()` calls operate on the new scope.

### exitScope() — Move Back Up

```cpp
void exitScope() {
    if (current->parent)
        current = current->parent;
}
```

After processing all children inside a class, method, or block, the analyzer
calls `exitScope()` to move `current` back to the parent scope.

**Trace (exiting class:Foo back to global):**
```
exitScope()
  → current is class:Foo
  → current->parent exists (global)
  → current = global
```

The guard `if (current->parent)` prevents moving past the root scope.

### put() — Register a Declaration

```cpp
bool put(const string& key, Record* record) {
    if (current->containsKey(key)) {
        delete record;
        return false;
    }
    current->records[key] = record;
    return true;
}
```

Attempts to add a new record to the **current** scope.

**Success path (first declaration of `x`):**
```
put("x", new Record("x", "variable", "int"))
  → current->containsKey("x") → false (not found)
  → current->records["x"] = the record
  → return true
```

**Failure path (duplicate declaration of `x`):**
```
put("x", new Record("x", "variable", "float"))
  → current->containsKey("x") → true (already exists!)
  → delete record (throw away the new Record to avoid memory leak)
  → return false
```

The caller (SemanticAnalyzer) checks the return value:
```cpp
if (!st.put(value, new Record(value, "variable", typeStr))) {
    reportError(node, "Already Declared variable: '" + value + "'");
}
```

**Important:** `put()` uses `containsKey()` (checks current scope only), NOT
`lookup()`. This means you CAN have a variable `x` in both a class and a
method — they're in different scopes. You CANNOT have two variables named `x`
in the same method.

### lookup() — Find a Declaration

```cpp
Record* lookup(const string& key) const {
    return current->lookup(key);
}
```

Delegates to the current scope's `lookup()`, which walks the parent chain.
Returns the `Record*` if found, or `nullptr` if the name doesn't exist in any
visible scope.

**Used by the analyzer to check things like:**
```cpp
Record* r = st.lookup("y");
if (!r) {
    reportError(node, "Undeclared identifier: 'y'");
}
```

### printTable() — Debug Dump

```cpp
void printTable() const {
    cout << "\n=== Symbol Table ===" << endl;
    root->printScope(0);
    cout << "====================" << endl;
}
```

Prints the entire scope tree starting from the root. This is the output you
see after running `./compiler`:

```
=== Symbol Table ===
--- Scope [global] ---
  [class] Foo : class
  --- Scope [class:Foo] ---
    [variable] x : int
    [method] bar : int
    --- Scope [method:bar] ---
      [parameter] a : int
  --- Scope [main] ---
    [variable] y : int
====================
```

### generate_dot() — Graphviz Output

```cpp
void generate_dot() const {
    ofstream out("symtable.dot");
    out << "digraph SymbolTable {" << endl;
    out << "  rankdir=TB;" << endl;                    // top-to-bottom layout
    out << "  node [fontname=\"Helvetica\", fontsize=12];" << endl;
    out << "  edge [color=black];" << endl;
    int count = 0;
    root->generate_dot_content(count, out);            // recursive content
    out << "}" << endl;
    out.close();
    printf("\nBuilt symbol table at symtable.dot. Run 'make symtable' to generate the PDF.\n");
}
```

Writes a `symtable.dot` file, then `make symtable` converts it to a PDF
where each scope is a labeled box with its records in a grid.

### resetTable() — Reset for a Second Pass

```cpp
void resetTable() {
    root->resetScope();
    current = root;
}
```

Resets all `next` counters and moves `current` back to root. Used if the
analyzer needs to traverse the tree a second time.

---

## Complete Call Flow — Putting It All Together

Here's how the three classes interact during compilation. Given this program:

```cpm
class Foo {
    x : int
    bar(a : int) : int {
        return a
    }
}
main() : int {
    y : int
    return 0
}
```

The `SemanticAnalyzer` calls these methods in this order:

```
1.  st = SymbolTable()
    → root = global scope (empty), current = root

2.  st.put("Foo", Record("Foo","class","class"))
    → global.containsKey("Foo") → false → insert → ✓
    → global.records = { Foo }

3.  st.enterScope("class:Foo")
    → global.nextChild() → create child Scope_0
    → current = Scope_0, label = "class:Foo"

4.  st.put("x", Record("x","variable","int"))
    → class:Foo.containsKey("x") → false → insert → ✓
    → class:Foo.records = { x }

5.  st.put("bar", Record("bar","method","int"))
    → class:Foo.containsKey("bar") → false → insert → ✓
    → class:Foo.records = { x, bar }

6.  st.enterScope("method:bar")
    → class:Foo.nextChild() → create child Scope_1
    → current = Scope_1, label = "method:bar"

7.  st.put("a", Record("a","parameter","int"))
    → method:bar.containsKey("a") → false → insert → ✓
    → method:bar.records = { a }

8.  st.lookup("a")                     (from return statement)
    → method:bar.lookup("a")
    → records.find("a") → FOUND → return Record("a","parameter","int")

9.  st.exitScope()
    → current = class:Foo (parent of method:bar)

10. st.exitScope()
    → current = global (parent of class:Foo)

11. st.enterScope("main")
    → global.nextChild() → create child Scope_2
    → current = Scope_2, label = "main"

12. st.put("y", Record("y","variable","int"))
    → main.containsKey("y") → false → insert → ✓
    → main.records = { y }

13. st.exitScope()
    → current = global

14. st.printTable()     → prints the tree
15. st.generate_dot()   → writes symtable.dot
```

**Final scope tree:**
```
Global { Foo:class }
├── class:Foo { x:variable/int, bar:method/int }
│   └── method:bar { a:parameter/int }
└── main { y:variable/int }
```

---

## Quick Reference

### The Three Operations for Semantic Checks

| Operation | Method | Checks | Returns | Used for |
|-----------|--------|--------|---------|----------|
| Register  | `st.put(key, record)` | Current scope only | `true` = OK, `false` = duplicate | Declarations (`VarDecl`, `Param`, `Class`, `Method`) |
| Find      | `st.lookup(key)` | Current scope + all parents | `Record*` or `nullptr` | Usage checks (`ID`, `FunctionCall`) |
| Enter     | `st.enterScope(label)` | — | — | Opening a class, method, or main block |
| Exit      | `st.exitScope()` | — | — | Closing the block, returning to parent |

### Memory Management

- Records are allocated with `new` by the caller and **owned by the Scope**.
- If `put()` rejects a duplicate, it `delete`s the record to prevent leaks.
- `Scope`'s destructor deletes all records and child scopes.
- `SymbolTable`'s destructor deletes the root scope → entire tree is freed.
