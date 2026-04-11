#include <utils/symbols/struct.h>
#include <utils/modules/module.h>

namespace bloop {

std::string
Struct::GetMangledName() const {
    if (!Parent) {
        return Name.Name;
    }
    return Parent->ToString() + "." + Name.Name;
}

}