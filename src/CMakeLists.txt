add_compile_options(
        -Wall -Wextra -Wpedantic -Wshadow
        -Wcast-align -Wconversion -Wsign-conversion
)

add_subdirectory(basic)
add_subdirectory(ast)
add_subdirectory(frontend)
add_subdirectory(sema)
