#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "tacgen.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

static void usage() {
    std::cout << "tblc - TabuLang compiler\n"
              << "usage: tblc <source.tbl> [--dump-tokens] [--dump-ast] "
                 "[--dump-symbols] [--dump-schemas] [--dump-tac] [-O1]\n";
}

int main(int argc, char** argv) {
    std::string path;
    bool dumpTokens = false, dumpTree = false, dumpSyms = false,
         dumpSchemas = false, dumpTac = false, opt = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help")              { usage(); return 0; }
        else if (a == "--dump-tokens")  { dumpTokens = true; }
        else if (a == "--dump-ast")     { dumpTree = true; }
        else if (a == "--dump-symbols") { dumpSyms = true; }
        else if (a == "--dump-schemas") { dumpSchemas = true; }
        else if (a == "--dump-tac")     { dumpTac = true; }
        else if (a == "-O1")            { opt = true; }
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

    auto ast = Parser(toks, errors).parseProgram();

    if (dumpTree) dumpAst(ast.get());

    Sema sema(errors);
    sema.analyze(ast.get());

    if (dumpSyms) sema.table().dump();

    if (dumpSchemas) {
        for (const auto& kid : ast->kids) {
            if (kid->kind != N::TableDecl) continue;
            Symbol* s = sema.table().lookup(kid->text);
            if (!s) continue;
            std::cout << s->name << " : {";
            for (size_t i = 0; i < s->schema.size(); ++i)
                std::cout << (i ? ", " : "") << s->schema[i].name
                          << ":" << s->schema[i].type;
            std::cout << "}\n";
        }
    }

    if (errors.any()) { errors.report(path); return 1; }

    TacGen tac;
    tac.generate(ast.get());

    if (dumpTac) {
        dumpStream(tac.main());
        for (const auto& f : tac.fragments()) dumpStream(f);
        std::cout << "total: " << tac.quadCount() << " quadruples, "
                  << tac.tempCount() << " temporaries\n";
    }

    std::cout << "front end: " << toks.size() << " tokens, "
              << ast->kids.size() << " top-level statements, no errors\n";
    return 0;
}