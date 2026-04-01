#include <utils/options.h>
#include <utils/modules/buildDriver.h>
#include <llvm/Support/Path.h>

using namespace bloop;

int
main(int argc, char **argv) {
    llvm::cl::HideUnrelatedOptions(Category);
    llvm::cl::ParseCommandLineOptions(argc, argv, "Bloop Compiler\n");

    std::string fileName = InputFilename;

    fs::path projectRoot = BuildDriver::GetProjectRoot(fs::current_path());
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
    if (!InputFilename.empty()) {
        llvm::errs() << llvm::errs().RED << "Compiler limitation: single-file compilation is unsupported; please compile the entire project\n" << llvm::errs().RESET;
        return 1;
    }
    if (BuildSub) {
        driver.BuildProj();
    }
    else {
        auto strOpt = []() -> std::string {
            if (BuildSub) {
                return "build";
            }
            else if (InitSub) {
                return "init";
            }
            else if (NewSub) {
                return "new";
            }
            else if (FetchSub) {
                return "fetch";
            }
        };
        llvm::errs() << llvm::errs().RED << "Compiler limitation: option '" + strOpt() + "' is currently unimplemented\n" << llvm::errs().RESET;
        return 1;
    }
    
    return 0;
}