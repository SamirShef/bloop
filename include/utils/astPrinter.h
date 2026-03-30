#pragma once
#include <ast/ast.h>
#include <llvm/Support/SourceMgr.h>
#include <ostream>
#include <vector>

namespace bloop {

class ColorManip {
private:
    const char *_code;
public:
    ColorManip(const char *c) : _code(c) {}
    
    friend std::ostream &
    operator<<(std::ostream &os, const ColorManip &manip) {
        os << manip._code;
        return os;
    }
};

#define COL(n, v) inline ColorManip n(v);

COL(Black, "\e[0;30m");
COL(Red, "\e[0;31m");
COL(Green, "\e[0;32m");
COL(Yellow, "\e[0;33m");
COL(Blue, "\e[0;34m");
COL(Purple, "\e[0;35m");
COL(Cyan, "\e[0;36m");
COL(White, "\e[0;37m");
COL(BoldBlack, "\e[1;30m");
COL(BoldRed, "\e[1;31m");
COL(BoldGreen, "\e[1;32m");
COL(BoldYellow, "\e[1;33m");
COL(BoldBlue, "\e[1;34m");
COL(BoldPurple, "\e[1;35m");
COL(BoldCyan, "\e[1;36m");
COL(BoldWhite, "\e[1;37m");
COL(Reset, "\e[0m");

#undef COL

class ASTPrinter {
    int _indent = 0;
    llvm::SourceMgr &_srcMgr;
    std::ostream &_out;

public:
    explicit ASTPrinter(llvm::SourceMgr &s, std::ostream &o) : _srcMgr(s), _out(o) {}

    void
    Print(std::vector<Stmt *> &ast, ColorManip col = White) {
        _out << col;
        for (auto &s : ast) {
            printNode(s);
        }
        _out << Reset;
    }

private:
    void
    printNode(Node *node);

    void
    printVDS(VarDeclStmt *vds);
    
    void
    printFDS(FuncDeclStmt *fds);

    void
    printUS(UsingStmt *us);

    void
    printRS(RetStmt *rs);

    void
    printBE(BinaryExpr *be);

    void
    printLE(LiteralExpr *le);

    void
    printUE(UnaryExpr *ue);

    void
    printVE(VarExpr *ve);

    void
    printType(Type *t);

    void
    printIndent() {
        if (_indent) {
            _out << std::string((_indent - 1) * 3, ' ');
            _out << "|_ ";
        }
    }

    void
    printLineCol(llvm::SMLoc pos) {
        unsigned bufferId = _srcMgr.FindBufferContainingLoc(pos);
        auto lineAndCol = _srcMgr.getLineAndColumn(pos, bufferId);
        _out << "<line: " << lineAndCol.first << ", col: " << lineAndCol.second << '>';
    }
};

}