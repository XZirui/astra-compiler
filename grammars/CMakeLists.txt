antlr_target(AstraLexer
        AstraLexer.g4
        LEXER
        OUTPUT_DIRECTORY_PREFIX astra/lexer
        PACKAGE astra::lexer
)

antlr_target(AstraParser
        AstraParser.g4
        PARSER
        DEPENDS_ANTLR AstraLexer
        OUTPUT_DIRECTORY_PREFIX astra/parser
        PACKAGE astra::parser
        VISITOR
        COMPILE_FLAGS -lib ${ANTLR_AstraLexer_OUTPUT_DIR}/astra/lexer
)

include_directories(
        ${ANTLR_AstraLexer_OUTPUT_DIR}
        ${ANTLR_AstraParser_OUTPUT_DIR}
)

# ANTLR-generated code uses dynamic_cast, which requires RTTI. It is off
# project-wide by default (see the top-level CMakeLists.txt).
add_library(astra-lexer
        ${ANTLR_AstraLexer_CXX_OUTPUTS}
)

add_library(astra-parser
        ${ANTLR_AstraParser_CXX_OUTPUTS}
)

target_compile_options(astra-lexer
        PRIVATE
        -frtti
)

target_compile_options(astra-parser
        PRIVATE
        -frtti
)

target_include_directories(astra-lexer PUBLIC
        ${ANTLR_AstraLexer_OUTPUT_DIR}
)

target_include_directories(astra-parser PUBLIC
        ${ANTLR_AstraParser_OUTPUT_DIR}
)

target_link_libraries(astra-parser PUBLIC
        antlr4_static
)

target_link_libraries(astra-lexer PUBLIC
        antlr4_static
)

