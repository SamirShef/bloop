#include "diag/engine.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <algorithm>

namespace bloop {

DiagnosticBuilder::~DiagnosticBuilder() {
    _engine.Emit(_diag);
}

static llvm::raw_ostream::Colors
getSeverityColor(DiagLevel sev) {
    switch (sev) {
        case DiagLevel::Error:   return llvm::raw_ostream::RED;
        case DiagLevel::Warning: return llvm::raw_ostream::YELLOW;
        case DiagLevel::Note:    return llvm::raw_ostream::CYAN;
        case DiagLevel::Help:    return llvm::raw_ostream::GREEN;
    }
}

static std::string
getSeverityName(DiagLevel sev) {
    switch (sev) {
        case DiagLevel::Error:   return "error";
        case DiagLevel::Warning: return "warning";
        case DiagLevel::Note:    return "note";
        case DiagLevel::Help:    return "help";
    }
}

void
DiagnosticEngine::Emit(const Diagnostic &diag) {
    auto &os = llvm::errs();
    auto color = getSeverityColor(diag.Level);

    os.changeColor(color, true);
    os << getSeverityName(diag.Level);
    if (!diag.Code.empty()) {
        os << "[" << diag.Code << "]";
    }
    os << ": ";
    os.resetColor();
    os.changeColor(llvm::raw_ostream::WHITE, true);
    os << diag.Message << '\n';
    os.resetColor();

    for (const auto &span : diag.Spans) {
        unsigned bufId = _srcMgr.FindBufferContainingLoc(span.Start);
        auto lineCol = _srcMgr.getLineAndColumn(span.Start, bufId);
        auto buf = _srcMgr.getMemoryBuffer(bufId);
        
        os.changeColor(llvm::raw_ostream::CYAN);
        os << "  --> ";
        os.resetColor();
        os << _srcMgr.getBufferInfo(bufId).Buffer->getBufferIdentifier() 
           << ":" << lineCol.first << ":" << lineCol.second << '\n';

        const char *ptr = span.Start.getPointer();
        const char *bufStart = buf->getBufferStart();
        const char *bufEnd = buf->getBufferEnd();

        const char *lineStart = ptr;
        while (lineStart > bufStart && *(lineStart - 1) != '\n') {
            lineStart--;
        }
        const char *lineEnd = ptr;
        while (lineEnd < bufEnd && *lineEnd != '\n') {
            lineEnd++;
        }

        std::string lineText(lineStart, lineEnd - lineStart);
        
        std::string gutter = std::to_string(lineCol.first) + " | ";
        os.changeColor(llvm::raw_ostream::CYAN);
        os << gutter;
        os.resetColor();
        os << lineText << '\n';

        os.changeColor(llvm::raw_ostream::CYAN);
        for (int i = 0; i < gutter.size() - 3; ++i) {
            os << " ";
        }
        os << " | ";
        os.resetColor();

        os.changeColor(color, true);
        for (int i = 1; i < lineCol.second; ++i) {
            os << " ";
        }
        os << "^";
        
        const char *endPtr = span.End.getPointer();
        int len = std::max(0, (int)(endPtr - ptr) - 1);
        for (int i = 0; i < len; ++i) {
            os << "~";
        }
        
        if (!span.Label.empty()) {
            os << " " << span.Label;
        }
        os.resetColor();
        os << '\n';
    }

    for (const auto &note : diag.Notes) {
        os.changeColor(llvm::raw_ostream::CYAN, true);
        os << "  = ";
        os.resetColor();
        os.changeColor(llvm::raw_ostream::WHITE, true);
        os << "note: ";
        os.resetColor();
        os << note << '\n';
    }
    os << '\n';
}

}