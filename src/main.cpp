#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "tacgen.hpp"
#include "blocks.hpp"
#include "dag.hpp"
#include "codegen.hpp"
#include "vm.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

static void usage() {
    std::cout << "tblc - TabuLang compiler\n"
              << "usage: tblc <source.tbl> [--dump-tokens] [--dump-ast] "
                 "[--dump-symbols] [--dump-schemas] [--dump-tac] [--dump-cfg] "
                 "[--dump-dag] [--dump-bytecode] [--run] [-O1]\n";
}

static Stream optimizeStream(const Stream& s) {
    Cfg cfg(s);
    Stream out{s.name, {}};
    for (const auto& b : cfg.blocks()) {
        Dag d(s, b.first, b.last);
        for (const auto& q : d.result()) out.code.push_back(q);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string path;
    bool dumpTokens = false, dumpTree = false, dumpSyms = false,
         dumpSchemas = false, dumpTac = false, dumpCfg = false,
         dumpDag = false, dumpBc = false, run = false, opt = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help")                { usage(); return 0; }
        else if (a == "--dump-tokens")    { dumpTokens = true; }
        else if (a == "--dump-ast")       { dumpTree = true; }
        else if (a == "--dump-symbols")   { dumpSyms = true; }
        else if (a == "--dump-schemas")   { dumpSchemas = true; }
        else if (a == "--dump-tac")       { dumpTac = true; }
        else if (a == "--dump-cfg")       { dumpCfg = true; }
        else if (a == "--dump-dag")       { dumpDag = true; }
        else if (a == "--dump-bytecode")  { dumpBc = true; }
        else if (a == "--run")            { run = true; }
        else if (a == "-O1")              { opt = true; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option " << a << "\n";
            return 2;
        } else { path = a; }
    }

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

    int before = tac.quadCount();

    if (dumpDag) {
        Cfg cfg(tac.main());
        for (const auto& b : cfg.blocks()) {
            Dag d(tac.main(), b.first, b.last, true);
            d.dump(b.id);
        }
    }

    Stream finalMain = tac.main();
    std::vector<Stream> finalFrags(tac.fragments().begin(), tac.fragments().end());

    if (opt) {
        finalMain = optimizeStream(tac.main());
        finalFrags.clear();
        for (const auto& f : tac.fragments()) finalFrags.push_back(optimizeStream(f));

        int after = static_cast<int>(finalMain.code.size());
        for (const auto& f : finalFrags) after += static_cast<int>(f.code.size());

        std::cout << "optimization: " << before << " quadruples before, "
                  << after << " after, " << (before - after) << " removed ("
                  << (before ? (before - after) * 100 / before : 0) << "%)\n";
    }

    if (dumpTac) {
        dumpStream(finalMain);
        for (const auto& f : finalFrags) dumpStream(f);
        if (!opt) std::cout << "total: " << tac.quadCount() << " quadruples, "
                            << tac.tempCount() << " temporaries\n";
    }

    if (dumpCfg) {
        Cfg(finalMain).dump();
        for (const auto& f : finalFrags) Cfg(f).dump();
    }

    if (dumpBc || run) {
        CodeGen cg;
        auto code = cg.generate(finalMain);

        if (dumpBc) dumpCode(code);

        if (run) {
            std::cout << "--- execution\n";
            Vm vm;
            long long n = vm.run(code);
            std::cout << "--- " << n << " instructions executed\n";
        }
    }

    std::cout << "front end: " << toks.size() << " tokens, "
              << ast->kids.size() << " top-level statements, no errors\n";
    return 0;
}