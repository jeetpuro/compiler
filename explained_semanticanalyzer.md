# SemanticAnalyzer.h — Complete Explained Reference

This document explains **every part** of `SemanticAnalyzer.h`. This is the
file where all semantic rules live. It walks the AST (Abstract Syntax Tree)
produced by the parser, builds the symbol table, and checks for errors like
undeclared variables, type mismatches, missing returns, and invalid operations.

**What is semantic analysis?** The parser checks that your code is
*syntactically* correct (proper grammar). Semantic analysis checks that it
*makes sense*: you can't use a variable you never declared, you can't add an
`int` to a `boolean`, and a method that promises to return `int` must actually
return something.

**Architecture:** `Node.h` is just data (the AST). `SymbolTable.h` is just a
data structure (the scope tree + records). `SemanticAnalyzer.h` connects them:
it walks the AST, fills the symbol table, and enforces all the rules.

---

## File Structure at a Glance

```
SemanticAnalyzer.h
├── Public fields
│   ├── st (SymbolTable)          — the symbol table being built
│   └── errors (int)              — error counter
├── Private fields
│   ├── currentMethodRetType      — tracks expected return type during traversal
│   ├── currentClassName          — tracks which class we're inside
│   ├── classMethods              — pre-scanned map: class → method → return type
│   └── classMethodParams         — pre-scanned map: class → method → [param types]
├── Public methods
│   ├── analyze()                 — entry point (calls preScan then buildSymbolTable)
│   └── buildSymbolTable()        — the big switch: one branch per AST node type
└── Private helpers
    ├── preScanClasses()          — pre-scan pass to record all class methods
    ├── lookupMethodInClass()     — look up a method's return type in a class
    ├── lookupMethodParamsInClass() — look up a method's param types
    ├── checkFunctionArgs()       — verify argument types match parameter types
    ├── getTypeStr()              — extract type string from a Type/ArrayType node
    ├── checkBinaryOp()           — shared logic for all binary operators
    ├── checkArrayAccess()        — validate array subscript operations
    ├── hasReturn()               — check if a node contains a return statement
    └── reportError()             — tag error on AST node and print it
```

---

## Fields — What the Analyzer Tracks

```cpp
class SemanticAnalyzer {
public:
    SymbolTable st;
    int errors = 0;

private:
    string currentMethodRetType = "";
    string currentClassName = "";
    map<string, map<string, string>> classMethods;
    map<string, map<string, vector<string>>> classMethodParams;
```

| Field | Type | Purpose |
|-------|------|---------|
| `st` | `SymbolTable` | The symbol table — stores all declarations in a scope tree |
| `errors` | `int` | Counts how many semantic errors were found |
| `currentMethodRetType` | `string` | While inside a method, holds its declared return type (e.g. `"int"`). Used by `ReturnStatement` to check if the returned value matches. |
| `currentClassName` | `string` | While inside a class, holds its name (e.g. `"Foo"`). Used by `FunctionCall` to resolve bare method calls within the class. |
| `classMethods` | nested map | Maps `className → methodName → returnType`. Populated once by `preScanClasses()` so that dot-calls like `obj.method()` can determine the return type even if the class was declared later in the file. |
| `classMethodParams` | nested map | Maps `className → methodName → [paramTypes]`. Same pre-scan, used to verify argument counts and types when calling methods. |

### Why pre-scan?

Consider:
```cpm
class A {
    doStuff() : int {
        volatile b : B
        return b.getValue()     ← B is declared AFTER A
    }
}
class B {
    getValue() : int { return 42 }
}
```

Without pre-scanning, when the analyzer reaches `b.getValue()`, it doesn't
know that class `B` has a method `getValue` that returns `int`. The pre-scan
runs first and records all class methods, so forward references work.

---

## analyze() — The Entry Point

```cpp
void analyze(Node* root) {
    if (!root) return;
    preScanClasses(root);   // must run first so all class method types are known
    buildSymbolTable(root);
    st.generate_dot();
}
```

Three steps, in order:
1. **preScanClasses** — Quick pass over the AST: find all `Class` nodes,
   record every method's return type and parameter types into `classMethods`
   and `classMethodParams`.
2. **buildSymbolTable** — Full pass: walk every node, register declarations,
   check all semantic rules.
3. **generate_dot** — Write the symbol table to `symtable.dot` for
   visualization.

This is called from `main.cc` after the parser produces the AST.

---

## buildSymbolTable() — The Main Traversal

This is the heart of the analyzer — one giant `if/else if` chain. Each
branch handles a specific AST node type. The method returns a `string`
representing the **type** of the expression (e.g. `"int"`, `"boolean"`,
`"float"`, `"unknown"`). This is how type checking works: each node evaluates
its children and returns a type, and the parent node compares child types.

```cpp
string buildSymbolTable(Node* node) {
    if (!node) return "";
    node->visited = true;
    const string& type  = node->type;
    const string& value = node->value;
```

Every call starts by:
1. Null-checking the node
2. Marking `visited = true` — so the visual debugger colors it green
3. Extracting `type` (the AST node type, e.g. `"VarDecl"`) and `value`
   (the identifier name, e.g. `"x"`)

Then the long chain of branches begins. Here is every branch explained:

---

### Branch: Program

```cpp
if (type == "Program") {
    // Pre-register every class name into global scope
    for (auto* child : node->children) {
        if (!child) continue;
        if (child->type == "Classes") {
            for (auto* cls : child->children) {
                if (cls && cls->type == "Class")
                    st.put(cls->value, new Record(cls->value, "class", "class"));
            }
        } else if (child->type == "Class") {
            st.put(child->value, new Record(child->value, "class", "class"));
        }
    }
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
}
```

**What it does:**
1. **Pre-registers all class names** in the global scope before processing anything.
   This is necessary because a variable might be declared with a class type
   (`d : MyClass`) before that class's `Class` node is visited. Without
   pre-registration, the type check for `d`'s type would fail.
2. **Recurses into all children** — the `Class` nodes, `MainStatement`, etc.

**Why two loops?** The first loop only registers names. The second loop does
the full traversal. This guarantees all class names are available before any
class body is processed.

**The `Classes` vs `Class` check:** Depending on the grammar, class nodes
might be wrapped in a `Classes` container node or appear directly as children
of `Program`. Both cases are handled.

---

### Branch: Class

```cpp
} else if (type == "Class") {
    string savedClassName = currentClassName;
    currentClassName = value;
    st.enterScope("class:" + value);

    // Pre-register all method signatures
    for (auto* child : node->children) {
        if (!child || child->type != "Methods") continue;
        for (auto* method : child->children) {
            if (!method || method->type != "Method") continue;
            string retType = "unknown";
            for (auto* mc : method->children) {
                if (mc && (mc->type == "Type" || mc->type == "ArrayType")) {
                    retType = getTypeStr(mc);
                    break;
                }
            }
            if (!st.put(method->value, new Record(method->value, "method", retType))) {
                reportError(method, "Already Declared Function: '" + method->value + "'");
            }
        }
    }

    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
    st.exitScope();
    currentClassName = savedClassName;
}
```

**What it does step by step:**

1. **Save/set `currentClassName`** — Saves the old value (in case of nested
   processing) and sets it to this class's name. This is used later by
   `FunctionCall` to know which class's methods to look up for bare calls.

2. **Enter the class scope** — `st.enterScope("class:Foo")` creates a child
   scope under global and moves `current` into it.

3. **Pre-register all methods** — Before recursing into the class body, scan
   all `Method` nodes and register their names and return types. This ensures
   that if method `a1` calls method `a2`, and `a2` is declared after `a1`,
   the lookup still works.

   For each method, it finds the `Type` or `ArrayType` child node to
   determine the return type, then calls `st.put()`. If `put()` returns
   `false`, it means a method with that name already exists in this scope →
   duplicate method error.

4. **Recurse** — Process all children (variables, methods, etc.)

5. **Exit scope and restore** — `st.exitScope()` moves back to the global
   scope. `currentClassName` is restored to its previous value.

**Why NOT re-register the class name here?** The class name was already
registered in the `Program` branch's pre-registration loop. If we tried
`st.put("Foo", ...)` again here, it would always return `false` (duplicate).
So the `Class` branch skips that and just opens the scope.

---

### Branch: Method

```cpp
} else if (type == "Method") {
    string retType = "unknown";
    for (auto* child : node->children) {
        if (child && (child->type == "Type" || child->type == "ArrayType")) {
            retType = getTypeStr(child);
            break;
        }
    }
    string savedRetType = currentMethodRetType;
    currentMethodRetType = retType;
    st.enterScope("method:" + value);
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
    st.exitScope();
    currentMethodRetType = savedRetType;

    // Check for missing return in non-void method
    if (retType != "void" && retType != "unknown") {
        Node* stmtsNode = nullptr;
        for (auto* child : node->children)
            if (child && child->type == "Statements") { stmtsNode = child; break; }
        if (stmtsNode && !hasReturn(stmtsNode)) {
            reportError(node, "Missing return statement in non-void method '" + value + "'");
        }
    }
}
```

**What it does:**

1. **Determine the return type** — Scan children for a `Type` or `ArrayType`
   node, extract the type string (e.g. `"int"`, `"float"`, `"int[]"`).

2. **Save/set `currentMethodRetType`** — The return type is stored so that
   when the traversal later hits a `ReturnStatement` inside this method,
   it can compare the returned expression's type against the expected type.

3. **Enter scope, recurse, exit scope** — Same pattern as `Class`.

4. **Missing return check** — After processing the method body, if the method
   is non-void, find the `Statements` node and use the `hasReturn()` helper
   to check whether there's at least one `ReturnStatement` somewhere in the
   body. If not, report an error.

**Note:** The method's name is NOT registered here — it was already
pre-registered in the `Class` branch above.

**Example trace for:**
```cpm
bar(a : int) : int {
    return a
}
```
```
1. retType = "int" (found Type:int child)
2. currentMethodRetType = "int"
3. enterScope("method:bar")
4.   Visit Param:a → registers "a" as parameter
5.   Visit Statements → Visit ReturnStatement → checks type
6. exitScope()
7. retType is "int" (non-void) → find Statements node → hasReturn? YES → OK
```

---

### Branch: Param

```cpp
} else if (type == "Param") {
    string typeStr = "unknown";
    if (!node->children.empty() && node->children.front())
        typeStr = getTypeStr(node->children.front());
    if (!st.put(value, new Record(value, "parameter", typeStr))) {
        reportError(node, "Already Declared parameter: '" + value + "'");
    }
}
```

A `Param` node represents a method parameter like `a : int`. Its `value` is
the parameter name (`"a"`), and its only child is a `Type` node.

1. Extract the type from the child node.
2. Register it in the current scope (which is the method scope).
3. If `put()` fails → duplicate parameter name.

No recursion needed — Param's only child is a Type, which has no semantic
significance to process further.

---

### Branch: VarDecl

```cpp
} else if (type == "VarDecl" && !value.empty()) {
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

    // Type existence check for class types
    {
        string baseType = typeStr;
        if (baseType.size() > 2 && baseType.substr(baseType.size()-2) == "[]")
            baseType = baseType.substr(0, baseType.size()-2);
        bool isPrimitive = (baseType == "int" || baseType == "float" ||
                            baseType == "boolean" || baseType == "void" ||
                            baseType == "unknown");
        if (!isPrimitive) {
            Record* cls = st.lookup(baseType);
            if (!cls || cls->kind != "class")
                reportError(node, "Undeclared type: '" + baseType + "'");
        }
    }
}
```

**What it does:**

1. **Extract the type** from the child `Type` or `ArrayType` node.
2. **Register** the variable in the current scope. Report duplicate if `put()` fails.
3. **Check that the type exists** — If the type isn't a primitive (`int`,
   `float`, `boolean`, `void`), it must be a declared class name. For example,
   `d : MyClass` requires that `MyClass` was declared as a class somewhere.
   Array types like `MyClass[]` are handled by stripping the `[]` suffix
   before looking up.

**Why `!value.empty()`?** In the AST, `VarDecl` nodes with an empty value are
intermediate grouping nodes (e.g. a container for multiple declarations).
Only leaf `VarDecl` nodes with actual identifier names need processing.

**Example trace for `d : MyClass`:**
```
1. typeStr = "MyClass"
2. st.put("d", Record("d", "variable", "MyClass")) → OK
3. baseType = "MyClass", isPrimitive? NO
4. st.lookup("MyClass") → found Record("MyClass", "class", "class")? 
   → YES → ok, no error
   → NO  → reportError("Undeclared type: 'MyClass'")
```

---

### Branch: MainStatement

```cpp
} else if (type == "MainStatement") {
    st.enterScope("main");
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
    st.exitScope();
}
```

The `main()` function. Creates a `"main"` scope, processes its body, then
exits. The simplest scope-creating branch — no pre-registration needed.

---

### Branch: Statements (Unreachable Code Detection)

```cpp
} else if (type == "Statements") {
    bool seenReturn = false;
    for (auto* child : node->children) {
        if (!child) continue;
        if (seenReturn) {
            reportError(child, "Unreachable statement after return");
            buildSymbolTable(child);  // still recurse for the debugger
        } else {
            buildSymbolTable(child);
            if (child->type == "ReturnStatement") seenReturn = true;
        }
    }
}
```

Iterates through the statements in a block. Once a `ReturnStatement` is
encountered, every subsequent statement is flagged as **unreachable**.

**Why still recurse after flagging?** So that the `visited` field is set on
unreachable nodes, which makes the visual debugger color them (green with a
red error overlay).

**Example:**
```cpm
bar() : int {
    return 5
    x := 7        ← unreachable!
    return 10      ← also unreachable!
}
```
```
1. Visit ReturnStatement → process it → seenReturn = true
2. Visit AssignmentStatement(x := 7) → seenReturn is true → ERROR
3. Visit ReturnStatement → seenReturn is true → ERROR
```

---

### Branch: ReturnStatement (Return Type Check)

```cpp
} else if (type == "ReturnStatement") {
    if (!node->children.empty()) {
        string retType = buildSymbolTable(node->children.front());
        if (!currentMethodRetType.empty()
            && currentMethodRetType != "unknown"
            && retType != "unknown"
            && retType != currentMethodRetType) {
            reportError(node, "Return type mismatch: expected '"
                + currentMethodRetType + "', got '" + retType + "'");
        }
    }
}
```

Evaluates the returned expression (the child node), gets its type, and
compares it against `currentMethodRetType` (set by the enclosing `Method`
branch).

**Example:**
```cpm
bar() : int {
    return true        ← returns boolean, but method expects int!
}
```
```
1. currentMethodRetType = "int" (set by Method branch)
2. Evaluate child: Bool:true → returns "boolean"
3. "boolean" != "int" → ERROR: "Return type mismatch: expected 'int', got 'boolean'"
```

**Guard conditions:** The check skips if any type is `"unknown"` or empty.
This prevents cascading errors — if evaluating the child already produced an
error (returned `"unknown"`), don't report a second error about the return
type.

---

### Branch: IfElseStatement

```cpp
} else if (type == "IfElseStatement") {
    printf("Entering if-else scope\n");
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
}
```

Simple pass-through with a debug print. Recurses into the condition
expression (which gets type-checked) and the if/else bodies (which get
their statements checked).

---

### Branch: ID (Undeclared Identifier Check)

```cpp
} else if (type == "ID") {
    Record* r = st.lookup(value);
    if (!r) {
        reportError(node, "Undeclared identifier: '" + value + "'");
        return "unknown";
    }
    return r->type;
}
```

When an identifier is **used** (not declared — declarations are handled by
`VarDecl` and `Param`), this branch fires. It looks up the name in the
symbol table. If not found, it's an error. If found, it **returns the type**
so the parent expression can type-check.

**Example trace for `y := 5` where `y : int` was declared:**
```
AssignmentStatement visits children:
  → ID:y → st.lookup("y") → FOUND (Record: variable, int) → return "int"
  → Int:5 → return "int"
  → "int" == "int" → no error
```

**If `y` was never declared:**
```
  → ID:y → st.lookup("y") → nullptr → ERROR → return "unknown"
```

---

### Branches: Literal Types (Int, Float, Bool)

```cpp
} else if (type == "Int") {
    return "int";
} else if (type == "Float") {
    return "float";
} else if (type == "Bool") {
    return "boolean";
}
```

Literals have known types. `Int:5` returns `"int"`, `Float:3.14` returns
`"float"`, `Bool:true` returns `"boolean"`. These are leaf nodes — no
children to process.

---

### Branch: AssignmentStatement (Type Mismatch Check)

```cpp
} else if (type == "AssignmentStatement") {
    string lhsType = "";
    string rhsType = "";
    int idx = 0;
    for (auto* child : node->children) {
        if (!child) continue;
        string t = buildSymbolTable(child);
        if (idx == 0) lhsType = t;
        else if (idx == 1) rhsType = t;
        idx++;
    }
    if (!lhsType.empty() && !rhsType.empty()
        && lhsType != "unknown" && rhsType != "unknown"
        && lhsType != rhsType) {
        reportError(node, "Type mismatch in assignment: '" + lhsType + "' := '" + rhsType + "'");
    }
}
```

An assignment has two children: the left-hand side (what's being assigned to)
and the right-hand side (the value). Both are recursively evaluated to get
their types, then compared.

**Example trace for `x := true` where `x : int`:**
```
1. child[0] = ID:x → st.lookup("x") → returns "int" → lhsType = "int"
2. child[1] = Bool:true → returns "boolean" → rhsType = "boolean"
3. "int" != "boolean" → ERROR: "Type mismatch in assignment: 'int' := 'boolean'"
```

---

### Branches: Arithmetic Operators (+, -, *, /, ^)

```cpp
} else if (type == "AddExpression")  { return checkBinaryOp(node, "+",  "numeric", "");
} else if (type == "SubExpression")  { return checkBinaryOp(node, "-",  "numeric", "");
} else if (type == "MultExpression") { return checkBinaryOp(node, "*",  "numeric", "");
} else if (type == "DivExpression")  { return checkBinaryOp(node, "/",  "numeric", "");
} else if (type == "PowerExpression"){ return checkBinaryOp(node, "^",  "numeric", "");
```

All five arithmetic operators use the shared `checkBinaryOp()` helper.

- **requiredType = `"numeric"`** → both operands must be `int` or `float`,
  and they must match each other.
- **resultType = `""`** (empty) → the result type is the same as the operand
  type (`int + int → int`, `float + float → float`).

**Valid:** `5 + 3` → `int + int → int`, `3.14 * 2.0` → `float * float → float`

**Invalid:** `5 + 3.14` → `int + float` → error (types must match),
`5 + true` → `int + boolean` → error (boolean is not numeric)

---

### Branches: Logical Operators (&, |, !)

```cpp
} else if (type == "AndExpression")  { return checkBinaryOp(node, "&",  "boolean", "boolean");
} else if (type == "OrExpression")   { return checkBinaryOp(node, "|",  "boolean", "boolean");
```

- **requiredType = `"boolean"`** → both operands must be `boolean`
- **resultType = `"boolean"`** → result is always `boolean`

```cpp
} else if (type == "NegationExpression") {
    if (node->children.empty()) return "unknown";
    string operandType = buildSymbolTable(node->children.front());
    if (operandType == "unknown") return "unknown";
    if (operandType != "boolean") {
        reportError(node, "invalid operand type for '!': expected 'boolean', got '" + operandType + "'");
        return "unknown";
    }
    return "boolean";
}
```

Negation (`!`) is unary — only one operand. It must be `boolean`, and the
result is `boolean`.

---

### Branches: Comparison Operators (<, >, <=, >=)

```cpp
} else if (type == "LessExpression")  { return checkBinaryOp(node, "<",  "numeric", "boolean");
} else if (type == "MoreExpression")  { return checkBinaryOp(node, ">",  "numeric", "boolean");
} else if (type == "LessEqExpression"){ return checkBinaryOp(node, "<=", "numeric", "boolean");
} else if (type == "MoreEqExpression"){ return checkBinaryOp(node, ">=", "numeric", "boolean");
```

- **requiredType = `"numeric"`** → both operands must be the same numeric type
- **resultType = `"boolean"`** → comparisons always produce `boolean`

**Valid:** `5 < 10` → `int < int → boolean`

**Invalid:** `5 < true` → `int < boolean` → error

---

### Branches: Equality Operators (=, !=)

```cpp
} else if (type == "EqExpression")    { return checkBinaryOp(node, "=",  "", "boolean");
} else if (type == "NotEqExpression") { return checkBinaryOp(node, "!=", "", "boolean");
```

- **requiredType = `""`** (empty) → operands can be any type, but must match
  each other
- **resultType = `"boolean"`** → the result of equality check is `boolean`

**Valid:** `5 = 5` → `int = int → boolean`, `true != false` → `boolean != boolean → boolean`

**Invalid:** `5 = true` → `int = boolean` → error (types don't match)

---

### Branch: ArrayExpression (Array Access Check)

```cpp
} else if (type == "ArrayExperssion") {
    return checkArrayAccess(node);
}
```

Delegates to the `checkArrayAccess()` helper (detailed below under Helpers).
Validates that:
- The variable being indexed is actually an array type
- The index is an `int`
- The index is not a function call

---

### Branch: FunctionCall (Method Invocation Check)

```cpp
} else if (type == "FunctionCall") {
    bool isDotCall = !node->children.empty()
                  && node->children.front()->type != "Expression";
    if (isDotCall) {
        // Dot-call: obj.method()
        string receiverType = buildSymbolTable(node->children.front());
        if (receiverType == "unknown") return "unknown";

        auto it = node->children.begin();
        ++it;
        Node* exprChild = (it != node->children.end() && (*it)->type == "Expression") ? *it : nullptr;
        checkFunctionArgs(node, exprChild, receiverType, value);

        string retType = lookupMethodInClass(receiverType, value);
        if (retType.empty()) {
            reportError(node, "Undeclared method '" + value + "' in class '" + receiverType + "'");
            return "unknown";
        }
        return retType;
    } else {
        // Bare call: method()
        Node* exprChild = (!node->children.empty() && node->children.front()->type == "Expression")
                           ? node->children.front() : nullptr;
        checkFunctionArgs(node, exprChild, currentClassName, value);

        Record* r = st.lookup(value);
        if (!r) {
            reportError(node, "Undeclared method: '" + value + "'");
            return "unknown";
        }
        if (r->kind == "class") return r->id;
        return r->type;
    }
}
```

This is the most complex branch. There are two kinds of function calls:

**1. Dot-calls (`obj.method()`):**

The first child is the receiver expression (e.g. `ID:obj`). It's evaluated
to get the receiver's type (which is a class name). Then:
- `checkFunctionArgs()` verifies argument count and types
- `lookupMethodInClass()` looks up the method's return type in the
  pre-scanned `classMethods` map
- If the method doesn't exist in that class → error

**Example trace for `b.getValue()` where `b : MyClass`:**
```
1. isDotCall = true (first child is ID:b, not Expression)
2. Evaluate ID:b → st.lookup("b") → Record(variable, "MyClass") → receiverType = "MyClass"
3. checkFunctionArgs(node, nullptr, "MyClass", "getValue") → check arg count
4. lookupMethodInClass("MyClass", "getValue") → "int" (from pre-scan)
5. return "int"
```

**2. Bare calls (`method()`):**

No receiver — the method is called within the current class. The first child
(if any) is the `Expression` node containing arguments.
- `checkFunctionArgs()` verifies arguments using `currentClassName`
- `st.lookup(value)` checks if the method exists
- Special case: if the lookup finds a **class** record (e.g. `MyClass()`),
  it's treated as a **constructor call** and returns the class name as the type

**Example trace for `bar(5)` inside class `Foo`:**
```
1. isDotCall = false
2. exprChild = Expression node with children [Int:5]
3. checkFunctionArgs(node, exprChild, "Foo", "bar") → check args
4. st.lookup("bar") → Record("bar", "method", "int") → return "int"
```

**Constructor call example: `MyClass()`:**
```
1. st.lookup("MyClass") → Record("MyClass", "class", "class")
2. r->kind == "class" → return "MyClass"  (the instance type)
```

---

### Branch: LengthFunction

```cpp
} else if (type == "LengthFunction") {
    if (node->children.empty()) return "unknown";
    string operandType = buildSymbolTable(node->children.front());
    if (operandType == "unknown") return "unknown";
    if (operandType.size() < 2 || operandType.substr(operandType.size() - 2) != "[]") {
        reportError(node, "'.length' applied to non-array type '" + operandType + "'");
        return "unknown";
    }
    return "int";
}
```

Handles `arr.length`. The operand must be an array type (ending in `[]`).
The result is always `"int"`.

**Valid:** `myArr.length` where `myArr : int[]` → returns `"int"`

**Invalid:** `x.length` where `x : int` → error: `.length` on non-array

---

### Branch: Expression (Argument List Wrapper)

```cpp
} else if (type == "Expression") {
    string firstType = "";
    for (auto* child : node->children) {
        if (!child) continue;
        string t = buildSymbolTable(child);
        if (firstType.empty()) firstType = t;
    }
    return !firstType.empty() ? firstType : "unknown";
}
```

An `Expression` node wraps the argument list in a function call. When it
has one child, it acts as a transparent wrapper and returns that child's type.
When it has multiple children (multiple arguments), each is evaluated, and
the first child's type is returned. The actual argument type checking is done
by `checkFunctionArgs()`, not here.

---

### Default Branch

```cpp
} else {
    for (auto* child : node->children)
        if (child) buildSymbolTable(child);
}
return "";
```

Any node type not explicitly handled (e.g. `Methods`, `Params`, `Classes`,
`Type`, or any other structural node) just recurses into its children.
This is the "pass-through" — the node itself doesn't need checking, but its
children might.

---

## Private Helpers

### preScanClasses() — Forward Reference Resolution

```cpp
void preScanClasses(Node* node) {
    if (!node) return;
    if (node->type == "Class") {
        const string& className = node->value;
        for (auto* child : node->children) {
            if (!child || child->type != "Methods") continue;
            for (auto* method : child->children) {
                if (!method || method->type != "Method") continue;
                string retType = "unknown";
                for (auto* mc : method->children) {
                    if (mc && (mc->type == "Type" || mc->type == "ArrayType")) {
                        retType = getTypeStr(mc);
                        break;
                    }
                }
                classMethods[className][method->value] = retType;

                vector<string> paramTypes;
                for (auto* mc : method->children) {
                    if (mc && mc->type == "Params") {
                        for (auto* param : mc->children) {
                            if (param && param->type == "Param" && !param->children.empty())
                                paramTypes.push_back(getTypeStr(param->children.front()));
                        }
                    }
                }
                classMethodParams[className][method->value] = paramTypes;
            }
        }
        return;  // don't recurse further into class internals
    }
    for (auto* child : node->children)
        if (child) preScanClasses(child);
}
```

Runs before `buildSymbolTable()`. It walks the AST looking for `Class` nodes.
For each class, it records every method's return type and parameter types into
two maps:

- `classMethods["Foo"]["bar"] = "int"` — method `bar` in class `Foo` returns `int`
- `classMethodParams["Foo"]["bar"] = {"int", "float"}` — method `bar` takes
  an `int` and a `float`

**Why not use the symbol table for this?** Because the symbol table is built
top-down during the main traversal. When processing class `A`, class `B`
might not have been visited yet. The pre-scan is a quick, lightweight pass
that only collects method signatures — it doesn't register anything in the
symbol table.

---

### lookupMethodInClass() and lookupMethodParamsInClass()

```cpp
string lookupMethodInClass(const string& className, const string& methodName) const {
    auto cit = classMethods.find(className);
    if (cit == classMethods.end()) return "";
    auto mit = cit->second.find(methodName);
    if (mit == cit->second.end()) return "";
    return mit->second;
}

vector<string> lookupMethodParamsInClass(const string& className, const string& methodName) const {
    auto cit = classMethodParams.find(className);
    if (cit == classMethodParams.end()) return {};
    auto mit = cit->second.find(methodName);
    if (mit == cit->second.end()) return {};
    return mit->second;
}
```

Simple map lookups into the pre-scanned data. Used by `FunctionCall` and
`checkFunctionArgs()` to resolve methods in a specific class.

---

### checkFunctionArgs() — Argument Type Verification

```cpp
void checkFunctionArgs(Node* callNode, Node* exprNode,
                       const string& className, const string& methodName) {
    vector<string> expectedParams = lookupMethodParamsInClass(className, methodName);

    vector<string> argTypes;
    if (exprNode) {
        for (auto* arg : exprNode->children) {
            if (!arg) continue;
            argTypes.push_back(buildSymbolTable(arg));
        }
    }

    // Skip if method params unknown
    if (!classMethodParams.count(className)
        || !classMethodParams.at(className).count(methodName)) return;

    // Check argument count
    if (argTypes.size() != expectedParams.size()) {
        reportError(callNode, "Wrong number of arguments for '" + methodName
            + "': expected " + to_string(expectedParams.size())
            + ", got " + to_string(argTypes.size()));
        return;
    }

    // Check each argument type
    for (size_t i = 0; i < argTypes.size(); i++) {
        if (argTypes[i] == "unknown" || expectedParams[i] == "unknown") continue;
        if (argTypes[i] != expectedParams[i]) {
            reportError(callNode, "Argument type mismatch for '" + methodName
                + "': parameter " + to_string(i + 1)
                + " expected '" + expectedParams[i]
                + "', got '" + argTypes[i] + "'");
        }
    }
}
```

**What it does:**

1. Look up the expected parameter types from the pre-scan.
2. Evaluate each argument expression to get the actual types.
3. Compare argument count — if they don't match, report immediately.
4. Compare each argument type — skip `"unknown"` to avoid cascading errors.

**Example for `bar(true)` where `bar(a : int) : int`:**
```
1. expectedParams = ["int"]
2. Evaluate Bool:true → argTypes = ["boolean"]
3. Count: 1 == 1 → OK
4. argTypes[0] = "boolean" != expectedParams[0] = "int"
   → ERROR: "Argument type mismatch for 'bar': parameter 1 expected 'int', got 'boolean'"
```

---

### getTypeStr() — Extract Type from AST Node

```cpp
static string getTypeStr(const Node* node) {
    if (node->type == "ArrayType" && !node->children.empty())
        return node->children.front()->value + "[]";
    return node->value;
}
```

A simple utility:
- For `Type:int` → returns `"int"`
- For `ArrayType` with child `Type:int` → returns `"int[]"`

Used everywhere types need to be read from the AST.

---

### checkBinaryOp() — Shared Binary Operator Logic

```cpp
string checkBinaryOp(Node* node, const string& op,
                     const string& requiredType, const string& resultType) {
    string lhsType = "", rhsType = "";
    int idx = 0;
    for (auto* child : node->children) {
        if (!child) continue;
        string t = buildSymbolTable(child);
        if (idx == 0) lhsType = t;
        else if (idx == 1) rhsType = t;
        idx++;
    }
    if (lhsType == "unknown" || rhsType == "unknown") return "unknown";

    bool valid;
    string effectiveResult = resultType;

    if (requiredType == "numeric") {
        bool isNumeric = (lhsType == "int" || lhsType == "float");
        valid = isNumeric && (lhsType == rhsType);
        if (resultType.empty())
            effectiveResult = lhsType;  // int+int→int, float+float→float
    } else if (requiredType.empty()) {
        valid = (lhsType == rhsType);   // equality: any matching type
    } else {
        valid = (lhsType == requiredType && rhsType == requiredType);
    }

    if (!valid) {
        reportError(node, "invalid operand type for '" + op + "': "
                          + lhsType + " " + op + " " + rhsType);
        return "unknown";
    }
    return effectiveResult;
}
```

This is the **workhorse** for all binary operators. Every operator
(`+`, `-`, `*`, `/`, `^`, `&`, `|`, `<`, `>`, `<=`, `>=`, `=`, `!=`) calls
this with different parameters.

**Step-by-step:**
1. **Evaluate both operands** by recursing into the two children.
2. **Skip if unknown** — if either operand already had an error, don't cascade.
3. **Check validity** based on `requiredType`:

| `requiredType` | Rule | Used by |
|----------------|------|---------|
| `"numeric"` | Both must be the same AND be `int` or `float` | `+`, `-`, `*`, `/`, `^`, `<`, `>`, `<=`, `>=` |
| `"boolean"` | Both must be `boolean` | `&`, `\|` |
| `""` (empty) | Both must match (any type) | `=`, `!=` |

4. **Determine result type** based on `resultType`:

| `resultType` | Behavior | Used by |
|--------------|----------|---------|
| `""` (empty) | Result = same as operand type | `+`, `-`, `*`, `/`, `^` |
| `"boolean"` | Result is always `boolean` | `&`, `\|`, `<`, `>`, `<=`, `>=`, `=`, `!=` |

**Example: `5 + 3`**
```
1. lhsType = "int", rhsType = "int"
2. requiredType = "numeric": isNumeric("int") = true, "int" == "int" → valid
3. resultType is "" → effectiveResult = "int"
4. return "int"
```

**Example: `5 + true`**
```
1. lhsType = "int", rhsType = "boolean"
2. requiredType = "numeric": isNumeric("int") = true, "int" != "boolean" → INVALID
3. ERROR: "invalid operand type for '+': int + boolean"
4. return "unknown"
```

---

### checkArrayAccess() — Array Subscript Validation

```cpp
string checkArrayAccess(Node* node) {
    if (node->children.size() < 2) return "unknown";

    auto it = node->children.begin();
    Node* arrChild = *it;
    ++it;
    Node* idxChild = *it;

    string arrType = buildSymbolTable(arrChild);
    string idxType = buildSymbolTable(idxChild);

    if (idxChild->type == "FunctionCall") {
        reportError(node, "Function call not valid as array index: '" + idxChild->value + "()'");
        return "unknown";
    }

    if (arrType == "unknown" || idxType == "unknown") return "unknown";

    if (idxType != "int") {
        reportError(node, "Array index must be 'int', got '" + idxType + "'");
        return "unknown";
    }

    if (arrType.size() < 2 || arrType.substr(arrType.size() - 2) != "[]") {
        reportError(node, "Subscript applied to non-array type '" + arrType + "'");
        return "unknown";
    }

    return arrType.substr(0, arrType.size() - 2);  // strip "[]"
}
```

Handles array subscript expressions like `arr[i]`. Performs three checks:

1. **Function calls as indices are rejected** — `arr[foo()]` is not allowed,
   even if `foo()` returns `int`. This is a deliberate language restriction.
2. **Index must be `int`** — `arr[3.14]` or `arr[true]` are errors.
3. **Operand must be an array** — `x[0]` where `x : int` is an error.

**Return type:** Strips `[]` from the array type. `int[]` → `int`,
`float[]` → `float`.

---

### hasReturn() — Recursive Return Finder

```cpp
static bool hasReturn(Node* node) {
    if (!node) return false;
    if (node->type == "ReturnStatement") return true;
    for (auto* child : node->children)
        if (hasReturn(child)) return true;
    return false;
}
```

Recursively checks whether any descendant of a node is a `ReturnStatement`.
Used by the `Method` branch to detect missing returns. It searches the entire
subtree, so a return inside an `if` body counts (even though it might not
always execute — this is a simplified check).

---

### reportError() — Error Reporting

```cpp
void reportError(Node* node, const string& msg) {
    errors++;
    node->errorMsg = msg;
    cerr << "\t@error at line " << node->lineno << ". " << msg << endl;
}
```

Called whenever a semantic error is found. Does three things:
1. Increments the `errors` counter
2. Stores the error message on the AST node itself — the visual debugger
   reads this to render the node as a red double-octagon
3. Prints the error to `stderr` with the line number

---

## Complete Error Catalog

Here is every error the analyzer can report, which branch triggers it, and
when:

| Error | Branch | Triggered when |
|-------|--------|---------------|
| `Already Declared Function: 'X'` | Class | Two methods with the same name in one class |
| `Already Declared parameter: 'X'` | Param | Two parameters with the same name in one method |
| `Already Declared variable: 'X'` | VarDecl | Variable name reused in the same scope |
| `Undeclared type: 'X'` | VarDecl | Variable declared with a non-existent class type |
| `Unreachable statement after return` | Statements | Code after a return statement |
| `Return type mismatch: expected 'X', got 'Y'` | ReturnStatement | Returned value doesn't match method's declared type |
| `Missing return statement in non-void method 'X'` | Method | Non-void method has no return |
| `Undeclared identifier: 'X'` | ID | Using a variable/name that was never declared |
| `Type mismatch in assignment: 'X' := 'Y'` | AssignmentStatement | Assigning a value to a variable of a different type |
| `invalid operand type for 'OP': X OP Y` | Binary operators | Operand types don't match the operator's rules |
| `invalid operand type for '!': ...` | NegationExpression | Negating a non-boolean |
| `Undeclared method 'X' in class 'Y'` | FunctionCall (dot) | Calling a method that doesn't exist in the class |
| `Undeclared method: 'X'` | FunctionCall (bare) | Calling a method that doesn't exist |
| `Wrong number of arguments for 'X'` | checkFunctionArgs | Calling with too many or too few arguments |
| `Argument type mismatch for 'X'` | checkFunctionArgs | Argument type doesn't match parameter type |
| `'.length' applied to non-array type 'X'` | LengthFunction | Using `.length` on a non-array |
| `Array index must be 'int', got 'X'` | checkArrayAccess | Non-integer array index |
| `Subscript applied to non-array type 'X'` | checkArrayAccess | Indexing a non-array variable |
| `Function call not valid as array index` | checkArrayAccess | Using `foo()` as an array index |

---

## How Types Flow Through the Tree

The key design insight is that `buildSymbolTable()` **returns a type string**.
This creates a bottom-up flow:

```
         AssignmentStatement  ← compares lhsType vs rhsType
        /                   \
   ID:x → "int"         AddExpression → "int"  ← compares lhs vs rhs
                        /            \
                   Int:5 → "int"   Int:3 → "int"
```

Each leaf node returns its type. Each operator node evaluates its children,
checks the rules, and returns the result type. Assignment and return nodes
use the types to verify compatibility.

When an error occurs, the node returns `"unknown"`. Parent nodes skip
their checks when they see `"unknown"`, preventing cascading errors:

```
         AssignmentStatement  ← "int" vs "unknown" → skip (no second error)
        /                   \
   ID:x → "int"         AddExpression → "unknown"  ← error already reported
                        /            \
                   Int:5 → "int"   Bool:true → "boolean"
```

---

## Quick Reference

### Testing Commands
```bash
make                           # build the compiler
./compiler test.txt            # run semantic analysis on a file
make tree_semantic             # generate tree_semantic.pdf with error highlighting
make symtable                  # generate symtable.pdf with scope visualization
make test_semantic             # run all semantic_errors/*.cpm test files
```

### Where all the checks live — by node type
```
SemanticAnalyzer::buildSymbolTable()
├── Program         → pre-register class names, recurse
├── Class           → enterScope, pre-register methods, recurse, exitScope
├── Method          → enterScope, recurse, exitScope, check missing return
├── Param           → register parameter in symbol table
├── VarDecl         → register variable, check type exists
├── MainStatement   → enterScope, recurse, exitScope
├── Statements      → unreachable code after return
├── ReturnStatement → check return type matches method declaration
├── IfElseStatement → recurse (pass-through with debug print)
├── ID              → check identifier is declared, return its type
├── Int/Float/Bool  → return literal type
├── AssignmentStatement → check lhs/rhs type match
├── Add/Sub/Mult/Div/Power Expression → checkBinaryOp (numeric)
├── And/Or Expression   → checkBinaryOp (boolean)
├── NegationExpression  → check operand is boolean
├── Less/More/LessEq/MoreEq Expression → checkBinaryOp (numeric → boolean)
├── Eq/NotEq Expression → checkBinaryOp (any matching → boolean)
├── ArrayExpression     → checkArrayAccess
├── FunctionCall        → dot-call or bare call, check args, return type
├── LengthFunction      → check operand is array, return int
├── Expression          → evaluate children, return first type
└── default             → recurse children (pass-through)
```
