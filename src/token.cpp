#include "token.hpp"

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Id:      return "ID";
        case Kind::Int:     return "INT";
        case Kind::Float:   return "FLOAT";
        case Kind::Str:     return "STRING";
        case Kind::Keyword: return "KEYWORD";
        case Kind::Op:      return "OP";
        case Kind::Eof:     return "EOF";
    }
    return "?";
}

const std::unordered_set<std::string> KEYWORDS = {
    "table", "let", "load", "with", "schema", "show", "print",
    "while", "if", "else", "true", "false", "asc", "desc",
    "int", "float", "bool", "string",
    "filter", "select", "derive", "group_by", "aggregate", "sort", "limit"
};

// Order matters: the lexer takes the first match, so two-character
// operators must precede the one-character operators they begin with.
const std::vector<std::string> OPERATORS = {
    "|>", "&&", "||", "<=", ">=", "==", "!=",
    "+", "-", "*", "/", "%", "<", ">", "=", "!",
    "(", ")", "{", "}", ",", ";", ":"
};