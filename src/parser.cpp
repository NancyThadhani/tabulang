#include "parser.hpp"

namespace {
int bp(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
    return 0;
}
bool isStageWord(const std::string& s) {
    return s == "filter" || s == "select" || s == "derive" || s == "group_by"
        || s == "aggregate" || s == "sort" || s == "limit";
}
}

const Token& Parser::peek(size_t k) const {
    size_t j = i_ + k;
    return t_[j < t_.size() ? j : t_.size() - 1];
}

bool Parser::is(const char* text, size_t k) const { return peek(k).text == text; }

const Token& Parser::advance() {
    const Token& c = peek();
    if (peek().kind != Kind::Eof) ++i_;
    return c;
}

bool Parser::accept(const char* text) {
    if (is(text)) { advance(); return true; }
    return false;
}

bool Parser::expect(const char* text, const char* context) {
    if (accept(text)) return true;
    error(std::string("expected '") + text + "' " + context +
          ", found '" + peek().text + "'");
    return false;
}

void Parser::error(const std::string& msg) {
    errors_.add("syntax", peek().line, peek().col, msg);
}

void Parser::sync() {
    while (peek().kind != Kind::Eof) {
        if (is(";")) { advance(); return; }
        if (is("}")) return;
        advance();
    }
}

NodePtr Parser::parseProgram() {
    auto root = mk(N::Program, "", 1, 1);
    while (peek().kind != Kind::Eof) {
        size_t before = i_;
        if (auto s = stmt()) root->add(std::move(s));
        if (i_ == before) advance();
    }
    return root;
}

NodePtr Parser::stmt() {
    if (is("table"))  return tableDecl();
    if (is("let"))    return letDecl();
    if (is("{"))      return block();

    if (is("show")) {
        auto t = advance();
        if (peek().kind != Kind::Id) { error("expected a table name after 'show'"); sync(); return nullptr; }
        auto name = advance();
        auto n = mk(N::Show, name.text, t.line, t.col);
        expect(";", "after show");
        return n;
    }
    if (is("print")) {
        auto t = advance();
        auto n = mk(N::Print, "", t.line, t.col);
        expect("(", "after 'print'");
        if (auto e = expr()) n->add(std::move(e)); else { sync(); return nullptr; }
        expect(")", "after the print expression");
        expect(";", "after print");
        return n;
    }
    if (is("if")) {
        auto t = advance();
        auto n = mk(N::If, "", t.line, t.col);
        expect("(", "after 'if'");
        if (auto c = expr()) n->add(std::move(c)); else { sync(); return nullptr; }
        expect(")", "after the if condition");
        if (auto s = stmt()) n->add(std::move(s));
        if (accept("else")) { if (auto e = stmt()) n->add(std::move(e)); }
        return n;
    }
    if (is("while")) {
        auto t = advance();
        auto n = mk(N::While, "", t.line, t.col);
        expect("(", "after 'while'");
        if (auto c = expr()) n->add(std::move(c)); else { sync(); return nullptr; }
        expect(")", "after the while condition");
        if (auto b = stmt()) n->add(std::move(b));
        return n;
    }
    if (peek().kind == Kind::Id && is("=", 1)) return assignOrError();

    error("expected a statement, found '" + peek().text + "'");
    sync();
    return nullptr;
}

NodePtr Parser::tableDecl() {
    auto t = advance();
    if (peek().kind != Kind::Id) { error("expected a table name after 'table'"); sync(); return nullptr; }
    auto name = advance();
    auto n = mk(N::TableDecl, name.text, t.line, t.col);
    if (!expect("=", "after the table name")) { sync(); return nullptr; }
    if (auto p = pipeline()) n->add(std::move(p)); else { sync(); return nullptr; }
    expect(";", "after the table declaration");
    return n;
}

NodePtr Parser::letDecl() {
    auto t = advance();
    if (peek().kind != Kind::Id) { error("expected a variable name after 'let'"); sync(); return nullptr; }
    auto name = advance();
    auto n = mk(N::LetDecl, name.text, t.line, t.col);
    expect(":", "after the variable name");
    n->type = typeName();
    if (!expect("=", "after the declared type")) { sync(); return nullptr; }
    if (auto e = expr()) n->add(std::move(e)); else { sync(); return nullptr; }
    expect(";", "after the declaration");
    return n;
}

NodePtr Parser::assignOrError() {
    auto name = advance();
    advance();                                // '='
    auto n = mk(N::Assign, name.text, name.line, name.col);
    if (auto e = expr()) n->add(std::move(e)); else { sync(); return nullptr; }
    expect(";", "after the assignment");
    return n;
}

NodePtr Parser::block() {
    auto t = advance();
    auto n = mk(N::Block, "", t.line, t.col);
    while (!is("}") && peek().kind != Kind::Eof) {
        size_t before = i_;
        if (auto s = stmt()) n->add(std::move(s));
        if (i_ == before) advance();
    }
    expect("}", "to close the block");
    return n;
}

std::string Parser::typeName() {
    if (is("int") || is("float") || is("bool") || is("string")) return advance().text;
    error("expected a type name, found '" + peek().text + "'");
    return "int";
}

NodePtr Parser::pipeline() {
    auto t = peek();
    auto pipe = mk(N::Pipeline, "", t.line, t.col);

    if (is("load")) {
        auto lt = advance();
        auto load = mk(N::Load, "", lt.line, lt.col);
        expect("(", "after 'load'");
        if (peek().kind != Kind::Str) { error("expected a file name string"); return nullptr; }
        load->text = advance().text;
        expect(")", "after the file name");
        expect("with", "after load(...)");
        expect("schema", "after 'with'");
        expect("{", "to open the schema");
        while (peek().kind == Kind::Id) {
            auto c = advance();
            auto col = mk(N::ColDef, c.text, c.line, c.col);
            expect(":", "after the column name");
            col->type = typeName();
            load->add(std::move(col));
            if (!accept(",")) break;
        }
        expect("}", "to close the schema");
        pipe->add(std::move(load));
    } else if (peek().kind == Kind::Id) {
        auto r = advance();
        pipe->add(mk(N::TableRef, r.text, r.line, r.col));
    } else {
        error("expected 'load' or a table name, found '" + peek().text + "'");
        return nullptr;
    }

    while (accept("|>")) {
        if (auto s = stage()) pipe->add(std::move(s)); else return nullptr;
    }
    return pipe;
}

NodePtr Parser::idList() {
    auto n = mk(N::IdList, "", peek().line, peek().col);
    while (peek().kind == Kind::Id) {
        auto c = advance();
        n->add(mk(N::Ident, c.text, c.line, c.col));
        if (!accept(",")) break;
    }
    return n;
}

NodePtr Parser::stage() {
    if (!isStageWord(peek().text)) {
        error("expected a stage name after '|>', found '" + peek().text + "'");
        return nullptr;
    }
    auto s = advance();
    auto n = mk(N::Stage, s.text, s.line, s.col);
    expect("(", "after the stage name");

    if (s.text == "filter") {
        if (auto e = expr()) n->add(std::move(e)); else return nullptr;
    } else if (s.text == "select" || s.text == "group_by") {
        n->add(idList());
    } else if (s.text == "derive") {
        if (peek().kind != Kind::Id) { error("expected a new column name"); return nullptr; }
        auto c = advance();
        auto item = mk(N::AggItem, c.text, c.line, c.col);
        expect("=", "after the derived column name");
        if (auto e = expr()) item->add(std::move(e)); else return nullptr;
        n->add(std::move(item));
    } else if (s.text == "aggregate") {
        while (peek().kind == Kind::Id) {
            auto out = advance();
            auto item = mk(N::AggItem, out.text, out.line, out.col);
            expect("=", "after the output column name");
            if (peek().kind != Kind::Id) { error("expected an aggregate function"); return nullptr; }
            auto fn = advance();
            item->type = fn.text;
            expect("(", "after the aggregate function");
            if (peek().kind != Kind::Id) { error("expected a column name"); return nullptr; }
            auto arg = advance();
            item->add(mk(N::Ident, arg.text, arg.line, arg.col));
            expect(")", "after the aggregate argument");
            n->add(std::move(item));
            if (!accept(",")) break;
        }
    } else if (s.text == "sort") {
        if (peek().kind != Kind::Id) { error("expected a sort column"); return nullptr; }
        auto c = advance();
        auto key = mk(N::SortKey, c.text, c.line, c.col);
        key->type = accept("desc") ? "desc" : (accept("asc") ? "asc" : "asc");
        n->add(std::move(key));
    } else if (s.text == "limit") {
        if (peek().kind != Kind::Int) { error("expected an integer bound"); return nullptr; }
        auto v = advance();
        n->add(mk(N::IntLit, v.text, v.line, v.col));
    }

    expect(")", "to close the stage");
    return n;
}

NodePtr Parser::expr(int minBp) {
    auto lhs = unary();
    if (!lhs) return nullptr;

    while (peek().kind == Kind::Op) {
        int p = bp(peek().text);
        if (p == 0 || p < minBp) break;
        auto op = advance();
        auto rhs = expr(p + 1);              // +1 gives left associativity
        if (!rhs) return nullptr;
        auto node = mk(N::Binary, op.text, op.line, op.col);
        node->add(std::move(lhs));
        node->add(std::move(rhs));
        lhs = std::move(node);
    }
    return lhs;
}

NodePtr Parser::unary() {
    if (is("-") || is("!")) {
        auto op = advance();
        auto n = mk(N::Unary, op.text, op.line, op.col);
        if (auto k = unary()) n->add(std::move(k)); else return nullptr;
        return n;
    }
    return primary();
}

NodePtr Parser::primary() {
    const Token& t = peek();
    switch (t.kind) {
        case Kind::Int:   { auto x = advance(); return mk(N::IntLit,   x.text, x.line, x.col); }
        case Kind::Float: { auto x = advance(); return mk(N::FloatLit, x.text, x.line, x.col); }
        case Kind::Str:   { auto x = advance(); return mk(N::StrLit,   x.text, x.line, x.col); }
        case Kind::Id:    { auto x = advance(); return mk(N::Ident,    x.text, x.line, x.col); }
        default: break;
    }
    if (is("true") || is("false")) { auto x = advance(); return mk(N::BoolLit, x.text, x.line, x.col); }
    if (accept("(")) {
        auto e = expr();
        expect(")", "to close the parenthesised expression");
        return e;
    }
    error("expected an expression, found '" + t.text + "'");
    return nullptr;
}