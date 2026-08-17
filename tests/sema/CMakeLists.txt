add_executable(astra-sema-tests
        NameResolutionTest.cpp
        TypeResolutionTest.cpp
)

target_link_libraries(astra-sema-tests
        PRIVATE
        Catch2::Catch2WithMain
        astra-test-support
        astra-sema
        astra-frontend
        astra-lexer
)

# Register one CTest test per TEST_CASE at build time.
catch_discover_tests(astra-sema-tests)
