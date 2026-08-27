cmake_minimum_required(VERSION 3.15)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(compiler main.cpp lexer.cpp parser.cpp)

llvm_map_components_to_libnames(llvm_libs core support irreader Analysis Target ExecutionEngine MC)
target_link_libraries(compiler PRIVATE ${llvm_libs})