add_executable(astra-frontend-tests
        TypesTest.cpp
        DeclarationsTest.cpp
        StatementsTest.cpp
        ExpressionsTest.cpp
)

target_link_libraries(astra-frontend-tests
        PRIVATE
        Catch2::Catch2WithMain
        astra-test-support
        astra-frontend
        astra-lexer
)

# Register one CTest test per TEST_CASE at build time.
catch_discover_tests(astra-frontend-tests)
