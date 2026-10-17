# Zh

> A compiler written in C++.

## Overview

**Zh** is a compiler project written in C++. Its goal is to make linear math problems much easier to write and read.

This project is currently under development.

## Features

- [ ] Lexical analysis (Lexer)
- [ ] Syntax analysis (Parser)
- [ ] Abstract Syntax Tree (AST)
- [ ] Semantic analysis
- [ ] Intermediate Representation (IR)
- [ ] Code generation
- [ ] Error reporting

## Supported Language Features

| Feature                | Status          |
| ---------------------- | --------------- |
| Variables              | implemented |
| Data types             | implemented |
| Arithmetic expressions | implemented |
| Conditional statements | implemented |
| Loops                  | implemented |
| Functions              | implemented |
| stdlib                 | not implemented |

## Architecture

The compiler is designed as a pipeline of processing stages:

1. **Lexing** — converts source code into tokens.
2. **Parsing** — converts tokens into a syntax tree.
3. **Semantic Analysis** — checks types, declarations, and other language rules.
4. **Intermediate Representation** — transforms the syntax tree into an intermediate form.
5. **Code Generation** — generates the target code.

*The architecture may evolve as the project develops.*

## Example
int mul(int a, int b) {
    return a * b;
}

int main() {
    int x = 9;
    int y = x;
    return mul(x, y);
}

### Compilation

```bash
./compiler input_file
```

### Output

Currently, the compiler hasn't any functions that would write output to stdout, so you need to:

```bash
$LASTEXITCODE
```
## Requirements

- C++ [standard, e.g. C++17 or C++20]
- [Compiler: GCC, Clang, or MSVC]
- [Build system: CMake, Make, etc.]

## Building

Clone the repository:

```bash
git clone <repository-url>
cd <repository-directory>
```

Build the project:

```bash
mkdir build
cd build
cmake build .
cmake --build build --config Release
```

## Usage

```bash
./compiler <input-file>
```

## Roadmap

- [ ] Implement the linear mathematics directly into language
- [ ] Implement the complex numbers
- [ ] Improve the symbolic algebra
- [ ] Expand the test suite

## Contributing

Contributions, bug reports, and suggestions are welcome.

Please open an issue before making major changes.

## License

This project is licensed under the MIT license. See the `LICENSE` file for details.
