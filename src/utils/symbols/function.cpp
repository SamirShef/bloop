#include <utils/symbols/function.h>
#include <utils/modules/module.h>
#include <sstream>

namespace bloop {

std::string
Function::GetMangledName() const {
    std::stringstream ss;
    ss << Name.Name;
    for (auto &a : Args) {
        ss << a.Type->ToString();
    }
    if (!Parent) {
        return ss.str();
    }
    return Parent->ToString() + "." + ss.str();
}

}