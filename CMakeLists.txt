cmake_minimum_required(VERSION 3.16)
project(tabulang CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -g)

add_library(tabulang_core
  src/token.cpp
  src/lexer.cpp
  src/ast.cpp
  src/parser.cpp
  src/symtab.cpp
  src/sema.cpp
  src/tacgen.cpp
  src/blocks.cpp
)
target_include_directories(tabulang_core PUBLIC src)

add_executable(tblc src/main.cpp)
target_link_libraries(tblc PRIVATE tabulang_core)