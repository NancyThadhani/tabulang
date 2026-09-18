#pragma once
#include "ast.hpp"
#include "tac.hpp"

class TacGen {
public:
    void generate(Node* program);

    const Stream& main() const { return main_; }
    const std::vector<Stream>& fragments() const { return frags_; }

    int quadCount() const;
    int tempCount() const { return tempN_; }

private:
    void stmt(Node* n, Stream& s);
    void block(Node* n, Stream& s);

    // arithmetic and relational: synthesises a place
    std::string expr(Node* n, Stream& s);
    // boolean: synthesises truelist and falselist instead of a place
    void boolExpr(Node* n, Stream& s, List& truelist, List& falselist);

    std::string pipeline(Node* n, Stream& s);
    std::string stage(Node* st, const std::string& inTable, Stream& s);

    // Builds a separate fragment for a per-row expression and returns its name.
    std::string fragmentFor(Node* e, bool asPredicate);

    std::string newTemp()  { return "t" + std::to_string(++tempN_); }
    std::string newTable() { return "T" + std::to_string(++tableN_); }

    Stream main_{"main", {}};
    std::vector<Stream> frags_;
    int tempN_ = 0, tableN_ = 0, fragN_ = 0;
};