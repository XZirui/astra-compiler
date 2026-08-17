add_library(astra-sema)

target_sources(astra-sema
        PRIVATE
        Analyze.cpp
        SemaBuilder.cpp
        SemaContext.cpp
        Type.cpp
)

llvm_map_components_to_libnames(llvm_libs
        support
)

target_include_directories(astra-sema
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(astra-sema
        PUBLIC
        astra-basic
        astra-ast
        ${llvm_libs}
)
