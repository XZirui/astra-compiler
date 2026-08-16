# Contains ANTLR-generated headers (via ASTBuilder.h/Parse.cpp), whose inline
# templates use RTTI; re-enable it (off project-wide by default).
add_library(astra-frontend)

target_sources(astra-frontend
        PRIVATE
        ASTBuilder.cpp
        Parse.cpp
)

target_compile_options(astra-frontend
        PRIVATE
        -frtti
)

llvm_map_components_to_libnames(llvm_libs
        support
)

target_include_directories(astra-frontend
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(astra-frontend
        PUBLIC
        astra-basic
        astra-ast
        astra-parser
        ${llvm_libs}
)
