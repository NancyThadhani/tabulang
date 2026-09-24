#include "dot.hpp"
#include "blocks.hpp"
#include <iostream>

// Escapes text for a DOT label.
static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

static int emitAst(const Node* n, int& next) {
    int id = next++;
    std::string label = nodeName(n->kind);
    if (!n->text.empty()) label += "\\n" + esc(n->text);
    if (!n->type.empty()) label += " : " + esc(n->type);
    std::cout << "  n" << id << " [label=\"" << label << "\"];\n";
    for (const auto& k : n->kids) {
        if (!k) continue;
        int child = emitAst(k.get(), next);
        std::cout << "  n" << id << " -> n" << child << ";\n";
    }
    return id;
}

void dotAst(const Node* root) {
    std::cout << "digraph AST {\n  node [shape=box, fontname=\"Helvetica\"];\n";
    int next = 0;
    if (root) emitAst(root, next);
    std::cout << "}\n";
}

void dotCfg(const std::vector<Stream>& streams) {
    std::cout << "digraph CFG {\n  node [shape=box, fontname=\"Courier\"];\n";
    for (const auto& s : streams) {
        Cfg cfg(s);
        std::cout << "  subgraph cluster_" << s.name << " {\n"
                  << "    label=\"" << s.name << "\";\n";
        for (const auto& b : cfg.blocks()) {
            std::string label = "B" + std::to_string(b.id) + "\\l";
            for (int i = b.first; i <= b.last; ++i) {
                const Quad& q = s.code[i];
                label += std::to_string(i) + ": " + esc(q.op) + " " + esc(q.a1) +
                         " " + esc(q.a2) + " " + esc(q.res) + "\\l";
            }
            std::cout << "    " << s.name << "_B" << b.id
                      << " [label=\"" << label << "\"];\n";
        }
        for (const auto& b : cfg.blocks())
            for (int t : b.succ)
                std::cout << "    " << s.name << "_B" << b.id << " -> "
                          << s.name << "_B" << t << ";\n";
        std::cout << "  }\n";
    }
    std::cout << "}\n";
}
