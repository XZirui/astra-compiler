add_executable(astra-ast-tests
        DumperTest.cpp
)

target_link_libraries(astra-ast-tests
        PRIVATE
        Catch2::Catch2WithMain
        astra-test-support
        astra-frontend
        astra-lexer
)

# Register one CTest test per TEST_CASE at build time.
catch_discover_tests(astra-ast-tests)
