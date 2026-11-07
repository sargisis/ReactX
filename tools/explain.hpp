// Short, human-language reports of build errors for the reactx CLI.
//
// A C++ mistake can make the compiler print hundreds of lines (templates,
// notes, the same error again in every caller). report() keeps the errors in
// the project's own files, drops what follows from an earlier one, shows the
// line of code, and says in plain words what usually causes it and what to
// do. The full output still goes to a log file and `--verbose`.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace explain {

struct Options {
    std::filesystem::path root;  // the project: errors in its files are the ones shown
    bool russian = false;        // explanations in Russian
    std::size_t maxErrors = 5;
};

// The report, or "" when the output has no error report() understands (then
// show the output's tail instead).
std::string report(std::string_view output, const Options& options);

// Russian when LC_ALL / LC_MESSAGES / LANG start with "ru".
bool russianLocale();

}  // namespace explain
