#pragma once
#include "ast.hpp"
#include "symtab.hpp"
#include "errors.hpp"

class Sema {
public:
    Sema(ErrorLog& errors) : errors_(errors) {}

    void analyze(Node* program);
    SymbolTable& table() { return sym_; }

private:
    void stmt(Node* n);
    void block(Node* n);

    // Returns the type of the expression, or "" if it could not be determined.
    std::string expr(Node* n);
    std::string binary(Node* n);
    std::string unary(Node* n);

    bool numeric(const std::string& t) const { return t == "int" || t == "float"; }
    // int promotes to float, never the reverse
    bool assignable(const std::string& from, const std::string& to) const;

    void err(const Node* n, const std::string& msg) {
        errors_.add("semantic", n->line, n->col, msg);
    }
    void typeErr(const Node* n, const std::string& msg) {
        errors_.add("type", n->line, n->col, msg);
    }

    SymbolTable sym_;
    ErrorLog& errors_;
};