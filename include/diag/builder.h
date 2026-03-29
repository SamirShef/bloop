#pragma once
#include <diag/diag.h>
#include <string>

namespace bloop {

class DiagnosticEngine;

class DiagnosticBuilder {
    DiagnosticEngine &_engine;
    Diagnostic _diag;

public:
    DiagnosticBuilder(DiagnosticEngine &eng, DiagLevel sev, std::string msg) : _engine(eng), _diag(sev, msg) {}

    DiagnosticBuilder(const DiagnosticBuilder &) = delete;
    DiagnosticBuilder(DiagnosticBuilder &&) = default;
    ~DiagnosticBuilder();

    DiagnosticBuilder &
    AddSpan(llvm::SMLoc start, llvm::SMLoc end, std::string label = "") {
        _diag.Spans.push_back(DiagnosticSpan(start, end, label));
        return *this;
    }

    DiagnosticBuilder &
    SetCode(int c) {
        _diag.Code = c;
        return *this;
    }

    DiagnosticBuilder &
    AddNote(std::string n) {
        _diag.Notes.push_back(n);
        return *this;
    }
};

}