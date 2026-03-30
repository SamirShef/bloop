#include <utils/astPrinter.h>

namespace bloop {

void
ASTPrinter::printNode(Node *node) {
    if (!node) {
        return;
    }

    #define NODE(k, f, t) case k: return f(static_cast<t *>(node));
    switch (node->GetKind()) {
        NODE(NkVarDeclStmt, printVDS, VarDeclStmt);
        NODE(NkFuncDeclStmt, printFDS, FuncDeclStmt);
        NODE(NkUsingStmt, printUS, UsingStmt);
        NODE(NkRetStmt, printRS, RetStmt);
        NODE(NkBinaryExpr, printBE, BinaryExpr);
        NODE(NkLitExpr, printLE, LiteralExpr);
        NODE(NkUnaryExpr, printUE, UnaryExpr);
        NODE(NkVarExpr, printVE, VarExpr);
    }
    #undef NODE
}

void
ASTPrinter::printVDS(VarDeclStmt *vds) {
    printIndent();
    _out << "VarDeclStmt ";
    printLineCol(vds->GetStartLoc());
    _out << ' ';
    printType(vds->GetType());
    if (vds->IsConst()) {
        _out << " const";
    }
    _out << ' ' << vds->GetName().Name << '\n';
    ++_indent;
    printNode(vds->GetExpr());
    --_indent;
    if (!_indent) {
        _out << '\n';
    }
}

void
ASTPrinter::printFDS(FuncDeclStmt *fds) {
    printIndent();
    _out << "FuncDeclStmt ";
    printLineCol(fds->GetStartLoc());
    _out << ' ' << fds->GetName().Name << " (";
    for (int i = 0; i < fds->GetArgs().size(); ++i) {
        auto &a = fds->GetArgs()[i];
        _out << a.Name.Name << ": ";
        printType(a.Type);
        if (a.DefaultVal) {
            _out << " <has default>";
        }
        if (i < fds->GetArgs().size() - 1) {
            _out << ", ";
        }
    }
    _out << ") ";
    printType(fds->GetRetType());
    _out << '\n';
    ++_indent;
    for (auto &s : fds->GetBody()) {
        printNode(s);
    }
    --_indent;
    _out << std::string(fds->GetBody().size() >= 1 + 1, '\n');
}

void
ASTPrinter::printUS(UsingStmt *us) {
    printIndent();
    _out << "UsingStmt ";
    printLineCol(us->GetStartLoc());
    _out << " '" << us->GetPath().Name << "'\n";
    if (!_indent) {
        _out << '\n';
    }
}

void
ASTPrinter::printRS(RetStmt *rs) {
    printIndent();
    _out << "RetStmt ";
    printLineCol(rs->GetStartLoc());
    if (rs->GetExpr()) {
        _out << '\n';
        ++_indent;
        printNode(rs->GetExpr());
        --_indent;
    }
    else {
        _out << '\n';
    }
    if (!_indent) {
        _out << '\n';
    }
}

void
ASTPrinter::printBE(BinaryExpr *be) {
    printIndent();
    _out << "BinaryExpr ";
    printLineCol(be->GetStartLoc());
    _out << " '" << be->GetOp().Val << "'\n";
    ++_indent;
    printNode(be->GetLHS());
    printNode(be->GetRHS());
    --_indent;
}

void
ASTPrinter::printLE(LiteralExpr *le) {
    printIndent();
    _out << "LiteralExpr ";
    printLineCol(le->GetStartLoc());
    _out << " ";
    printType(le->GetVal().Type);
    _out << " '" << le->GetVal().ToString() << "'\n";
}

void
ASTPrinter::printUE(UnaryExpr *ue) {
    printIndent();
    _out << "UnaryExpr ";
    printLineCol(ue->GetStartLoc());
    _out << " '" << ue->GetOp().Val << "'\n";
    ++_indent;
    printNode(ue->GetRHS());
    --_indent;
}

void
ASTPrinter::printVE(VarExpr *ve) {
    printIndent();
    _out << "VarExpr ";
    printLineCol(ve->GetStartLoc());
    _out << " '" << ve->GetName().Name << "'";
}

void
ASTPrinter::printType(Type *t) {
    if (!t) {
        _out << "<type not inferred>";
        return;
    }
    _out << t->ToString();
}

}