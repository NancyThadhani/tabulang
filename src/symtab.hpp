#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// A column of a table: its name and its type.
struct Column {
    std::string name;
    std::string type;                 // int | float | bool | string
};

using Schema = std::vector<Column>;   // ordered, so column order is preserved

enum class SymKind { Var, Table };

struct Symbol {
    std::string name;
    SymKind kind;
    std::string type;                 // scalar type; empty for a table
    Schema schema;                    // tables only
    int line = 0, col = 0;            // where it was declared
};

// A stack of scopes. Lookup walks outward, which gives shadowing;
// declaration checks only the innermost scope, which gives redeclaration errors.
class SymbolTable {
public:
    SymbolTable() { push(); }         // the global scope

    void push();
    void pop();

    // Returns nullptr if the name is already declared in the innermost scope.
    Symbol* declare(const Symbol& s);
    Symbol* lookup(const std::string& name);
    Symbol* lookupInnermost(const std::string& name);

    size_t depth() const { return scopes_.size(); }
    void dump() const;

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};