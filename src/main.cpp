#include "lexer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

static void usage() {
    std::cout << "tblc - TabuLang compiler\n"
              << "usage: tblc <source.tbl> [--dump-tokens] [-O1]\n";
}

int main(int argc, char** argv) {
    std::string path;
    bool dumpTokens = false, opt = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help")             { usage(); return 0; }
        else if (a == "--dump-tokens") { dumpTokens = true; }
        else if (a == "-O1")           { opt = true; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option " << a << "\n";
            return 2;
        } else { path = a; }
    }
    (void)opt;

    if (path.empty()) { usage(); return 2; }

    std::ifstream in(path);
    if (!in) { std::cerr << "cannot open " << path << "\n"; return 2; }
    std::stringstream ss; ss << in.rdbuf();

    ErrorLog errors;
    auto toks = Lexer(ss.str(), errors).tokens();

    if (dumpTokens)
        for (const auto& t : toks)
            std::cout << kindName(t.kind) << "\t'" << t.text
                      << "'\t@" << t.line << ":" << t.col << "\n";

    if (errors.any()) { errors.report(path); return 1; }

    std::cout << "lexical analysis: " << toks.size() << " tokens, no errors\n";
    return 0;
}