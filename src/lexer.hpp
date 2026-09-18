#pragma once
#include "token.hpp"
#include "errors.hpp"

class Lexer {
public:
    Lexer(std::string src, ErrorLog& errors)
        : src_(std::move(src)), errors_(errors) {}

    std::vector<Token> tokens();

private:
    enum class State { Start, InId, InInt, InFrac, InStr, InComment };

    char peek(size_t k = 0) const;
    char advance();
    bool matchOperator(std::string& out);

    std::string src_;
    ErrorLog& errors_;
    size_t i_ = 0;
    int line_ = 1, col_ = 1;
};