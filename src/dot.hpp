#pragma once
#include "ast.hpp"
#include "tac.hpp"
#include <vector>

// Graphviz DOT output. Paste the text into any Graphviz viewer, or run
//   dot -Tpng file.dot -o file.png
void dotAst(const Node* root);
void dotCfg(const std::vector<Stream>& streams);   // one cluster per stream
