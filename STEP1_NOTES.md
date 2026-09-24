# Step 1: table runtime (notes for the viva)

What changed
- table.hpp / table.cpp (new): Table and Cell types, CSV loader checked against
  the declared schema, and the select, group_by, aggregate, sort, limit operators.
- tacgen.cpp: the load quadruple now carries the declared schema in arg2,
  e.g. region:string,revenue:int,units:int, so the runtime can check the header.
- bytecode.hpp: TABLE_STUB becomes TABLE. Each stage is one TABLE instruction
  holding its input table, its argument and its output table.
- codegen.cpp: stage quadruples lower to TABLE instructions. Codegen tracks which
  names hold tables, so "= T7 top" becomes a table copy, not a scalar STORE.
  String literals are interned to integer ids (internString).
- vm.cpp: tables live in a second environment beside the scalars. filter and
  derive run their fragment's bytecode once per row through the same dispatch
  loop; a column shadows an outer scalar, the same rule sema uses.
- main.cpp: fragments are compiled to bytecode too; --run prints instructions
  executed plus rows and cells processed. Bug fix: -O1 now renumbers jump
  targets after the DAG removes quadruples (before, the optimized loop never ended).

Design choices to defend
1. Why intern strings? TabVM values are 64-bit integers. Mapping each distinct
   string to an id lets EQ and NE compare strings with no new opcodes.
   Limitation: < and > on strings inside filter compare ids, not text.
2. Why reuse the dispatch loop for rows? One interpreter, one set of opcodes;
   the fragment is ordinary bytecode that ends with HALT and leaves its value
   on the stack.
3. Why count rows and cells? They measure data touched, which is what the
   Step 3 pipeline passes (pushdown, fusion, pruning) are meant to reduce.

Known limitation
- Row-level arithmetic is integer. Float columns are truncated inside filter
  and derive; they are exact in aggregate, sort and show.
