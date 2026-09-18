#include "ast.hpp"
#include <iostream>

const char* nodeName(N k) {
    switch (k) {
        case N::Program:   return "Program";
        case N::TableDecl: return "TableDecl";
        case N::LetDecl:   return "LetDecl";
        case N::Assign:    return "Assign";
        case N::Show:      return "Show";
        case N::Print:     return "Print";
        case N::If:        return "If";
        case N::While:     return "While";
        case N::Block:     return "Block";
        case N::Pipeline:  return "Pipeline";
        case N::Load:      return "Load";
        case N::TableRef:  return "TableRef";
        case N::ColDef:    return "ColDef";
        case N::Stage:     return "Stage";
        case N::AggItem:   return "AggItem";
        case N::SortKey:   return "SortKey";
        case N::IdList:    return "IdList";
        case N::Binary:    return "Binary";
        case N::Unary:     return "Unary";
        case N::IntLit:    return "IntLit";
        case N::FloatLit:  return "FloatLit";
        case N::StrLit:    return "StrLit";
        case N::BoolLit:   return "BoolLit";
        case N::Ident:     return "Ident";
    }
    return "?";
}

void dumpAst(const Node* n, int indent) {
    if (!n) return;
    std::cout << std::string(indent * 2, ' ') << nodeName(n->kind);
    if (!n->text.empty()) std::cout << " '" << n->text << "'";
    if (!n->type.empty()) std::cout << " : " << n->type;
    std::cout << "\n";
    for (const auto& k : n->kids) dumpAst(k.get(), indent + 1);
}