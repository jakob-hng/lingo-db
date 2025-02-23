#include "execution/CBackend.h"
#include "execution/BackendPasses.h"
#include "execution/Error.h"

#include "mlir/Conversion/SCFToControlFlow/SCFToControlFlow.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/ExecutionEngine/ExecutionEngine.h"
#include "mlir/IR/Builders.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"

#include <csignal>
#include <filesystem>
#include <fstream>
#include <regex>
#include <spawn.h>

#include "dlfcn.h"
#include "unistd.h"

namespace {


class DefaultCBackend : public execution::ExecutionBackend {
   void execute(mlir::ModuleOp& moduleOp, runtime::ExecutionContext* executionContext) override {
      if (auto getExecContextFn = mlir::dyn_cast_or_null<mlir::func::FuncOp>(moduleOp.lookupSymbol("rt_get_execution_context"))) {
         getExecContextFn.erase();
      }

      std::string translatedModule;
      llvm::raw_string_ostream sstream(translatedModule);

      if (execution::emitC(moduleOp.getOperation(), sstream,false).failed()) {
         error.emit() << "Can not translate module to c++";
         return;
      }
      std::regex r("void main\\(\\) \\{");
      translatedModule = std::regex_replace(translatedModule, r, "extern \"C\" void mainFunc() {");

      std::ofstream outputFile("mlir-c-module.cpp");
      outputFile << "#include<cstdint>\n"
                    "#include<tuple>\n"
                    "#include \"runtime/helpers.h\"\n"
                    "#include \"runtime/Buffer.h\"\n"
                    "namespace std {\n"
                    "template <>\n"
                    "struct make_unsigned<__int128> {\n"
                    "   typedef __uint128_t type;\n"
                    "};\n"
                    "}"
                    "int8_t* executionContext;\n"
                    "extern \"C\" int8_t* rt_get_execution_context(){\n"
                    "\treturn executionContext;\t\n"
                    "}\n"
                    "extern \"C\" void rt_set_execution_context(int8_t* c){\n"
                    "\texecutionContext=c;\n"
                    "}"
                    "size_t hash_64(size_t val){\n"
                    "\tsize_t p1=11400714819323198549ull;\n"
                    "\tsize_t m1=val*p1;\n"
                    "\treturn m1 ^ __builtin_bswap64(m1);\n"
                    "}"
                    "size_t hash_combine(size_t h1, size_t h2){\n"
                    "\treturn h2 ^ __builtin_bswap64(h1);\n"
                    "}"
                    "extern \"C\" size_t hashVarLenData(runtime::VarLen32);"
                 << std::endl;
      outputFile << translatedModule << std::endl;
      outputFile.close();
      usleep(20000);

      auto currPath = std::filesystem::current_path();
      std::string cmd = " clang++ -shared -O0 -g -gdwarf-4 -fPIC -Wl,--export-dynamic -x c++ -std=c++20 -I " + std::string(SOURCE_DIR) + "/include mlir-c-module.cpp -o c-backend.so";
      auto* pPipe = ::popen(cmd.c_str(), "r");
      if (pPipe == nullptr) {
         error.emit() << "Could not compile query module statically (Pipe could not be opened)";
         return;
      }
      std::array<char, 256> buffer;
      std::string result;
      while (not std::feof(pPipe)) {
         auto bytes = std::fread(buffer.data(), 1, buffer.size(), pPipe);
         result.append(buffer.data(), bytes);
      }
      auto rc = ::pclose(pPipe);
      if (WEXITSTATUS(rc)) {
         error.emit() << "Could not compile query module statically (Pipe could not be closed)";
         return;
      }
      void* handle = dlopen(std::string(currPath.string() + "/c-backend.so").c_str(), RTLD_LAZY);
      const char* dlsymError = dlerror();
      if (dlsymError) {
         error.emit() << "Can not open static library: " << std::string(dlsymError);
         return;
      }
      auto mainFunc = reinterpret_cast<execution::mainFnType>(dlsym(handle, "mainFunc"));
      dlsymError = dlerror();
      if (dlsymError) {
         dlclose(handle);
         error.emit() << "Could not load symbol for main function: " << std::string(dlsymError);
         return;
      }
      auto setExecutionContextFn = reinterpret_cast<execution::setExecutionContextFnType>(dlsym(handle, "rt_set_execution_context"));
      dlsymError = dlerror();
      if (dlsymError) {
         dlclose(handle);
         error.emit() << "Could not load symbol for rt_set_execution_context function: " << std::string(dlsymError);
         return;
      }
      setExecutionContextFn(executionContext);
      std::vector<size_t> measuredTimes;
      for (size_t i = 0; i < numRepetitions; i++) {
         auto executionStart = std::chrono::high_resolution_clock::now();
         mainFunc();
         auto executionEnd = std::chrono::high_resolution_clock::now();
         measuredTimes.push_back(std::chrono::duration_cast<std::chrono::microseconds>(executionEnd - executionStart).count() / 1000.0);
      }
      dlclose(handle);
   }
   bool requiresSnapshotting() override {
      return true;
   }
};
} // namespace

std::unique_ptr<execution::ExecutionBackend> execution::createCBackend() {
   return std::make_unique<DefaultCBackend>();
}