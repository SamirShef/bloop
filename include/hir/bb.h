#include <hir/node.h>
#include <string>
#include <vector>

namespace bloop {

class HIRFuncDeclStmt;

class HIRBasicBlock : public HIRNode {
    std::string _name;
    HIRFuncDeclStmt *_parent;
    std::vector<HIRNode *> _instructions;
    HIRNode *_terminator = nullptr;

public:
    explicit HIRBasicBlock(std::string n, HIRFuncDeclStmt *p) : _name(n), _parent(p), HIRNode(HIRNkBasicBlock) {}

    void
    AddInstruction(HIRNode *node) {
        if (_terminator) {
            return;
        }
        _instructions.push_back(node);
    }

    void
    SetTerminator(HIRNode *node) {
        if (_terminator) {
            return;
        }
        _terminator = node;
        _instructions.push_back(node);
    }

    bool
    HasTerminator() const {
        return _terminator != nullptr;
    }

    std::vector<HIRNode *> &
    GetInstructions() {
        return _instructions;
    }

    HIRFuncDeclStmt *
    GetParent() {
        return _parent;
    }
    
    const std::string &
    GetName() {
        return _name;
    }
};

}