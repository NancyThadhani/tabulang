# Step 2: pipeline optimizer, -O2 (notes for the viva)

What changed
- pipeopt.hpp / pipeopt.cpp (new): three passes over the stage quadruples of
  the main stream. They run before the DAG, so -O2 = pipeline passes + -O1.
- vm.cpp: new rowpass stage runs several filter/derive steps on each row in one visit.
- codegen.cpp: rowpass lowers to one TABLE instruction.
- main.cpp: -O2 and --dump-pipeline flags; --run also prints execution time.
- tools/gencsv.cpp (new): deterministic CSV generator for benchmarks.

The three passes
1. Predicate pushdown. A filter moves above a derive, sort or select directly
   before it. It is blocked when the predicate reads the column the derive
   creates (tests/programs/pushdown_blocked.tbl proves the refusal).
   Why it is safe: filter only removes rows; derive, sort and select do not
   change the columns the predicate reads, so the same rows survive.
2. Projection pruning. A backward walk computes, for every table, the columns
   anything downstream reads. The load then declares only those, so unread
   columns are never parsed or carried. A shown table needs every column.
   This is liveness analysis, applied to columns instead of variables.
3. Stage fusion. Adjacent filter and derive stages become one rowpass, so each
   row is visited once. A failing filter step stops the row before later steps.

Also true after pushdown: derive(unit_price = revenue / units) now runs only on
rows where units > 0, so a division by zero the unoptimized plan would hit is
never reached. Database optimizers behave the same way.

Correctness check: every test program prints identical output at -O0, -O1, -O2.
