# Top‑level build script for the Naïve LLVM IR Runner
cmake_minimum_required(VERSION 3.15)
project(LLVMJIT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# ------------------------------------------------------------
# Locate LLVM (expects an installed or built‑from‑source LLVM
# that exports LLVMConfig.cmake).  If multiple LLVM versions
# are present, specify one with -DLLVM_DIR=<path> when running
# cmake ..
# ------------------------------------------------------------
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/parser/parser.cpp
    src/jitrunner/jitrunner.cpp
)

add_executable(naive_ir_runner ${SOURCES})

# ------------------------------------------------------------
# Link only the minimal components we rely on.  `core` gives
# us LLVMCore + required deps; `support` provides LLVMSupport.
# llvm_map_components_to_libnames expands them into actual lib
# names that exist on the current LLVM build.
# ------------------------------------------------------------
llvm_map_components_to_libnames(LLVM_LIBS core support irreader bitreader)

target_link_libraries(naive_ir_runner PRIVATE ${LLVM_LIBS})

# Thread libs (Linux) — LLVMSupport may transitively need them
if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(naive_ir_runner PRIVATE Threads::Threads)
endif()

install(TARGETS naive_ir_runner RUNTIME DESTINATION bin)