# astra

## Building

### Prerequisites

- CMake 3.20 or newer, with any generator of your choice (e.g. Ninja or
  Unix Makefiles).
- A C++20 compiler, e.g. clang++ 18 or a recent g++.
- A Java runtime and the ANTLR 4.13.2 complete jar. Download it from
  https://www.antlr.org/download.html (direct jar:
  https://www.antlr.org/download/antlr-4.13.2-complete.jar). CMake looks
  for the jar on `PATH` under one of `antlr.jar`, `antlr4.jar`,
  `antlr-4.jar` or `antlr-4.13.2-complete.jar`. If yours is elsewhere,
  pass `-DANTLR_EXECUTABLE=/path/to/antlr-4.13.2-complete.jar`.
- LLVM with its CMake config files installed. The configure step needs the
  `LLVM_DIR` pointing at the `lib/cmake/llvm` directory of the installation.
- Network access on the first configure. The ANTLR C++ runtime is built from
  source via `ExternalAntlr4Cpp`, and Catch2 v3.8.1 is fetched with
  FetchContent when tests are enabled.

### Configure

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
```

Useful options:

- `-DBUILD_TESTS=OFF` skips the Catch2 test targets (default is ON).
- `-DBUILD_TOOLS=OFF` skips the `astra` driver binary (default is ON).

### Build

```sh
cmake --build build                 # everything: grammar libs, libraries, driver, tests
cmake --build build --target astra-frontend   # the frontend library only
cmake --build build --target format           # run clang-format-18 over all sources
```

Notes:

- The lexer/parser are generated from the `.g4` grammars at build time into
  `build/grammars/antlr4cpp_generated_src/`. Never edit generated files.

## Running

### The driver

```sh
./build/tools/astra <source file>
```

The driver parses the file and dumps the resulting AST to stdout as a
clang-style tree, e.g.:

```
Program
`- TopLevelObject
  `- Decl
    `- FunctionDecl 'main'
      |- ReturnType
      | `- BuiltinType 'Void'
      `- Body
        `- Block
```

- `--dump-ast=false` suppresses the dump (the dump is the only output for
  now, so it defaults to on).
- The driver exits with 1 when the file cannot be opened or when the
  lexer/parser reports syntax errors.

### Tests

```sh
ctest --test-dir build                  # run every test
ctest --test-dir build --output-on-failure
ctest --test-dir build -R frontend      # filter tests by name
```

The test executables can also be run directly:

```sh
./build/tests/frontend/astra-frontend-tests
./build/tests/ast/astra-ast-tests
```
