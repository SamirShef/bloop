#include <utils/modules/buildDriver.h>
#include <llvm/Support/Path.h>

using namespace bloop;

int
main(int argc, char **argv) {
    if (argc != 2) {
        llvm::errs() << llvm::errs().RED << "Usage: bloop <input_file_path>\n" << llvm::errs().RESET;
        return 1;
    }
    std::string fileName = argv[1];

    fs::path projectRoot = fs::absolute(fileName).parent_path();
    fs::path vendorRoot  = std::string(getenv("HOME")) + "/.bloop/vendor";
    if (!fs::exists(vendorRoot)) {
        fs::create_directories(vendorRoot);
    }
    fs::path stdRoot     = "/usr/lib/bloop/libs";
    fs::path registryPath= std::string(getenv("HOME")) + "/.bloop/registry";
    if (!fs::exists(registryPath)) {
        fs::create_directories(registryPath);
    }

    BuildDriver driver(projectRoot, vendorRoot, stdRoot, registryPath);
    driver.Execute(llvm::sys::path::stem(fileName).str());
    
    return 0;
}