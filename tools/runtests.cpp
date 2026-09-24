// runtests: TabuLang regression runner.
// usage (from the project root):  build/runtests            run every case
//                                 build/runtests --update   rewrite expected files
//
// tests/cases.txt holds one case per line:
//   run | TC-17 | description | tests/programs/demo.tbl --run -O2 | 0
//   eq  | TC-32 | description | tests/programs/demo.tbl
// "run" compares stdout, stderr and the exit code with tests/expected/<id>.out/.err.
// "eq" runs the program at -O0 and at -O2 and checks the printed results are
// identical, which is the claim that optimization preserves meaning.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
static const char* TBLC = "build\\tblc.exe";
#else
static const char* TBLC = "build/tblc";
#endif

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss; ss << in.rdbuf();
    return ss.str();
}

// Removes what legitimately differs between machines: carriage returns,
// path separators, and the wall-clock timing line.
static std::string normalise(const std::string& raw, bool resultsOnly) {
    std::stringstream in(raw), out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        for (auto& c : line) if (c == '\\') c = '/';
        if (line.rfind("--- execution time", 0) == 0) continue;
        if (resultsOnly && (line.rfind("---", 0) == 0 || line.rfind("front end", 0) == 0 ||
                            line.rfind("optimization:", 0) == 0)) continue;
        out << line << "\n";
    }
    return out.str();
}

struct Result { std::string out, err; int code; };

static Result runTblc(const std::string& args) {
    std::string cmd = std::string(TBLC) + " " + args + " > tc_out.txt 2> tc_err.txt";
    int status = std::system(cmd.c_str());
#ifdef _WIN32
    int code = status;
#else
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    Result r{readFile("tc_out.txt"), readFile("tc_err.txt"), code};
    std::remove("tc_out.txt");
    std::remove("tc_err.txt");
    return r;
}

int main(int argc, char** argv) {
    bool update = argc > 1 && std::string(argv[1]) == "--update";
    std::ifstream cases("tests/cases.txt");
    if (!cases) { std::cerr << "run from the project root: tests/cases.txt not found\n"; return 2; }

    int pass = 0, fail = 0;
    std::string line;
    while (std::getline(cases, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) f.push_back(trim(part));
        if (f.size() < 4) continue;
        const std::string& kind = f[0], id = f[1], desc = f[2], args = f[3];

        bool ok;
        std::string why;
        if (kind == "eq") {
            Result a = runTblc(args + " --run");
            Result b = runTblc(args + " --run -O2");
            ok = normalise(a.out, true) == normalise(b.out, true) && a.code == b.code;
            if (!ok) why = "results differ between -O0 and -O2";
        } else {
            int want = f.size() > 4 ? std::atoi(f[4].c_str()) : 0;
            Result r = runTblc(args);
            std::string outPath = "tests/expected/" + id + ".out";
            std::string errPath = "tests/expected/" + id + ".err";
            if (update) {
                std::ofstream(outPath, std::ios::binary) << normalise(r.out, false);
                std::ofstream(errPath, std::ios::binary) << normalise(r.err, false);
            }
            bool outOk = normalise(r.out, false) == normalise(readFile(outPath), false);
            bool errOk = normalise(r.err, false) == normalise(readFile(errPath), false);
            bool codeOk = r.code == want;
            ok = outOk && errOk && codeOk;
            if (!outOk) why = "stdout differs from " + outPath;
            else if (!errOk) why = "stderr differs from " + errPath;
            else if (!codeOk) why = "exit code " + std::to_string(r.code) +
                                    ", expected " + std::to_string(want);
        }

        std::cout << (ok ? "PASS  " : "FAIL  ") << id << "  " << desc << "\n";
        if (!ok) std::cout << "        " << why << "\n";
        (ok ? pass : fail)++;
    }
    std::cout << "\n" << pass << " passed, " << fail << " failed, "
              << pass + fail << " total\n";
    return fail == 0 ? 0 : 1;
}
