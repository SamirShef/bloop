#include <diag/engine.h>
#include <iomanip>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Format.h>
#include <algorithm>
#include <map>
#include <sstream>

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

static std::string
getSeverityShortName(DiagLevel sev) {
    switch (sev) {
        case DiagLevel::Error:   return "E";
        case DiagLevel::Warning: return "W";
        case DiagLevel::Note:    return "N";
        case DiagLevel::Help:    return "H";
    }
}

void
DiagnosticEngine::Emit(const Diagnostic &diag) {
    auto &os = llvm::errs();
    auto color = getSeverityColor(diag.Level);

    os.changeColor(color, true);
    os << getSeverityName(diag.Level);
    if (diag.Code != -1) {
        os << "[" << getSeverityShortName(diag.Level) << llvm::format("%04d", diag.Code) << "]";
    }
    os << ": ";
    os.resetColor();
    os.changeColor(llvm::raw_ostream::WHITE, true);
    os << diag.Message << '\n';
    os.resetColor();

    std::map<unsigned, std::vector<const DiagnosticSpan *>> lineGroups;
    for (const auto &span : diag.Spans) {
        unsigned bufId = _srcMgr.FindBufferContainingLoc(span.Start);
        unsigned lineNo = _srcMgr.getLineAndColumn(span.Start, bufId).first;
        lineGroups[lineNo].push_back(&span);
    }

    for (auto &group : lineGroups) {
        unsigned lineNo = group.first;
        auto &spansOnLine = group.second;

        std::sort(spansOnLine.begin(), spansOnLine.end(), [](const DiagnosticSpan *a, const DiagnosticSpan *b) {
            return a->Start.getPointer() < b->Start.getPointer();
        });

        unsigned bufId = _srcMgr.FindBufferContainingLoc(spansOnLine[0]->Start);
        auto lineColBase = _srcMgr.getLineAndColumn(spansOnLine[0]->Start, bufId);
        auto buf = _srcMgr.getMemoryBuffer(bufId);

        os.changeColor(llvm::raw_ostream::CYAN);
        os << "    --> ";
        os.resetColor();
        os << _srcMgr.getBufferInfo(bufId).Buffer->getBufferIdentifier()
           << ':' << lineNo << ':' << lineColBase.second << '\n';

        const char *bufStart = buf->getBufferStart();
        const char *bufEnd = buf->getBufferEnd();
        const char *ptr = spansOnLine[0]->Start.getPointer();
        const char *lineStart = ptr;
        while (lineStart > bufStart && *(lineStart - 1) != '\n') {
            --lineStart;
        }
        const char *lineEnd = ptr;
        while (lineEnd < bufEnd && *lineEnd != '\n') {
            ++lineEnd;
        }
        std::string lineText(lineStart, lineEnd - lineStart);

        std::ostringstream gutterStream;
        gutterStream << std::setw(4) << lineNo << " | ";
        std::string gutter = gutterStream.str();
        os.changeColor(llvm::raw_ostream::CYAN);
        os << gutter;
        os.resetColor();
        os << lineText << '\n';

        std::string indent(gutter.size() - 3, ' ');
        
        os.changeColor(llvm::raw_ostream::CYAN);
        os << indent << " | ";
        os.resetColor();

        int topmostIndex = -1;
        for (int i = (int)spansOnLine.size() - 1; i >= 0; --i) {
            if (!spansOnLine[i]->Label.empty()) {
                topmostIndex = i;
                break;
            }
        }
        
        int currentPos = 1;
        for (int i = 0; i < spansOnLine.size(); ++i) {
            auto *span = spansOnLine[i];
            auto col = _srcMgr.getLineAndColumn(span->Start, bufId).second;
            
            for (; currentPos < col; ++currentPos) {
                os << ' ';
            }
            
            os.changeColor(color, true);
            int len = std::max(1, (int)(span->End.getPointer() - span->Start.getPointer()));
            for (int k = 0; k < len; ++k) {
                os << '~';
            }
            currentPos += len;
            os.resetColor();

            if (i == topmostIndex) {
                os.changeColor(color, true);
                os << ' ' << span->Label;
                os.resetColor();
            }
        }
        os << '\n';

        for (int i = topmostIndex - 1; i >= 0; --i) {
            if (spansOnLine[i]->Label.empty()) continue;

            os.changeColor(llvm::raw_ostream::CYAN);
            os << indent << " | ";
            os.resetColor();

            currentPos = 1;
            for (int j = 0; j <= i; ++j) {
                auto col = _srcMgr.getLineAndColumn(spansOnLine[j]->Start, bufId).second;
                for (; currentPos < col; ++currentPos) {
                    os << ' ';
                }
                
                os.changeColor(color, true);
                if (j == i) {
                    os << "| " << spansOnLine[j]->Label;
                }
                else if (!spansOnLine[j]->Label.empty()) {
                    os << "|";
                }
                ++currentPos;
                os.resetColor();
            }
            os << '\n';
        }
    }

    for (const auto &note : diag.Notes) {
        os.changeColor(llvm::raw_ostream::CYAN, true);
        os << "     = ";
        os.resetColor();
        os.changeColor(llvm::raw_ostream::WHITE, true);
        os << "note: ";
        os.resetColor();
        os << note << '\n';
    }
    for (const auto &help : diag.Helps) {
        os.changeColor(llvm::raw_ostream::GREEN, true);
        os << "     = ";
        os.resetColor();
        os.changeColor(llvm::raw_ostream::WHITE, true);
        os << "help: ";
        os.resetColor();
        os << help << '\n';
    }
    os << '\n';
}

}