#pragma once
#include <diag/level.h>
#include <diag/span.h>
#include <string>
#include <vector>

namespace bloop {

struct Diagnostic {
    DiagLevel Level;
    std::string Code;
    std::string Message;
    std::vector<DiagnosticSpan> Spans;
    std::vector<std::string> Notes;

    explicit Diagnostic(DiagLevel level, std::string msg) : Level(level), Message(msg) {}
};

}