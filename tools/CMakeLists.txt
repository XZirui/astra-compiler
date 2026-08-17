add_compile_options(
        -Wall -Wextra -Wpedantic -Wshadow
        -Wcast-align -Wconversion -Wsign-conversion
)

add_executable(astra
        astra.cpp
)

llvm_map_components_to_libnames(llvm_libs
        support
)

target_link_libraries(astra
        PRIVATE
        astra-frontend
        astra-sema
        astra-lexer
        ${llvm_libs}
)
