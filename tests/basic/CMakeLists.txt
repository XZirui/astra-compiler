add_executable(astra-basic-tests
        DiagnosticsTest.cpp
)

target_link_libraries(astra-basic-tests
        PRIVATE
        Catch2::Catch2WithMain
        astra-test-support
        astra-frontend
        astra-lexer
)

# Register one CTest test per TEST_CASE at build time.
catch_discover_tests(astra-basic-tests)
