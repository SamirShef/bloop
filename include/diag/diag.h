#pragma once
#include <diag/level.h>
#include <diag/span.h>
#include <string>
#include <vector>

namespace bloop {

struct Diagnostic {
    DiagLevel Level;
    int Code = -1;
    std::string Message;
    std::vector<DiagnosticSpan> Spans;
    std::vector<std::string> Notes;
    std::vector<std::string> Helps;

    explicit Diagnostic(DiagLevel level, std::string msg) : Level(level), Message(msg) {}
};

}