# x86-64 Assembly — What AsmCodeGen.cc Generates

This document explains every instruction category that `AsmCodeGen.cc` emits,
using `valid/test1.cpm` as the concrete example.

Source program:
```cpm
main(): int {
  volatile result : float := ( (2 ^ (4 * 2) + 10 - 2 * 6 + 1.0 / 4.0) * 2 )
  print(result)
  return 0
}
```

---

## 1. The .rodata Section — Constants

```asm
    .section .rodata
.LC2:
    .double 4
.LC3:
    .double 2
.LC4:
    .double 10
.LC5:
    .double 6
.LC6:
    .double 1.0
.LC7:
    .double 4.0
.fmt:
    .string "%g\n"
```

**Why:** SSE2 floating-point registers (`%xmm0`–`%xmm15`) cannot be loaded with
an immediate constant directly (unlike integer registers). Every numeric literal
from the TAC is stored as an 8-byte IEEE 754 double in read-only memory.

`.double N` — tells the assembler to store the value N as a 64-bit float.  
`.string "…"` — null-terminated byte string for `printf`.

The labels (`.LC0`, `.LC1`, …) are local to the file. Code accesses them with
**RIP-relative addressing**: `.LC2(%rip)` means "the address of .LC2 relative
to the current instruction pointer" — the standard position-independent way to
read data in x86-64.

---

## 2. Function Prologue

```asm
main:
    pushq   %rbp          ; save caller's base pointer
    movq    %rsp, %rbp    ; set our base pointer to current stack top
    subq    $80, %rsp     ; reserve 80 bytes for locals (9 doubles × 8 bytes, rounded to 16)
```

**Stack frame layout for test1** (offsets from `%rbp`):

| Offset     | Variable |
|------------|----------|
| `-8(%rbp)` | `t0`     |
| `-16(%rbp)`| `t1`     |
| `-24(%rbp)`| `t2`     |
| `-32(%rbp)`| `t3`     |
| `-40(%rbp)`| `t4`     |
| `-48(%rbp)`| `t5`     |
| `-56(%rbp)`| `t6`     |
| `-64(%rbp)`| `t7`     |
| `-72(%rbp)`| `result` |

The x86-64 ABI requires the stack to be **16-byte aligned** before any `call`.
After `pushq %rbp` (8 bytes) the stack is 16-byte aligned, so we subtract a
multiple of 16 (80) to stay aligned.

---

## 3. SSE2 Floating-Point Instructions

All arithmetic in the generated code uses **SSE2 scalar double** instructions.
They operate on the low 64 bits of an XMM register.

### movsd — Move Scalar Double

```asm
movsd   .LC2(%rip), %xmm0      ; load constant 4.0 from rodata into xmm0
movsd   -8(%rbp), %xmm0        ; load variable t0 from stack into xmm0
movsd   %xmm0, -8(%rbp)        ; store xmm0 to stack slot t0
```

`movsd src, dst` — copies one 64-bit double. Source or destination can be
memory or an XMM register (but not both memory at once).

### Arithmetic instructions (two-operand form)

```asm
mulsd   .LC3(%rip), %xmm0      ; xmm0 = xmm0 * 2   (t0 = 4 * 2)
addsd   .LC4(%rip), %xmm0      ; xmm0 = xmm0 + 10  (t2 = t1 + 10)
subsd   -32(%rbp), %xmm0       ; xmm0 = xmm0 - t3  (t4 = t2 - t3)
divsd   .LC7(%rip), %xmm0      ; xmm0 = xmm0 / 4.0 (t5 = 1.0 / 4.0)
```

All follow the pattern:
```
[op]sd  src, %xmm0     ; xmm0 = xmm0 [op] src
```
The source (`src`) can be a memory operand (stack slot or rodata label) directly —
no need to load it into a second register first.

### How the codegen uses them (pattern for every binary TAC op)

```
TAC:  t0 = 4 * 2
```
```asm
movsd   .LC2(%rip), %xmm0     ; load src1 (4) into xmm0
mulsd   .LC3(%rip), %xmm0     ; xmm0 *= src2 (2)
movsd   %xmm0, -8(%rbp)       ; store result into t0
```

---

## 4. Calling pow (Exponentiation)

```asm
; TAC: t1 = pow(2, t0)
movsd   .LC3(%rip), %xmm0     ; first argument  → %xmm0
movsd   -8(%rbp), %xmm1       ; second argument → %xmm1
call    pow@PLT               ; call C library pow(); result in %xmm0
movsd   %xmm0, -16(%rbp)      ; store result into t1
```

**x86-64 calling convention for floating-point arguments:**
- First float arg → `%xmm0`, second → `%xmm1`, etc.
- Float return value comes back in `%xmm0`.

`pow@PLT` — `@PLT` (Procedure Linkage Table) is how GAS calls a function from a
shared library (libm) in a position-independent way. The linker fills in the
real address at load time.

---

## 5. Printing — printf

```asm
; TAC: print result
leaq    .fmt(%rip), %rdi       ; 1st arg: format string pointer → %rdi
movsd   -72(%rbp), %xmm0      ; 2nd arg: the double value     → %xmm0
movb    $1, %al                ; tell printf: 1 XMM register is used
call    printf@PLT
```

**x86-64 integer/pointer arguments** go in: `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`.  
`leaq label(%rip), %rdi` — loads the address of the format string.

The `movb $1, %al` is required by the x86-64 ABI for **variadic functions**
like `printf`: `%al` must contain the count of XMM registers used for arguments.

---

## 6. Control Flow

### Unconditional jump (TAC: `goto BN`)

```asm
jmp     B3       ; unconditional jump to basic block B3
```

### Conditional jump (TAC: `IfFalseGoto cond BN`)

```asm
; jump to BN if cond == 0.0 (false)
movsd   -32(%rbp), %xmm0      ; load condition value
xorpd   %xmm1, %xmm1          ; xmm1 = 0.0
comisd  %xmm1, %xmm0          ; compare xmm0 with 0.0, sets ZF/CF
je      B4                    ; jump if equal (cond was 0 → false)
```

`comisd` performs an **ordered comparison** of two doubles and sets the CPU
flags (`ZF`, `CF`, `PF`) without modifying any register.

### Comparison result stored as a double (TAC: `t = a < b`)

```asm
movsd   -24(%rbp), %xmm0
movsd   -8(%rbp), %xmm1
comisd  %xmm1, %xmm0          ; compare xmm0 vs xmm1, sets flags
setbe   %al                   ; set %al = 1 if below-or-equal (for CmpLE)
movzbl  %al, %eax             ; zero-extend to 32-bit
cvtsi2sd %eax, %xmm0          ; convert integer 0 or 1 to double
movsd   %xmm0, -32(%rbp)      ; store boolean result (0.0 or 1.0)
```

`setCC` variants used per comparison:

| TAC op | setCC  | meaning           |
|--------|--------|-------------------|
| `<`    | `setb` | below (unsigned)  |
| `<=`   | `setbe`| below or equal    |
| `>`    | `seta` | above             |
| `>=`   | `setae`| above or equal    |
| `==`   | `sete` | equal             |
| `!=`   | `setne`| not equal         |

---

## 7. Function Epilogue — return 0

```asm
xorl    %eax, %eax    ; set return value to 0 (integer, for main)
leave                 ; mov %rbp, %rsp  +  pop %rbp  (restores stack frame)
ret                   ; pop return address and jump to it
```

`leave` is shorthand for:
```asm
movq    %rbp, %rsp
popq    %rbp
```
It undoes the prologue and restores the caller's stack frame.

For non-main functions returning a double, the result would be placed in
`%xmm0` before `leave`/`ret`.

---

## Full output.s for test1 (annotated)

```asm
    .section .rodata
.LC2:   .double 4       ; constant "4"
.LC3:   .double 2       ; constant "2"
.LC4:   .double 10      ; constant "10"
.LC5:   .double 6       ; constant "6"
.LC6:   .double 1.0     ; constant "1.0"
.LC7:   .double 4.0     ; constant "4.0"
.fmt:   .string "%g\n"

    .text
    .globl  main
    .type   main, @function
main:
    pushq   %rbp
    movq    %rsp, %rbp
    subq    $80, %rsp
B0:
    movsd   .LC2(%rip), %xmm0   ; load 4
    mulsd   .LC3(%rip), %xmm0   ; * 2  → t0 = 8
    movsd   %xmm0, -8(%rbp)

    movsd   .LC3(%rip), %xmm0   ; load 2 (base for pow)
    movsd   -8(%rbp), %xmm1    ; load t0 = 8 (exponent)
    call    pow@PLT             ; t1 = 2^8 = 256
    movsd   %xmm0, -16(%rbp)

    movsd   -16(%rbp), %xmm0   ; load t1 = 256
    addsd   .LC4(%rip), %xmm0  ; + 10 → t2 = 266
    movsd   %xmm0, -24(%rbp)

    movsd   .LC3(%rip), %xmm0  ; load 2
    mulsd   .LC5(%rip), %xmm0  ; * 6  → t3 = 12
    movsd   %xmm0, -32(%rbp)

    movsd   -24(%rbp), %xmm0   ; load t2 = 266
    subsd   -32(%rbp), %xmm0   ; - t3 = 12 → t4 = 254
    movsd   %xmm0, -40(%rbp)

    movsd   .LC6(%rip), %xmm0  ; load 1.0
    divsd   .LC7(%rip), %xmm0  ; / 4.0 → t5 = 0.25
    movsd   %xmm0, -48(%rbp)

    movsd   -40(%rbp), %xmm0   ; load t4 = 254
    addsd   -48(%rbp), %xmm0   ; + t5 = 0.25 → t6 = 254.25
    movsd   %xmm0, -56(%rbp)

    movsd   -56(%rbp), %xmm0   ; load t6 = 254.25
    mulsd   .LC3(%rip), %xmm0  ; * 2 → t7 = 508.5
    movsd   %xmm0, -64(%rbp)

    movsd   -64(%rbp), %xmm0   ; load t7
    movsd   %xmm0, -72(%rbp)   ; result = t7

    leaq    .fmt(%rip), %rdi    ; printf format string
    movsd   -72(%rbp), %xmm0   ; value to print
    movb    $1, %al
    call    printf@PLT          ; prints 508.5

    xorl    %eax, %eax          ; return 0
    leave
    ret
```
