#include "lexer.hpp"
#include <cctype>

char Lexer::peek(size_t k) const {
    size_t j = i_ + k;
    return j < src_.size() ? src_[j] : '\0';
}

char Lexer::advance() {
    char c = src_[i_++];
    if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
    return c;
}

bool Lexer::matchOperator(std::string& out) {
    for (const auto& op : OPERATORS) {
        if (src_.compare(i_, op.size(), op) == 0) {
            for (size_t k = 0; k < op.size(); ++k) advance();
            out = op;
            return true;
        }
    }
    return false;
}

std::vector<Token> Lexer::tokens() {
    std::vector<Token> out;
    State state = State::Start;
    std::string buf;
    int sl = 1, sc = 1;                 // start position of the current lexeme

    auto isIdStart = [](char c) { return std::isalpha((unsigned char)c) || c == '_'; };
    auto isIdPart  = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
    auto isDigit   = [](char c) { return std::isdigit((unsigned char)c) != 0; };

    while (i_ < src_.size()) {
        char c = peek();

        switch (state) {
        case State::Start:
            sl = line_; sc = col_;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '#') {
                advance(); state = State::InComment;
            } else if (isIdStart(c)) {
                buf = std::string(1, advance()); state = State::InId;
            } else if (isDigit(c)) {
                buf = std::string(1, advance()); state = State::InInt;
            } else if (c == '"') {
                advance(); buf.clear(); state = State::InStr;
            } else {
                std::string op;
                if (matchOperator(op)) {
                    out.push_back({Kind::Op, op, sl, sc});
                } else {
                    errors_.add("lexical", sl, sc,
                                std::string("illegal character '") + c + "'");
                    advance();                      // recovery: skip one char
                }
            }
            break;

        case State::InId:
            if (isIdPart(c)) {
                buf += advance();
            } else {
                Kind k = KEYWORDS.count(buf) ? Kind::Keyword : Kind::Id;
                out.push_back({k, buf, sl, sc});
                state = State::Start;
            }
            break;

        case State::InInt:
            if (isDigit(c)) {
                buf += advance();
            } else if (c == '.' && isDigit(peek(1))) {
                buf += advance(); state = State::InFrac;
            } else {
                out.push_back({Kind::Int, buf, sl, sc});
                state = State::Start;
            }
            break;

        case State::InFrac:
            if (isDigit(c)) {
                buf += advance();
            } else {
                out.push_back({Kind::Float, buf, sl, sc});
                state = State::Start;
            }
            break;

        case State::InStr:
            if (c == '"') {
                advance();
                out.push_back({Kind::Str, buf, sl, sc});
                state = State::Start;
            } else if (c == '\n') {
                errors_.add("lexical", sl, sc, "unterminated string");
                state = State::Start;
            } else {
                buf += advance();
            }
            break;

        case State::InComment:
            if (c == '\n') state = State::Start;
            advance();
            break;
        }
    }

    // flush whatever the automaton was in the middle of
    if (state == State::InId) {
        Kind k = KEYWORDS.count(buf) ? Kind::Keyword : Kind::Id;
        out.push_back({k, buf, sl, sc});
    } else if (state == State::InInt) {
        out.push_back({Kind::Int, buf, sl, sc});
    } else if (state == State::InFrac) {
        out.push_back({Kind::Float, buf, sl, sc});
    } else if (state == State::InStr) {
        errors_.add("lexical", sl, sc, "unterminated string at end of file");
    }

    out.push_back({Kind::Eof, "", line_, col_});
    return out;
}