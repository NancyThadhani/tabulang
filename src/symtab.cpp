#include "symtab.hpp"
#include <iostream>

void SymbolTable::push() { scopes_.emplace_back(); }

void SymbolTable::pop() {
    if (scopes_.size() > 1) scopes_.pop_back();
}

Symbol* SymbolTable::declare(const Symbol& s) {
    auto& top = scopes_.back();
    if (top.count(s.name)) return nullptr;      // redeclaration in this scope
    auto it = top.emplace(s.name, s).first;
    return &it->second;
}

Symbol* SymbolTable::lookupInnermost(const std::string& name) {
    auto& top = scopes_.back();
    auto it = top.find(name);
    return it == top.end() ? nullptr : &it->second;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return &f->second;   // innermost wins: shadowing
    }
    return nullptr;
}

void SymbolTable::dump() const {
    std::cout << "symbol table\n";
    for (size_t d = 0; d < scopes_.size(); ++d) {
        std::cout << "  scope " << d << ":\n";
        for (const auto& kv : scopes_[d]) {
            const Symbol& s = kv.second;
            std::cout << "    " << (s.kind == SymKind::Table ? "table " : "var   ")
                      << s.name;
            if (!s.type.empty()) std::cout << " : " << s.type;
            if (s.kind == SymKind::Table) {
                std::cout << " {";
                for (size_t i = 0; i < s.schema.size(); ++i)
                    std::cout << (i ? ", " : "") << s.schema[i].name
                              << ":" << s.schema[i].type;
                std::cout << "}";
            }
            std::cout << "   declared @" << s.line << ":" << s.col << "\n";
        }
    }
}