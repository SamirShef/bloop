#pragma once
#include <diag/builder.h>
#include <llvm/Support/SourceMgr.h>

namespace bloop {

class DiagnosticEngine {
    llvm::SourceMgr &_srcMgr;
    unsigned _errorsCount = 0;

public:
    explicit DiagnosticEngine(llvm::SourceMgr &mgr) : _srcMgr(mgr) {}

    DiagnosticBuilder Report(DiagLevel sev, std::string msg) {
        if (sev == DiagLevel::Error) {
            ++_errorsCount;
        }
        return DiagnosticBuilder(*this, sev, msg);
    }

    void Emit(const Diagnostic &diag);

    unsigned GetErrorsCount() const {
        return _errorsCount;
    }
    
    llvm::SourceMgr &GetSourceMgr() {
        return _srcMgr;
    }
};

}