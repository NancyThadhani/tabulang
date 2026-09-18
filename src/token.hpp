#pragma once
#include <string>
#include <vector>
#include <unordered_set>

enum class Kind { Id, Int, Float, Str, Keyword, Op, Eof };

const char* kindName(Kind k);

struct Token {
    Kind kind;
    std::string text;
    int line;
    int col;
};

extern const std::unordered_set<std::string> KEYWORDS;
extern const std::vector<std::string> OPERATORS;   // longest first