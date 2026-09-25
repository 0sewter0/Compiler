cmake_minimum_required(VERSION 3.15)
project(Compiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
endif()

find_package(LLVM REQUIRED CONFIG PATHS "C:/msys64/clang64/lib/cmake/llvm" NO_DEFAULT_PATH)

set(LLVM_INCLUDE_DIRS "C:/msys64/clang64/include")
include_directories(BEFORE "C:/msys64/clang64/include/c++/v1")
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(compiler 
    src/main.cpp 
    src/MParser.cpp
    src/lexer.cpp 
    src/parser.cpp 
    src/parseDeclaration.cpp 
    src/parseFunction.cpp 
    src/parseLoop.cpp 
    src/codegen.cpp 
    src/codegenHelpers.cpp
    src/semanticAnalysis.cpp)

target_compile_options(compiler PRIVATE
    -nostdinc++
    "-IC:/msys64/clang64/include/c++/v1")
target_link_options(compiler PRIVATE -stdlib=libc++)

llvm_map_components_to_libnames(llvm_libs core support irreader Analysis target executionengine mc)
target_link_libraries(compiler PRIVATE ${llvm_libs})

if (MSVC) 
    add_compile_options(/utf-8 /GR /EHsc /permissive-)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()