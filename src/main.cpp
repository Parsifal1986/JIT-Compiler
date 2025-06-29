#include "./parser/parser.hpp"
#include "./jitrunner/jitrunner.hpp"
#include "./util/util.hpp"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include <iostream>

int main(int argc, char **argv) {
  llvm::InitLLVM X(argc, argv);
  llvm::cl::opt<std::string> InputFile(llvm::cl::Positional, llvm::cl::desc("<input .ll/.bc>"), llvm::cl::Required);
  llvm::cl::ParseCommandLineOptions(argc, argv, "Naïve IR Runner\n");

  llvm::LLVMContext Ctx;
  try {
    auto Module = loadModuleFromFile(InputFile, Ctx);
    JITRunner Runner(*Module);
    int64_t exitCode = Runner.runModule();
    std::cout << "Program exited with code: " << exitCode << "\n";
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}