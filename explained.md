# How `b := x.a1(y.a1(a4()).a4()).a3()` type-checks as valid

## The core confusion

> "a1 doesn't contain a4 — so how can you call `.a4()` on it?"

You are calling `.a4()` **not on the method `a1`**, but on **the value that `a1` returns**.

`a1` is declared as:
```
a1(num: int): A { ... }
```
It promises to return an object of type **`A`**.  
Class `A` has these methods defined on it: `a1`, `a2`, `a3`, `a4`, `a5`.  
So any expression of type `A` can have those methods called on it — including the return value of `a1(...)`.

---

## Step-by-step type resolution

The expression is: `b := x.a1(y.a1(a4()).a4()).a3()`

The AST the parser builds for the right-hand side looks like this:

```
FunctionCall:a3                        ← outermost: something.a3()
  FunctionCall:a1                      ← the receiver of .a3()  →  x.a1(...)
    ID:x                               ← receiver of outer a1   →  x : A
    Expression:                        ← argument passed to a1
      FunctionCall:a4                  ← something.a4()
        FunctionCall:a1                ← the receiver of .a4()  →  y.a1(...)
          ID:y                         ← receiver of inner a1   →  y : A
          Expression:                  ← argument passed to inner a1
            FunctionCall:a4            ← bare call  →  a4()
```

Read from the **inside out**:

| Step | What evaluates | Receiver type | Arg type | Expected | ✓/✗ | Result type |
|------|----------------|--------------|----------|----------|-----|-------------|
| 1 | `a4()` bare call | `A` (`currentClassName`) | — | — | ✓ | **`int`** |
| 2 | `y.a1(int)` | `y : A` | `int` | `int` ✓ | ✓ | **`A`** |
| 3 | `(A).a4()` | result of step 2 = `A` | — | — | ✓ | **`int`** |
| 4 | `x.a1(int)` | `x : A` | `int` | `int` ✓ | ✓ | **`A`** |
| 5 | `(A).a3()` | result of step 4 = `A` | — | — | ✓ | **`boolean`** |
| 6 | `b := boolean` | — | — | `boolean` ✓ | ✓ | — |

Every argument type matches its parameter, and every `.method()` is called on a value of type `A`, which defines all those methods. **No error.**

---

## How the analyzer resolves types for chained calls

In `SemanticAnalyzer.h`, when a `FunctionCall` node is evaluated:

1. **Is it a dot-call?**  
   A dot-call is detected by checking if `children[0]` is something other than an `Expression` node. The first child is the *receiver expression*.

2. **What type is the receiver?**  
   `buildSymbolTable(children[0])` is called recursively. This walks down the whole inner chain until it resolves a concrete type (e.g. `A`).

3. **Look up the method on that type.**  
   `lookupMethodInClass(receiverType, methodName)` checks the pre-scanned map built by `preScanClasses()`. That map records every method's **declared return type** from the method signature, not from whether the body is correct.

4. **Return the declared return type.**  
   This becomes the type available to the next outer call to chain on.

---

## Why the bad `return this` inside `a1` does not matter here

`a1` has a bug: `return this` causes `"Undeclared identifier: 'this'"`.  
The `ReturnStatement` checker in the analyzer emits that error, but then the method's declared return type in the **pre-scanned map** is still `A` (taken from the signature `a1(num: int): A`).

When any call site does `x.a1(...)`, the FunctionCall handler looks up `a1` in the pre-scanned map, gets `A`, and returns `A` as the result type. The body error is already reported separately. The analyzer deliberately keeps the declared type usable so errors inside one method don't cause spurious false-alarms everywhere that method is called.

---

## What would actually be an error on that line

The comment in the test file asks: *"y.a1(...) returns A, then .a4() invalid on A?"*

That would only be true if `A` did **not** define `a4`. But `A` does define `a4(): int`. So `.a4()` on an `A`-typed value is perfectly legal.

The line **has no semantic error**. The comment in the `.cpm` file is incorrect about this particular line.
