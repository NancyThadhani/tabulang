# TabuLang

A compiler for TabuLang, a statically typed domain-specific language for data
analysis, written for the BCSE307P Compiler Design Lab Project.

Nancy Thadhani, 24BCB0084

## What it does

A TabuLang program loads a table with a declared schema and chains pipeline
stages over it with `|>`. The compiler checks the program statically, including
the columns and column types flowing through each stage, so a misspelled column
name is a compile-time error rather than a runtime failure.

Source text is scanned by a hand-written DFA, parsed by recursive descent with
precedence climbing, type-checked against a scoped symbol table with static
schema inference, translated to three-address code with backpatching,
partitioned into basic blocks and a control flow graph, optimized per block
through a DAG, and lowered to bytecode for TabVM.

No parser generator and no third-party library. C++17, standard library only.

## Build

    cmake -S . -B build -G Ninja
    cmake --build build

## Run

Each flag prints one phase's output, so any module can be demonstrated alone.

    ./build/tblc tests/programs/demo.tbl --dump-tokens
    ./build/tblc tests/programs/demo.tbl --dump-ast
    ./build/tblc tests/programs/demo.tbl --dump-symbols
    ./build/tblc tests/programs/demo.tbl --dump-schemas
    ./build/tblc tests/programs/demo.tbl --dump-tac
    ./build/tblc tests/programs/demo.tbl --dump-cfg
    ./build/tblc tests/programs/demo.tbl --dump-dag
    ./build/tblc tests/programs/demo.tbl --dump-tac -O1
    ./build/tblc tests/programs/scalar.tbl --dump-bytecode
    ./build/tblc tests/programs/scalar.tbl --run

## Modules

| File | Phase |
| --- | --- |
| `lexer.cpp` | DFA lexical analysis with error recovery |
| `parser.cpp` | Recursive descent, precedence climbing, panic-mode recovery |
| `ast.cpp` | Typed node tree and pretty printer |
| `symtab.cpp` | Scope stack, symbol and schema records |
| `sema.cpp` | Type checking, promotion, static schema inference |
| `tacgen.cpp` | Quadruples by syntax-directed translation, backpatching |
| `blocks.cpp` | Leader algorithm, control flow graph |
| `dag.cpp` | Per-block DAG, CSE, folding, copy propagation, dead code elimination |
| `codegen.cpp` | TabVM bytecode |
| `vm.cpp` | Dispatch loop, operand stack, scalar environment |

## Test programs

`tests/programs/` holds `demo.tbl` and `scalar.tbl` for the working cases, and
`bad_chars.tbl`, `bad_syntax.tbl`, `bad_sema.tbl` and `bad_schema.tbl`, one per
compile-time error class.

## Status

Phase 2 complete. All nine phases implemented; local optimization removes 4 of
37 quadruples on `demo.tbl` with the program's meaning preserved.

Phase 3: pipeline optimization (predicate pushdown, stage fusion, projection
pruning), the table runtime, and instrumentation.