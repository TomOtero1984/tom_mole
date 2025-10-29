# tom_mole

**A minimal LLVM-based compiler for a small experimental language.**

`tom_mole` is a small, experimental programming language implemented in C++ using the LLVM framework. Its primary goal is to demonstrate parsing, AST generation, and LLVM IR code generation in a concise compiler pipeline.

This project showcases a hands-on understanding of compiler fundamentals and LLVM's JIT and IR capabilities.

---

## Features

* Lexer and parser for a minimal imperative language with:

  * `let` statements for variable declarations
  * Arithmetic expressions: `+`, `-`, `*`, `/`
  * `print` statements
* Generates LLVM IR using `llvm::IRBuilder` and `llvm::Module`
* Supports integer arithmetic and variable scoping
* Demonstrates calling C library functions (`printf`) from generated IR
* Fully self-contained compilation pipeline, capable of producing executable LLVM IR for testing

---

## Example

**Source (`test.mole`):**

```mole
let x = 5;
let y = 10;
print x + y;
```

**LLVM IR generated (`test.ll`):**

```llvm
define i32 @main() {
entry:
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  store i32 10, ptr %y, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %addtmp = add i32 %x1, %y2
  %0 = call i32 (ptr, ...) @printf(ptr @0, i32 %addtmp)
  ret i32 0
}

declare i32 @printf(ptr, ...)
```

**Execution via LLVM JIT (`lli`):**

```bash
$ lli test.ll
15
```

---

## Build Instructions

Ensure LLVM 21+ is installed (via Homebrew or system LLVM) and `llvm-config` is available.

```bash
clang++ -std=c++17 -stdlib=libc++ \
    llvm_minicomp.cpp \
    $(llvm-config --cxxflags --ldflags --libs core orcjit native --system-libs) \
    -o llvm_minicomp
```

---

## Usage

```bash
./llvm_minicomp test.mole > test.ll
lli test.ll
```

* The compiler reads a `.mole` source file, parses it, generates LLVM IR, and prints it to stdout.
* `lli` executes the generated IR directly.

---

## Why tom_mole?

This project demonstrates:

* Practical experience with **LLVM IR generation**
* Compiler pipeline design: lexer → parser → AST → codegen
* C++11/14/17 modern features: `unique_ptr`, move semantics, `unordered_map`
* Interfacing with C library functions from LLVM IR

It is intended as a learning platform and proof-of-concept for small language design and JIT compilation.

---

## License

MIT License – free for personal and professional use.

