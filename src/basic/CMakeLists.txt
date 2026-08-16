add_library(astra-basic)

target_sources(astra-basic
        PRIVATE
        DiagnosticsEngine.cpp
        FloatParse.cpp
)

llvm_map_components_to_libnames(llvm_libs
        support
)

target_include_directories(astra-basic
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(astra-basic
        PUBLIC
        ${llvm_libs}
)
