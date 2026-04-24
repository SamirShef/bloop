#include <utils/options.h>
#include <utils/modules/buildDriver.h>
#include <utils/modules/fileNode.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/CommandLine.h>

using namespace bloop;

int
createNewProj(const std::string &name);

int
initProj(const std::string &name);

void
clearArtefacts(fs::path projectRoot, const std::string &projectName);

int
main(int argc, char **argv) {
    llvm::cl::HideUnrelatedOptions(Category);
    llvm::cl::ParseCommandLineOptions(argc, argv, "Bloop Compiler\n");

    if (argc < 2) {
        llvm::cl::PrintHelpMessage();
        return 0;
    }
    
    std::string fileName = InputFilename;

    if (!InputFilename.empty()) {
        llvm::errs() << llvm::errs().RED << "Compiler limitation: single-file compilation is unsupported; please compile the entire project\n" << llvm::errs().RESET;
        return 1;
    }
    if (BuildSub || RunSub) {
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
        driver.BuildProj();
        if (RunSub) {
            auto m = BuildDriver::ParseToml(projectRoot.stem().string(), projectRoot / "bloop.toml");
            std::system(("." / projectRoot / "build" / "obj" / m.PackageName).string().c_str());
        }
    }
    else if (NewSub) {
        return createNewProj(NewName);
    }
    else if (InitSub) {
        return initProj(fs::current_path().stem().string());
    }
    else if (ClearSub) {
        fs::path projectRoot = BuildDriver::GetProjectRoot(fs::current_path());
        auto m = BuildDriver::ParseToml(projectRoot.stem().string(), projectRoot / "bloop.toml");
        clearArtefacts(projectRoot, m.PackageName);
    }
    else if (FetchSub) {
        llvm::errs() << llvm::errs().RED << "Compiler limitation: option 'fetch ' is currently unimplemented\n" << llvm::errs().RESET;
        return 1;
    }
    else {
        llvm::cl::PrintHelpMessage();
        return 1;
    }
    
    return 0;
}

int
createNewProj(const std::string &name) {
    if (fs::exists(name) && fs::is_directory(name)) {
        llvm::errs() << llvm::errs().RED << "Error: directory '" + name + "' already exists\n" << llvm::errs().RESET;
        return 1;
    }
    fs::create_directory(name);
    fs::current_path(name);
    return initProj(name);
}

int
initProj(const std::string &name) {
    fs::path root = fs::current_path();
    fs::path manifPath = root / "bloop.toml";
    fs::path srcPath = root / "src" / "main.bl";
    std::ofstream manif(manifPath);
    if (manif.is_open()) {
        manif << "[package]\n";
        manif << "name = \"" << name << "\"\n";
        manif << "root = \"src/main.bl\"";
        manif.close();
    }
    else {
        llvm::errs() << llvm::errs().RED << "Error: Could not create file 'bloop.toml'\n" << llvm::errs().RESET;
        return 1;
    }
    fs::create_directories(srcPath.parent_path());
    std::ofstream mainFile(srcPath);
    if (mainFile.is_open()) {
        mainFile << "func main(): i32 {\n";
        mainFile << "    return 0;\n";
        mainFile << "}";
        mainFile.close();
    }
    else {
        llvm::errs() << llvm::errs().RED << "Error: Could not create file 'src/main.bl'\n" << llvm::errs().RESET;
        return 1;
    }
    return 0;
}

void
clearArtefacts(fs::path projectRoot, const std::string &projectName) {
    fs::path artefactsDir = projectRoot / "build" / "obj";
    if (fs::exists(artefactsDir)) {
        fs::remove_all(artefactsDir);
    }
    fs::path binaryPath = projectRoot / "build" / projectName;
    if (fs::exists(binaryPath)) {
        fs::remove(binaryPath);
    }
}