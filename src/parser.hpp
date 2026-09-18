#pragma once
#include "ast.hpp"
#include "token.hpp"
#include "errors.hpp"

class Parser {
public:
    Parser(std::vector<Token> toks, ErrorLog& errors)
        : t_(std::move(toks)), errors_(errors) {}

    NodePtr parseProgram();

private:
    const Token& peek(size_t k = 0) const;
    bool is(const char* text, size_t k = 0) const;
    const Token& advance();
    bool accept(const char* text);
    bool expect(const char* text, const char* context);
    void error(const std::string& msg);
    void sync();

    NodePtr stmt();
    NodePtr tableDecl();
    NodePtr letDecl();
    NodePtr assignOrError();
    NodePtr block();
    NodePtr pipeline();
    NodePtr stage();
    NodePtr idList();
    std::string typeName();

    NodePtr expr(int minBp = 0);
    NodePtr unary();
    NodePtr primary();

    std::vector<Token> t_;
    size_t i_ = 0;
    ErrorLog& errors_;
};