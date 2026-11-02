// The public API, frozen: compares include/reactx/*.hpp with api/reactx.api.
//
//   reactx_api_check <source dir>            fails if the API changed
//   reactx_api_check <source dir> --update   writes the new api/reactx.api
//
// The snapshot is every public header without comments, preprocessor lines
// (except #define), `detail` namespaces and private sections, one
// declaration per line. A change there is a change of the public API: an
// addition is fine (update the snapshot, note it in the CHANGELOG), a rename
// or a removal isn't before 2.0 (see "API stability" in the README).
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Without comments; string and character literals are kept as they are.
std::string withoutComments(const std::string& src) {
    std::string out;
    for (std::size_t i = 0; i < src.size(); ++i) {
        char c = src[i];
        if (c == '"' || c == '\'') {
            // A raw string R"x(...)x" ends with )x".
            if (c == '"' && i > 0 && src[i - 1] == 'R') {
                auto open = src.find('(', i);
                std::string end = ")" + src.substr(i + 1, open - i - 1) + "\"";
                auto close = src.find(end, open);
                close = close == std::string::npos ? src.size() : close + end.size();
                out.append(src, i, close - i);
                i = close - 1;
                continue;
            }
            out += c;
            for (++i; i < src.size() && src[i] != c; ++i) {
                out += src[i];
                if (src[i] == '\\' && i + 1 < src.size()) out += src[++i];
            }
            if (i < src.size()) out += src[i];
        } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            out += '\n';
        } else if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            auto end = src.find("*/", i + 2);
            i = end == std::string::npos ? src.size() : end + 1;
            out += ' ';
        } else {
            out += c;
        }
    }
    return out;
}

std::string trim(std::string s) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    std::string out;  // runs of spaces -> one
    for (char ch : s)
        if (!(std::isspace(static_cast<unsigned char>(ch)) && !out.empty() && out.back() == ' '))
            out += std::isspace(static_cast<unsigned char>(ch)) ? ' ' : ch;
    return out;
}

// Braces outside string and character literals.
int braceDelta(const std::string& line) {
    int delta = 0;
    char quote = 0;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quote) {
            if (c == '\\') ++i;
            else if (c == quote) quote = 0;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '{') {
            ++delta;
        } else if (c == '}') {
            --delta;
        }
    }
    return delta;
}

std::string publicPart(const std::string& header) {
    std::istringstream in(withoutComments(header));
    std::string out, raw;
    int depth = 0;
    int skipUntil = -1;      // inside a detail namespace: skip while depth > this
    int privateDepth = -1;   // inside a private section of a class at this depth
    while (std::getline(in, raw)) {
        std::string line = trim(raw);
        int before = depth;
        depth += braceDelta(line);
        if (line.empty()) continue;
        if (skipUntil >= 0) {
            if (depth <= skipUntil) skipUntil = -1;
            continue;
        }
        if (line.starts_with("namespace detail") || line.contains("::detail {")) {
            if (depth > before) skipUntil = before;
            continue;
        }
        if (privateDepth >= 0) {
            if (depth < privateDepth) privateDepth = -1;  // the class ended
            else if (before == privateDepth && (line == "public:" || line == "protected:")) privateDepth = -1;
            else continue;
        }
        if (line == "private:") {
            privateDepth = before;
            continue;
        }
        if (line.starts_with('#') && !line.starts_with("#define")) continue;
        out += line;
        out += '\n';
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: reactx_api_check <source dir> [--update]\n";
        return 2;
    }
    fs::path root = argv[1];
    bool update = argc > 2 && std::string(argv[2]) == "--update";

    std::vector<fs::path> headers;
    for (const auto& entry : fs::directory_iterator(root / "include/reactx"))
        if (entry.path().extension() == ".hpp") headers.push_back(entry.path());
    std::ranges::sort(headers);

    std::string api;
    for (const auto& h : headers) api += "== " + h.filename().string() + "\n" + publicPart(readFile(h));

    fs::path snapshot = root / "api/reactx.api";
    if (update) {
        fs::create_directories(snapshot.parent_path());
        std::ofstream(snapshot, std::ios::binary) << api;
        std::cout << "wrote " << snapshot.string() << '\n';
        return 0;
    }
    std::string old = readFile(snapshot);
    if (old == api) {
        std::cout << "OK, the public API matches api/reactx.api\n";
        return 0;
    }
    // The first lines that differ, to see what changed.
    std::istringstream a(old), b(api);
    std::string la, lb, file;
    int shown = 0;
    for (int n = 1; shown < 10; ++n) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(b, lb));
        if (!ga && !gb) break;
        if (gb && lb.starts_with("== ")) file = lb;
        if (!ga) la = "<end>";
        if (!gb) lb = "<end>";
        if (la != lb) {
            std::cerr << file << " line " << n << ":\n  was: " << la << "\n  now: " << lb << '\n';
            ++shown;
            break;  // the rest is shifted; one is enough to point at it
        }
    }
    std::cerr << "The public API changed (include/reactx/ vs api/reactx.api).\n"
                 "An addition: run `reactx_api_check " << root.string() << " --update`, commit api/reactx.api and\n"
                 "note it in both CHANGELOGs. A rename or a removal breaks apps: add the new name instead\n"
                 "and deprecate the old one (AGENTS.md, README \"API stability\").\n";
    return 1;
}
