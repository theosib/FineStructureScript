cmake_minimum_required(VERSION 3.16)
project(finescript VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build shared library by default; override with -DBUILD_SHARED_LIBS=OFF for static
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Ensure PIC for shared library compatibility
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Library target
add_library(finescript
    src/value.cpp
    src/interner.cpp
    src/map_data.cpp
    src/lexer.cpp
    src/parser.cpp
    src/scope.cpp
    src/evaluator.cpp
    src/execution_context.cpp
    src/script_engine.cpp
    src/builtins.cpp
)
target_include_directories(finescript PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(finescript PUBLIC cxx_std_17)

set_target_properties(finescript PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Install rules
include(GNUInstallDirs)
install(TARGETS finescript
    EXPORT finescriptTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/finescript
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT finescriptTargets
    FILE finescriptTargets.cmake
    NAMESPACE finescript::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/finescript
)

# Testing
option(FINESCRIPT_BUILD_TESTS "Build tests" ON)
if(FINESCRIPT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
