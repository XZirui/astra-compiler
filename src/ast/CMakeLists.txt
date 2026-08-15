add_library(astra-ast)

target_sources(astra-ast
        PRIVATE
        ASTDumper.cpp
)

llvm_map_components_to_libnames(llvm_libs
        support
)

target_include_directories(astra-ast
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(astra-ast
        PUBLIC
        ${llvm_libs}
)
