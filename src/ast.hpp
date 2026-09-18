#pragma once
#include <memory>
#include <string>
#include <vector>

enum class N {
    Program, TableDecl, LetDecl, Assign, Show, Print, If, While, Block,
    Pipeline, Load, TableRef, ColDef, Stage, AggItem, SortKey, IdList,
    Binary, Unary, IntLit, FloatLit, StrLit, BoolLit, Ident
};

const char* nodeName(N k);

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Node {
    N kind;
    std::string text;              // operator, identifier, literal, stage name
    int line = 0, col = 0;
    std::string type;              // filled in by semantic analysis later
    std::vector<NodePtr> kids;

    Node(N k, std::string t, int l, int c)
        : kind(k), text(std::move(t)), line(l), col(c) {}

    Node* add(NodePtr n) { kids.push_back(std::move(n)); return kids.back().get(); }
};

inline NodePtr mk(N k, std::string t, int l, int c) {
    return std::make_unique<Node>(k, std::move(t), l, c);
}

void dumpAst(const Node* n, int indent = 0);