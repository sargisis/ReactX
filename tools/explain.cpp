// Short reports of build errors (see explain.hpp). Native only: it runs in
// the CLI, so <regex> and std::format are fine here.
#include "explain.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace explain {

namespace {

struct Diag {
    fs::path file;
    int line = 0;
    int column = 0;
    std::string message;  // as the compiler wrote it (quotes made plain)
};

// GCC quotes with ‘ ’ in a UTF-8 locale and ' ' otherwise; clang with ' '.
std::string plainQuotes(std::string s) {
    for (auto [from, to] : {std::pair<std::string_view, std::string_view>{"‘", "'"}, {"’", "'"}, {"`", "'"}}) {
        for (auto at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
    }
    return s;
}

void replaceAll(std::string& s, std::string_view from, std::string_view to) {
    for (auto at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
}

// Types as people write them: std::string, not std::__cxx11::basic_string<char, ...>.
std::string readableTypes(std::string s) {
    replaceAll(s, "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string");
    replaceAll(s, "std::__cxx11::basic_string<char>", "std::string");
    replaceAll(s, "std::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string");
    replaceAll(s, "std::__2::basic_string<char>", "std::string");
    replaceAll(s, "std::basic_string<char>", "std::string");
    replaceAll(s, "std::__cxx11::", "std::");
    replaceAll(s, "std::__2::", "std::");
    replaceAll(s, "std::__1::", "std::");
    static const std::regex allocator(R"(, std::allocator<[^<>]*(<[^<>]*>)?[^<>]*> ?>)");
    s = std::regex_replace(s, allocator, ">");
    if (s.size() > 220) s = s.substr(0, 217) + "...";
    return s;
}

bool inside(const fs::path& file, const fs::path& dir) {
    auto f = file.lexically_normal().string(), d = dir.lexically_normal().string();
    if (!d.empty() && d.back() != '/') d += '/';
    return f.starts_with(d);
}

// The project's own sources: under the root, not in build/ (fetched ReactX, generated files).
bool ownFile(const fs::path& file, const fs::path& root) {
    std::error_code ec;
    fs::path abs = file.is_absolute() ? file : fs::weakly_canonical(root / file, ec);
    return inside(abs, root) && !inside(abs, root / "build");
}

std::string relativeTo(const fs::path& file, const fs::path& root) {
    std::error_code ec;
    fs::path abs = file.is_absolute() ? file : root / file;
    auto rel = fs::relative(abs, root, ec);
    return ec || rel.empty() ? file.string() : rel.string();
}

std::string sourceLine(const fs::path& file, const fs::path& root, int line) {
    std::ifstream in(file.is_absolute() ? file : root / file);
    std::string text;
    for (int i = 1; std::getline(in, text); ++i)
        if (i == line) return text;
    return {};
}

std::string what(const std::string& type, bool ru) {
    static const std::regex literal(R"(const char ?\[\d+\])");
    if (std::regex_search(type, literal)) return ru ? "текст" : "text";
    if (type.find("std::string") != std::string::npos) return ru ? "текст (std::string)" : "text (std::string)";
    if (type.find("<lambda") != std::string::npos) return ru ? "лямбду" : "a lambda";
    return type;
}

// Which ReactX library a namespace belongs to: rx::query -> reactx::query.
std::optional<std::string> libraryOf(const std::string& text) {
    static const std::regex ns(R"(rx::(charts|i18n|query|markdown|icons|forms|log|motion|dnd|calendar|api|auth|config)::)");
    std::smatch m;
    if (std::regex_search(text, m, ns)) return "reactx::" + m[1].str();
    return std::nullopt;
}

using Explainer = std::function<std::optional<std::string>(const std::string& message, bool ru)>;

// Plain words for the mistakes people make most; nullopt: not this one.
const std::vector<Explainer>& rules() {
    static const std::vector<Explainer> list{
        // Designated initializers out of order: {.variant = ..., .label = ...}.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"(designator order for field '([\w:]+)::(\w+)' does not match declaration order in '([\w:]+)')");
            static const std::regex clang(R"(field designators to be specified in declaration order; field '(\w+)' will be initialized after field '(\w+)')");
            std::smatch s;
            if (std::regex_search(m, s, gcc))
                return ru ? std::format("Поля в {{.поле = …}} пишутся в том порядке, в каком объявлены в {}: .{} стоит не на своём месте — "
                                        "переставьте его (порядок полей — в заголовке или в llms.txt).",
                                        s[3].str(), s[2].str())
                          : std::format("Fields in {{.field = ...}} go in the order {} declares them: .{} is out of place — move it "
                                        "(the order is in the header or in llms.txt).",
                                        s[3].str(), s[2].str());
            if (std::regex_search(m, s, clang))
                return ru ? std::format("Поля в {{.поле = …}} пишутся в порядке объявления: .{} должно идти раньше .{}.", s[2].str(), s[1].str())
                          : std::format("Fields in {{.field = ...}} go in declaration order: .{} must come before .{}.", s[2].str(), s[1].str());
            return std::nullopt;
        },
        // A setter called with the wrong type: setCount("zero").
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"(no match for call to '\(.*rx::Setter<(.+?)>\}?\) \((.+)\)')");
            static const std::regex clang(R"(no matching function for call to object of type '.*rx::Setter<(.+?)>')");
            std::smatch s;
            if (std::regex_search(m, s, gcc))
                return ru ? std::format("Сеттер из useState<{0}> принимает {0} (или функцию {0} → {0}), а получил {1}.", s[1].str(), what(s[2].str(), true))
                          : std::format("A setter from useState<{0}> takes a {0} (or a function {0} -> {0}); it got {1}.", s[1].str(), what(s[2].str(), false));
            if (std::regex_search(m, s, clang))
                return ru ? std::format("Сеттер из useState<{0}> принимает {0} (или функцию {0} → {0}), а получил значение другого типа.", s[1].str())
                          : std::format("A setter from useState<{0}> takes a {0} (or a function {0} -> {0}); it got another type.", s[1].str());
            return std::nullopt;
        },
        // A props struct that can't be built: usually a handler with the wrong parameters.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex props(R"(could not convert '\{.*\}' from '<brace-enclosed initializer list>' to '([\w:]*?(\w+Props))')");
            static const std::regex lambda(R"(<lambda\(([^)]+)\)>)");
            std::smatch s, l;
            if (!std::regex_search(m, s, props)) return std::nullopt;
            if (std::regex_search(m, l, lambda))
                return ru ? std::format("В {} передан обработчик с параметрами ({}), а он вызывается без них (onClick, onToggle) или с другими "
                                        "(onChange получает значение). Уберите параметры: [..] {{ … }} — или посмотрите тип поля в llms.txt.",
                                        s[2].str(), l[1].str())
                          : std::format("A handler with parameters ({}) was given to {}, but it is called with none (onClick, onToggle) or "
                                        "with others (onChange gets the value). Drop the parameters: [..] {{ ... }} — or see the field's type in llms.txt.",
                                        l[1].str(), s[2].str());
            return ru ? std::format("Одно из полей {} получило значение не того типа: проверьте каждое {{.поле = …}} по llms.txt.", s[2].str())
                      : std::format("One of the fields of {} got a value of the wrong type: check each {{.field = ...}} in llms.txt.", s[2].str());
        },
        // A name that doesn't exist.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"('([\w:]+)' was not declared in this scope(?:; did you mean '([\w:]+)')?)");
            static const std::regex clang(R"(use of undeclared identifier '([\w:]+)'(?:; did you mean '([\w:]+)')?)");
            std::smatch s;
            if (!std::regex_search(m, s, gcc) && !std::regex_search(m, s, clang)) return std::nullopt;
            std::string hint = s[2].matched ? (ru ? std::format(" Может быть, {}?", s[2].str()) : std::format(" Did you mean {}?", s[2].str())) : "";
            return ru ? std::format("{} здесь неизвестно: опечатка, нет нужного #include или забыто using namespace rx;.{}", s[1].str(), hint)
                      : std::format("{} is unknown here: a typo, a missing #include, or a missing using namespace rx;.{}", s[1].str(), hint);
        },
        // Not in a namespace: rx::ui::datepicker, rx::query::useQueryy.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"('([\w:]+)' is not a member of '(rx(?:::\w+)?)')");
            static const std::regex clang(R"(no member named '(\w+)' in namespace '(rx(?:::\w+)?)')");
            std::smatch s;
            if (!std::regex_search(m, s, gcc) && !std::regex_search(m, s, clang)) return std::nullopt;
            return ru ? std::format("В {} нет {}: проверьте имя по llms.txt. Если оно новое, проект может быть на старом ReactX — reactx update; "
                                    "если это библиотека — нужен её #include \"rx/….hpp\".",
                                    s[2].str(), s[1].str())
                      : std::format("{} has no {}: check the name in llms.txt. If it is new, the project may be on an older ReactX — "
                                    "reactx update; if it is a library, it needs its #include \"rx/....hpp\".",
                                    s[2].str(), s[1].str());
        },
        // No such field / member: props.titel.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"('(?:const )?([\w:<>, ]+)' has no (?:non-static data )?member named '(\w+)'(?:; did you mean '(\w+)')?)");
            static const std::regex clang(R"(no member named '(\w+)' in '([\w:<>, ]+)')");
            static const std::regex designator(R"('([\w:]+)' has no non-static data member named '(\w+)')");
            std::smatch s;
            if (std::regex_search(m, s, gcc)) {
                std::string hint = s[3].matched ? (ru ? std::format(" Может быть, .{}?", s[3].str()) : std::format(" Did you mean .{}?", s[3].str())) : "";
                return ru ? std::format("У {} нет поля .{}.{}", s[1].str(), s[2].str(), hint) : std::format("{} has no member .{}.{}", s[1].str(), s[2].str(), hint);
            }
            if (std::regex_search(m, s, clang))
                return ru ? std::format("У {} нет поля .{}.", s[2].str(), s[1].str()) : std::format("{} has no member .{}.", s[2].str(), s[1].str());
            (void)designator;
            return std::nullopt;
        },
        // A header of a library that isn't linked.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex header(R"((rx|reactx)/(\w+)\.hpp'?:? (?:No such file or directory|file not found))");
            std::smatch s;
            if (!std::regex_search(m, s, header)) return std::nullopt;
            if (s[1] == "rx")
                return ru ? std::format("Библиотека не подключена: добавьте reactx::{0} в target_link_libraries в CMakeLists.txt "
                                        "(она идёт с ReactX 1.2+; на старой версии — reactx update).",
                                        s[2].str())
                          : std::format("The library isn't linked: add reactx::{0} to target_link_libraries in CMakeLists.txt "
                                        "(it comes with ReactX 1.2+; on an older one run reactx update).",
                                        s[2].str());
            return ru ? std::format("В ReactX нет заголовка reactx/{}.hpp: проверьте имя (llms.txt).", s[2].str())
                      : std::format("ReactX has no header reactx/{}.hpp: check the name (llms.txt).", s[2].str());
        },
        // Linker: a function of a library that isn't linked.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            if (m.find("undefined reference") == std::string::npos && m.find("undefined symbol") == std::string::npos) return std::nullopt;
            if (auto lib = libraryOf(m))
                return ru ? std::format("Компоновщик не нашёл код библиотеки: добавьте {} в target_link_libraries в CMakeLists.txt.", *lib)
                          : std::format("The linker can't find the library's code: add {} to target_link_libraries in CMakeLists.txt.", *lib);
            if (m.find("rx::net::") != std::string::npos)
                return ru ? std::string("rx::net (HttpServer) есть только в нативной сборке сервера: держите сервер в отдельной цели, не в web-приложении.")
                          : std::string("rx::net (HttpServer) exists only in a native server build: keep the server in its own target, not in the web app.");
            return ru ? std::string("Функция объявлена, но её код не найден: забыт .cpp в SOURCES, библиотека в target_link_libraries "
                                    "или тело функции.")
                      : std::string("A function is declared but its code isn't found: a .cpp missing from SOURCES, a library missing "
                                    "from target_link_libraries, or the function's body.");
        },
        // A value of one type where another is expected.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex gcc(R"(cannot convert '(.+?)' to '(.+?)')");
            static const std::regex clang(R"(no viable conversion from '(.+?)' to '(.+?)')");
            std::smatch s;
            if (!std::regex_search(m, s, gcc) && !std::regex_search(m, s, clang)) return std::nullopt;
            return ru ? std::format("Здесь нужен {}, а передан {}.", s[2].str(), what(s[1].str(), true))
                      : std::format("A {} is expected here, but {} was given.", s[2].str(), what(s[1].str(), false));
        },
        // A missing ; or }.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex expected(R"(expected '([;}\)\]])' (?:before|at end of))");
            std::smatch s;
            if (!std::regex_search(m, s, expected)) return std::nullopt;
            return ru ? std::format("Не хватает {} — обычно в конце предыдущей строки или выражения.", s[1].str())
                      : std::format("A {} is missing — usually at the end of the line or expression before.", s[1].str());
        },
        // The parser stumbles a token after the real mistake (a ; missing on the line before).
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            if (!m.starts_with("expected primary-expression") && !m.starts_with("expected unqualified-id") &&
                !m.starts_with("expected expression") && !m.starts_with("expected ',' or ';'"))
                return std::nullopt;
            return ru ? std::string("Синтаксис сломан чуть раньше этого места: чаще всего не хватает ; в конце предыдущей строки, "
                                    "или скобки ( { [ не закрыты.")
                      : std::string("The syntax broke just before this point: most often a ; is missing at the end of the line "
                                    "before, or a ( { [ isn't closed.");
        },
        // Copying something that can't be copied.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            if (m.find("use of deleted function") == std::string::npos && m.find("call to deleted") == std::string::npos) return std::nullopt;
            return ru ? std::string("Этот объект нельзя копировать (например, HttpServer, Sessions, unique_ptr): передайте его по ссылке (&) или std::move.")
                      : std::string("This object can't be copied (an HttpServer, Sessions, a unique_ptr): pass it by reference (&) or std::move it.");
        },
        // A function called with arguments it doesn't take.
        [](const std::string& m, bool ru) -> std::optional<std::string> {
            static const std::regex call(R"(no matching (?:function|constructor) for (?:call to|initialization of) '([\w:]+))");
            std::smatch s;
            if (!std::regex_search(m, s, call)) return std::nullopt;
            return ru ? std::format("{} не принимает такие аргументы: проверьте их число и типы (llms.txt).", s[1].str())
                      : std::format("{} doesn't take these arguments: check how many and their types (llms.txt).", s[1].str());
        },
    };
    return list;
}

// One element call h(...) failing is almost always an earlier error inside it.
bool cascade(const std::string& message) {
    return message.starts_with("no matching function for call to 'h(") || message.starts_with("no matching function for call to 'h'");
}

struct Parsed {
    std::vector<Diag> errors;      // in the project's files, first ones first
    std::vector<std::string> linker;  // undefined references
    std::vector<std::string> cmake;   // CMake Error blocks
    std::size_t lines = 0;
};

Parsed parse(std::string_view output, const fs::path& root) {
    static const std::regex diag(R"(^(.+?):(\d+):(?:(\d+):)? (?:fatal )?error: (.*)$)");
    static const std::regex requiredHere(R"(^(.+?):(\d+):(\d+):\s+required from here)");
    Parsed p;
    std::optional<Diag> pendingDep;  // an error in a header; the "required from here" note names the user's line
    std::string cmakeBlock;
    bool inCmake = false;
    std::size_t start = 0;
    while (start <= output.size()) {
        auto end = output.find('\n', start);
        if (end == std::string_view::npos) end = output.size();
        std::string line = plainQuotes(std::string(output.substr(start, end - start)));
        start = end + 1;
        ++p.lines;
        if (inCmake) {
            if (line.empty() || line.starts_with("-- ") || line.starts_with("CMake ")) {
                p.cmake.push_back(cmakeBlock);
                inCmake = false;
            } else {
                cmakeBlock += "\n" + line;
                continue;
            }
        }
        if (line.starts_with("CMake Error")) {
            inCmake = true;
            cmakeBlock = line;
            continue;
        }
        if (line.find("undefined reference to") != std::string::npos || line.find("undefined symbol:") != std::string::npos) {
            p.linker.push_back(readableTypes(line.substr(line.find("undefined"))));
            continue;
        }
        std::smatch m;
        if (std::regex_search(line, m, diag)) {
            Diag d{m[1].str(), std::stoi(m[2].str()), m[3].matched ? std::stoi(m[3].str()) : 0, m[4].str()};
            if (ownFile(d.file, root)) {
                p.errors.push_back(std::move(d));
                pendingDep.reset();
            } else if (!pendingDep) {
                pendingDep = std::move(d);
            }
            continue;
        }
        if (pendingDep && std::regex_search(line, m, requiredHere) && ownFile(m[1].str(), root)) {
            Diag d = *pendingDep;
            d.file = m[1].str();
            d.line = std::stoi(m[2].str());
            d.column = std::stoi(m[3].str());
            p.errors.push_back(std::move(d));
            pendingDep.reset();
        }
    }
    if (inCmake) p.cmake.push_back(cmakeBlock);
    return p;
}

std::string explanation(const std::string& message, bool ru) {
    for (const auto& rule : rules())
        if (auto text = rule(message, ru)) return *text;
    return {};
}

}  // namespace

bool russianLocale() {
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"})
        if (const char* v = std::getenv(name); v && *v) return std::string_view(v).starts_with("ru");
    return false;
}

std::string report(std::string_view output, const Options& o) {
    const bool ru = o.russian;
    Parsed p = parse(output, o.root);
    std::string out;

    // Errors in the code: one per line of code, cascades of an earlier error dropped.
    std::vector<Diag> shown;
    std::set<std::string> seen;
    std::size_t hidden = 0;
    const bool others = std::ranges::any_of(p.errors, [](const Diag& d) { return !cascade(d.message); });
    for (const auto& d : p.errors) {
        std::string where = d.file.string() + ":" + std::to_string(d.line);
        if (others && cascade(d.message)) continue;
        if (!seen.insert(where).second) continue;
        if (shown.size() >= o.maxErrors) {
            ++hidden;
            continue;
        }
        shown.push_back(d);
    }
    for (const auto& d : shown) {
        std::string code = sourceLine(d.file, o.root, d.line);
        std::string text = explanation(d.message, ru);
        out += std::format("\n  {}:{}\n", relativeTo(d.file, o.root), d.line);
        if (!code.empty()) {
            out += std::format("  {:>5} | {}\n", d.line, code);
            if (d.column > 0) out += std::format("        | {}^\n", std::string(static_cast<std::size_t>(d.column - 1), ' '));
        }
        if (!text.empty()) out += "  → " + text + "\n";
        else out += "  → " + readableTypes(d.message) + "\n";
    }
    if (hidden > 0)
        out += ru ? std::format("\n  …и ещё {} (часто это следствие первых — исправьте их и соберите снова).\n", hidden)
                  : std::format("\n  ...and {} more (often caused by the first ones — fix those and build again).\n", hidden);

    // Linker errors (no line of code to point at).
    std::set<std::string> said;
    for (const auto& l : p.linker) {
        std::string text = explanation(l, ru);
        if (!said.insert(text.empty() ? l : text).second) continue;
        out += "\n  " + l + "\n";
        if (!text.empty()) out += "  → " + text + "\n";
        if (said.size() >= 3) break;
    }

    // CMake: its own message is already short and in words.
    for (const auto& c : p.cmake) {
        out += "\n" + c + "\n";
        if (auto lib = libraryOf(c); lib && c.find("not found") != std::string::npos)
            out += ru ? "  → Цели " + *lib + " нет: она есть в ReactX 1.2+ (reactx update), а серверные (api, auth, config) — только в нативной сборке.\n"
                      : "  → There is no target " + *lib + ": it comes with ReactX 1.2+ (reactx update); server ones (api, auth, config) only in native builds.\n";
    }

    if (out.empty()) return {};
    // Russian plural forms: 1 ошибка, 3 ошибки, 5 ошибок.
    auto ruForm = [](std::size_t n, const char* one, const char* few, const char* many) {
        const std::size_t d = n % 10, h = n % 100;
        return d == 1 && h != 11 ? one : (d >= 2 && d <= 4 && (h < 12 || h > 14)) ? few : many;
    };
    const std::size_t count = shown.size() + hidden + (p.linker.empty() ? 0 : 1) + p.cmake.size();
    std::string head = ru ? std::format("✗ Сборка не прошла: {} {} (компилятор напечатал {} {} — они в логе).\n", count,
                                        ruForm(count, "ошибка", "ошибки", "ошибок"), p.lines, ruForm(p.lines, "строку", "строки", "строк"))
                          : std::format("✗ The build failed: {} error{} (the compiler printed {} lines — they are in the log).\n", count,
                                        count == 1 ? "" : "s", p.lines);
    return head + out;
}

}  // namespace explain
