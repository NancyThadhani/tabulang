#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "tacgen.hpp"
#include "blocks.hpp"
#include "dag.hpp"
#include "codegen.hpp"
#include "vm.hpp"
#include "pipeopt.hpp"
#include "dot.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <map>

static void usage() {
    std::cout << "tblc - TabuLang compiler\n"
              << "usage: tblc <source.tbl> [--dump-tokens] [--dump-ast] "
                 "[--dump-symbols] [--dump-schemas] [--dump-tac] [--dump-cfg] "
                 "[--dump-dag] [--dump-pipeline] [--dump-bytecode] [--dot-ast] [--dot-cfg] [--run] [-O1] [-O2]\n";
}

// Optimizes each basic block through its DAG, then renumbers jump targets.
// Removing quadruples shifts every later index, and every jump target is a
// block leader, so a map from old leader index to new index is enough.
static Stream optimizeStream(const Stream& s) {
    Cfg cfg(s);
    Stream out{s.name, {}};
    std::map<int, int> newIndex;
    for (const auto& b : cfg.blocks()) {
        newIndex[b.first] = static_cast<int>(out.code.size());
        Dag d(s, b.first, b.last);
        for (const auto& q : d.result()) out.code.push_back(q);
    }
    newIndex[static_cast<int>(s.code.size())] = static_cast<int>(out.code.size());

    for (auto& q : out.code) {
        bool isJump = q.op == "goto" || (q.op.size() > 2 && q.op.compare(0, 2, "if") == 0);
        if (!isJump || q.res.empty()) continue;
        auto it = newIndex.find(std::stoi(q.res));
        if (it != newIndex.end()) q.res = std::to_string(it->second);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string path;
    bool dumpTokens = false, dumpTree = false, dumpSyms = false,
         dumpSchemas = false, dumpTac = false, dumpCfg = false,
         dumpDag = false, dumpBc = false, run = false, opt = false,
         opt2 = false, dumpPipe = false,
         dotTree = false, dotFlow = false;

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
        else if (a == "-O2")              { opt = true; opt2 = true; }
        else if (a == "--dump-pipeline")  { dumpPipe = true; }
        else if (a == "--dot-ast")        { dotTree = true; }
        else if (a == "--dot-cfg")        { dotFlow = true; }
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

    if (opt2) {
        // pipeline passes run first, on the unoptimized stage quadruples
        Stream piped = tac.main();
        PipeReport pr = optimizePipelines(piped, tac.fragments());
        if (dumpPipe) {
            std::cout << "--- pipeline optimization\n";
            for (const auto& l : pr.log) std::cout << "  " << l << "\n";
            std::cout << "  " << pr.pushed << " filter(s) pushed down, " << pr.pruned
                      << " column(s) pruned, " << pr.fused << " stage(s) fused\n";
        }
        finalMain = optimizeStream(piped);
        finalFrags.clear();
        for (const auto& f : tac.fragments()) finalFrags.push_back(optimizeStream(f));

        int after = static_cast<int>(finalMain.code.size());
        for (const auto& f : finalFrags) after += static_cast<int>(f.code.size());
        std::cout << "optimization: " << before << " quadruples before, "
                  << after << " after, " << (before - after) << " removed ("
                  << (before ? (before - after) * 100 / before : 0) << "%)\n";
    } else if (opt) {
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

    if (dotTree) { dotAst(ast.get()); return 0; }
    if (dotFlow) {
        std::vector<Stream> all{finalMain};
        all.insert(all.end(), finalFrags.begin(), finalFrags.end());
        dotCfg(all);
        return 0;
    }

    if (dumpCfg) {
        Cfg(finalMain).dump();
        for (const auto& f : finalFrags) Cfg(f).dump();
    }

    if (dumpBc || run) {
        CodeGen cg;
        auto code = cg.generate(finalMain);

        // every per-row fragment is compiled to its own bytecode
        std::map<std::string, std::vector<Instr>> fragCode;
        bool cgFailed = cg.failed();
        for (const auto& f : finalFrags) {
            CodeGen fcg;
            fragCode[f.name] = fcg.generate(f);
            cgFailed = cgFailed || fcg.failed();
        }
        if (cgFailed) return 4;

        if (dumpBc) {
            dumpCode(code);
            for (const auto& f : fragCode) {
                std::cout << "--- fragment " << f.first << "\n";
                dumpCode(f.second);
            }
        }

        if (run) {
            std::string baseDir;
            auto slash = path.find_last_of("/\\");
            if (slash != std::string::npos) baseDir = path.substr(0, slash);

            std::cout << "--- execution\n";
            Vm vm(fragCode, baseDir);
            auto t0 = std::chrono::steady_clock::now();
            bool ok = vm.run(code);
            auto t1 = std::chrono::steady_clock::now();
            const RunStats& st = vm.stats();
            std::cout << "--- " << st.instructions << " instructions executed, "
                      << st.rowsProcessed << " rows and " << st.cellsProcessed
                      << " cells processed by table stages\n"
                      << "--- execution time: "
                      << std::chrono::duration<double, std::milli>(t1 - t0).count()
                      << " ms\n";
            if (!ok) return 3;
        }
    }

    std::cout << "front end: " << toks.size() << " tokens, "
              << ast->kids.size() << " top-level statements, no errors\n";
    return 0;
}