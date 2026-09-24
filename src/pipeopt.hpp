#pragma once
#include "tac.hpp"
#include <string>
#include <vector>

// Pipeline-level optimization (-O2). Works on the stage quadruples of the
// main stream, using what each stage means rather than what its code does:
//   1. predicate pushdown   moves a filter above derive, sort and select
//   2. projection pruning   loads only the columns something downstream reads
//   3. stage fusion         merges adjacent filter/derive stages into one
//                           row pass, so each row is visited once
struct PipeReport {
    int pushed = 0;               // filters moved earlier
    int pruned = 0;               // columns dropped at load
    int fused = 0;                // stages removed by fusion
    std::vector<std::string> log; // one line per rewrite, for --dump-pipeline
};

PipeReport optimizePipelines(Stream& main, const std::vector<Stream>& frags);
