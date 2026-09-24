# TabuLang

A compiler for TabuLang, a statically typed domain-specific language for data
analysis, written for the BCSE307P Compiler Design Lab Project.

Nancy Thadhani, 24BCB0084

## What it does

A TabuLang program loads a table with a declared schema and chains pipeline
stages over it with `|>`. The compiler checks every column name and type at
compile time, so a misspelled column is a compile-time error, not a runtime
failure. It then optimizes the program twice: classically, per basic block
through a DAG, and at the pipeline level, by moving filters earlier, loading
only the columns that are read, and fusing row-wise stages into one pass.
The result runs on TabVM, a stack machine written for this project.

C++17, standard library only. No parser generator, no third-party library.

## Build

    cmake -S . -B build -G Ninja
    cmake --build build

This builds three programs: `tblc` (the compiler), `runtests` (the test
runner) and `gencsv` (the benchmark data generator). After replacing source
files with older timestamps, rebuild with `cmake --build build --clean-first`.

On Windows, write `.\build\tblc.exe` wherever this file writes `./build/tblc`.

## Run a program

    ./build/tblc tests/programs/demo.tbl --run          # no optimization
    ./build/tblc tests/programs/demo.tbl --run -O1      # + DAG local optimization
    ./build/tblc tests/programs/demo.tbl --run -O2      # + pipeline optimization

`--run` prints the program's output, then the instructions executed, the rows
and cells processed by table stages, and the execution time.

Exit codes: 0 success, 1 compile error, 3 runtime error, 4 code generation error.

## Inspect one phase

Each flag prints one phase's output, so any module can be shown on its own.

| Flag | Shows |
| --- | --- |
| `--dump-tokens` | token stream with line and column |
| `--dump-ast` | abstract syntax tree |
| `--dump-symbols` | symbol table with declaration positions |
| `--dump-schemas` | inferred schema of every table |
| `--dump-tac` | three-address code (quadruples) and per-row fragments |
| `--dump-cfg` | basic blocks and control flow graph |
| `--dump-dag` | DAG of each basic block |
| `--dump-pipeline` | each pipeline rewrite applied at `-O2` |
| `--dump-bytecode` | TabVM bytecode, main program and fragments |
| `--dot-ast`, `--dot-cfg` | Graphviz DOT for the AST or the CFG |

To draw a graph: `./build/tblc tests/programs/demo.tbl --dot-cfg > cfg.dot`,
then `dot -Tpng cfg.dot -o cfg.png`, or paste the text into any online
Graphviz viewer.

## The language in one example

    table sales = load("sales.csv") with schema {
      region : string, revenue : int, units : int
    };
    let threshold : int = 1000;

    table top = sales
      |> derive(unit_price = revenue / units)
      |> filter(revenue > threshold && units > 0)
      |> group_by(region)
      |> aggregate(total = sum(revenue), avg_price = mean(unit_price))
      |> sort(total desc) |> limit(3);

    show top;

Stages: `filter`, `select`, `derive`, `group_by`, `aggregate` (with `sum`,
`count`, `mean`, `min`, `max`), `sort` (`asc` or `desc`), `limit`.
Scalars: `int`, `bool`, `string`, with `let`, assignment, `if`/`else`,
`while` and `print`. CSV paths are relative to the `.tbl` file.

## Test

    ./build/runtests            # run all 40 cases
    ./build/runtests --update   # rewrite expected outputs after a deliberate change

Cases are listed in `tests/cases.txt`; expected outputs are in `tests/expected/`.
Five cases also check that `-O0` and `-O2` print identical results.

## Benchmark

    ./build/gencsv 1000000 tests/bench/sales_bench.csv
    ./build/tblc tests/bench/bench.tbl --run
    ./build/tblc tests/bench/bench.tbl --run -O2

The generator uses a fixed seed, so row, cell and instruction counts are the
same on every machine; only the time varies.

## Known limitations

- TabVM scalar values are 64-bit integers. Float literals in scalar code are
  rejected with a clear error. Float columns are exact in aggregate, sort and
  show, but are truncated inside `filter` and `derive` expressions.
- Strings compare with `==` and `!=`; ordering comparisons on strings compare
  internal ids, not text.
- A variable redeclared in an inner scope shares one runtime slot with the
  outer variable. Semantic analysis handles the scoping correctly; code
  generation does not yet rename shadowed variables.
- No joins, window functions, user-defined functions or arrays.

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
| `pipeopt.cpp` | Predicate pushdown, projection pruning, stage fusion |
| `codegen.cpp` | TabVM bytecode |
| `vm.cpp` | Dispatch loop, operand stack, per-row fragment execution |
| `table.cpp` | Table runtime: CSV loading and the stage operators |
| `dot.cpp` | Graphviz output for the AST and CFG |
