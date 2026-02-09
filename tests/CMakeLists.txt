include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Test executables
set(TEST_SOURCES
    test_value.cpp
    test_interner.cpp
    test_lexer.cpp
    test_parser.cpp
    test_evaluator.cpp
    test_scope.cpp
    test_builtins.cpp
    test_integration.cpp
)

add_executable(finescript_tests ${TEST_SOURCES})
target_link_libraries(finescript_tests PRIVATE finescript Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(finescript_tests)
