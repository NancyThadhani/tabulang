# Step 3: tests, DOT output, user instructions (notes for the viva)

What changed
- tools/runtests.cpp (new): regression runner. Each case in tests/cases.txt
  compares stdout, stderr and the exit code against tests/expected/, or (kind
  "eq") checks that -O0 and -O2 print identical results.
- tests/cases.txt + tests/expected/ (new): 40 cases across the front end,
  middle end, execution, boundaries, runtime errors and optimization.
- dot.cpp (new): --dot-ast and --dot-cfg print Graphviz DOT.
- codegen.cpp: a float literal in scalar code is now a clear error (exit 4)
  instead of silently becoming 0.
- New programs: boundary/ (6 cases), zero_guard.tbl, zero_rows.csv.
- README.md rewritten as the user instructions.

Why stdout and stderr are compared separately
When both go to one file, stdout is buffered and stderr is not, so the lines
interleave differently on different machines. Separate files make the test
deterministic.

The zero_guard result (TC-34, TC-35)
Unoptimized, derive divides by units before the filter removes units = 0, so
the program stops with division by zero at row 2. At -O2 pushdown runs the
filter first and the program succeeds. The optimizer never introduces an
error; it can remove one that would only occur on rows the program discards.
SQL engines behave the same way.

How the runner was itself checked
One expected value was changed on purpose (6600 to 6601). The runner reported
FAIL for exactly that case, and PASS again once it was restored.
