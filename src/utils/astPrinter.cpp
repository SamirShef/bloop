#include <utils/astPrinter.h>

namespace bloop {

void
ASTPrinter::printNode(Node *node) {
    if (!node) {
        return;
    }

    #define NODE(k, f, t) case k: return f(llvm::cast<t>(node));
    switch (node->GetKind()) {
        NODE(NkVarDeclStmt, printVDS, VarDeclStmt);
        NODE(NkVarAsgnStmt, printVAS, VarAsgnStmt);
        NODE(NkFieldAsgnStmt, printFAS, FieldAsgnStmt);
        NODE(NkFuncDeclStmt, printFDS, FuncDeclStmt);
        NODE(NkFuncCallStmt, printFCS, FuncCallStmt);
        NODE(NkMethodCallStmt, printMCS, MethodCallStmt);
        NODE(NkUsingStmt, printUS, UsingStmt);
        NODE(NkRetStmt, printRS, RetStmt);
        NODE(NkIfElseStmt, printIES, IfElseStmt);
        NODE(NkForLoopStmt, printFLS, ForLoopStmt);
        NODE(NkBreakStmt, printBS, BreakStmt);
        NODE(NkContinueStmt, printCS, ContinueStmt);
        NODE(NkBinaryExpr, printBE, BinaryExpr);
        NODE(NkLitExpr, printLE, LiteralExpr);
        NODE(NkUnaryExpr, printUE, UnaryExpr);
        NODE(NkVarExpr, printVE, VarExpr);
        NODE(NkFieldExpr, printFE, FieldExpr);
        NODE(NkFuncCallExpr, printFCE, FuncCallExpr);
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
    _connectionStack.push_back(true);
    printNode(vds->GetExpr());
    _connectionStack.pop_back();
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printVAS(VarAsgnStmt *vas) {
    printIndent();
    _out << "VarAsgnStmt ";
    printLineCol(vas->GetStartLoc());
    _out << ' ' << vas->GetName().Name << '\n';
    _connectionStack.push_back(true);
    printNode(vas->GetExpr());
    _connectionStack.pop_back();
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printFAS(FieldAsgnStmt *fas) {
    printIndent();
    _out << "FieldAsgnStmt ";
    printLineCol(fas->GetStartLoc());
    _out << ' ' << fas->GetName().Name << "from:\n";
    _connectionStack.push_back(true);
    printNode(fas->GetBase());
    _connectionStack.pop_back();
    
    _connectionStack.push_back(true);
    printNode(fas->GetExpr());
    _connectionStack.pop_back();
    if (_connectionStack.empty()) {
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
    for (int i = 0; i < fds->GetBody().size(); ++i) {
        _connectionStack.push_back(i != fds->GetBody().size() - 1);
        printNode(fds->GetBody()[i]);
        _connectionStack.pop_back();
    }
    _out << std::string(fds->GetBody().size() >= 1 + 1, '\n');
}

void
ASTPrinter::printFCS(FuncCallStmt *fcs) {
    printIndent();
    _out << "FuncCallStmt ";
    printLineCol(fcs->GetStartLoc());
    _out << ' ' << fcs->GetFCE()->GetName().Name << '\n';
    for (int i = 0; i < fcs->GetFCE()->GetArgs().size(); ++i) {
        _connectionStack.push_back(i != fcs->GetFCE()->GetArgs().size() - 1);
        auto &a = fcs->GetFCE()->GetArgs()[i];
        printNode(a);
        _connectionStack.pop_back();
    }
    _out << '\n';
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printMCS(MethodCallStmt *mcs) {
    printIndent();
    _out << "MethodCallStmt ";
    printLineCol(mcs->GetStartLoc());
    _out << ' ' << mcs->GetMCE()->GetName().Name << " from \n";
    _connectionStack.push_back(true);
    printNode(mcs->GetMCE()->GetBase());
    _connectionStack.pop_back();
    for (int i = 0; i < mcs->GetMCE()->GetArgs().size(); ++i) {
        _connectionStack.push_back(i != mcs->GetMCE()->GetArgs().size() - 1);
        auto &a = mcs->GetMCE()->GetArgs()[i];
        printNode(a);
        _connectionStack.pop_back();
    }
    _out << '\n';
    
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printUS(UsingStmt *us) {
    printIndent();
    _out << "UsingStmt ";
    printLineCol(us->GetStartLoc());
    _out << " '";
    for (int i = 0; i < us->GetPath().size(); ++i) {
        _out << us->GetPath()[i].Name;
        if (i < us->GetPath().size() - 1) {
            _out << '.';
        }
    }
    _out << "'\n";
    if (_connectionStack.empty()) {
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
        _connectionStack.push_back(true);
        printNode(rs->GetExpr());
        _connectionStack.pop_back();
    }
    else {
        _out << '\n';
    }
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printIES(IfElseStmt *ies) {
    printIndent();
    _out << "IfElseStmt ";
    printLineCol(ies->GetStartLoc());
    _out << '\n';
    _connectionStack.push_back(true);
    printNode(ies->GetCond());
    _connectionStack.pop_back();
    for (int i = 0; i < ies->GetThenBranch().size(); ++i) {
        _connectionStack.push_back(i != ies->GetThenBranch().size() - 1);
        printNode(ies->GetThenBranch()[i]);
        _connectionStack.pop_back();
    }
    if (ies->GetElseBranch().size()) {
        for (int i = 0; i < ies->GetElseBranch().size(); ++i) {
            _connectionStack.push_back(i != ies->GetElseBranch().size() - 1);
            printNode(ies->GetElseBranch()[i]);
            _connectionStack.pop_back();
        }
    }
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printFLS(ForLoopStmt *fls) {
    printIndent();
    _out << "ForLoopStmt ";
    printLineCol(fls->GetStartLoc());
    _out << '\n';

    if (fls->GetIndexator()) {
        _connectionStack.push_back(true);
        printNode(fls->GetIndexator());
        _connectionStack.pop_back();
    }
    
    _connectionStack.push_back(true);
    printNode(fls->GetCond());
    _connectionStack.pop_back();

    if (fls->GetIteration()) {
        _connectionStack.push_back(true);
        printNode(fls->GetIteration());
        _connectionStack.pop_back();
    }
    
    for (int i = 0; i < fls->GetBody().size(); ++i) {
        _connectionStack.push_back(i != fls->GetBody().size() - 1);
        printNode(fls->GetBody()[i]);
        _connectionStack.pop_back();
    }
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printBS(BreakStmt *bs) {
    printIndent();
    _out << "BreakStmt ";
    printLineCol(bs->GetStartLoc());
    _out << '\n';
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printCS(ContinueStmt *cs) {
    printIndent();
    _out << "ContinueStmt ";
    printLineCol(cs->GetStartLoc());
    _out << '\n';
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printBE(BinaryExpr *be) {
    printIndent();
    _out << "BinaryExpr ";
    printLineCol(be->GetStartLoc());
    _out << " '" << be->GetOp().Val << "'\n";
    _connectionStack.push_back(true);
    printNode(be->GetLHS());
    _connectionStack.pop_back();

    _connectionStack.push_back(true);
    printNode(be->GetRHS());
    _connectionStack.pop_back();
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
    _connectionStack.push_back(true);
    printNode(ue->GetRHS());
    _connectionStack.pop_back();
}

void
ASTPrinter::printVE(VarExpr *ve) {
    printIndent();
    _out << "VarExpr ";
    printLineCol(ve->GetStartLoc());
    _out << " '" << ve->GetName().Name << "'\n";
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printFE(FieldExpr *fe) {
    printIndent();
    _out << "FieldExpr ";
    printLineCol(fe->GetStartLoc());
    _out << " '" << fe->GetName().Name << "' from\n";
    _connectionStack.push_back(true);
    printNode(fe->GetBase());
    _connectionStack.pop_back();
    if (_connectionStack.empty()) {
        _out << '\n';
    }
}

void
ASTPrinter::printFCE(FuncCallExpr *fce) {
    printIndent();
    _out << "FuncCallExpr ";
    printLineCol(fce->GetStartLoc());
    _out << ' ' << fce->GetName().Name << '\n';
    for (int i = 0; i < fce->GetArgs().size(); ++i) {
        _connectionStack.push_back(i != fce->GetArgs().size() - 1);
        auto &a = fce->GetArgs()[i];
        printNode(a);
        _connectionStack.pop_back();
    }
    _out << '\n';
    if (_connectionStack.empty()) {
        _out << '\n';
    }
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