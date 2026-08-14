// $antlr-format alignTrailingComments true, columnLimit 150, minEmptyLines 1, maxEmptyLinesToKeep 1, reflowComments false, useTab false
// $antlr-format allowShortRulesOnASingleLine false, allowShortBlocksOnASingleLine true, alignSemicolons hanging, alignColons hanging

parser grammar AstraParser;

options {
    tokenVocab = AstraLexer;
}

file
    : topLevelObject* EOF
    ;

// TODO: Adjust top level objects
topLevelObject
    : declaration
    ;

// TODO class decl
declaration
    : functionDecl
    | variableDecl SEMICOLON
    ;

functionDecl
    : FUN IDENTIFIER LPAREN (parameter (COMMA parameter)*)? RPAREN ARROW type block
    ;

parameter
    : IDENTIFIER COLON type (ASSIGNMENT expression)?
    ;

type
    : (parenType | typeRef | builtinType) (LBRACKET expression RBRACKET)*
    | functionType
    ;

parenType
    : LPAREN type RPAREN
    ;

typeRef
    : IDENTIFIER
    ;

functionType
    : FUN LPAREN paramTypeList? RPAREN ARROW type
    ;

paramTypeList
    : type (COMMA type)*
    ;

// TODO value for template
typeArguments
    : LT typeArgument? GT
    ;

typeArgument
    : type (COMMA type)*
    ;

builtinType
    : VOID
    | BOOL
    | INT
    | LONG
    | FLOAT
    | DOUBLE
    ;

block
    : LBRACE statement* RBRACE
    ;

statement
    : declStatement
    | assignment SEMICOLON
    | controlStatement
    | exprStmt
    ;

declStatement
    : declaration
    ;

// TODO
assignment
    : postfixUnaryExpr assignmentOperator expression
    ;

assignmentOperator
    : ASSIGNMENT
    | ADD_ASSIGNMENT
    | SUB_ASSIGNMENT
    | MULT_ASSIGNMENT
    | DIV_ASSIGNMENT
    | MOD_ASSIGNMENT
    ;

variableDecls
    : variableDecl (COMMA variableDecl)*
    ;

variableDecl
    : VAR IDENTIFIER ((COLON type (ASSIGNMENT expression)?) | ((COLON type)? ASSIGNMENT expression))
    | VAL IDENTIFIER (COLON type)? ASSIGNMENT expression
    ;

controlStatement
    : forStmt
    | forEachStmt
    | whileStmt
    | doWhileStmt
    | ifStmt
// TODO tryStmt
    ;

forStmt
    : FOR LPAREN variableDecls? SEMICOLON expression? SEMICOLON forUpdate?
    RPAREN block
    ;

forUpdate
    : assignment | expression
    ;

forEachStmt
    : FOR LPAREN IDENTIFIER IN expression RPAREN block
    ;

whileStmt
    : WHILE LPAREN expression RPAREN block
    ;

doWhileStmt
    : DO block WHILE LPAREN expression RPAREN
    ;

ifStmt
    : IF LPAREN expression RPAREN block (ELSE (ifStmt | block))?
    ;

exprStmt
    : expression SEMICOLON
    ;

expression
    : disjunction
    ;

disjunction
    : conjunction (DISJ conjunction)*
    ;

conjunction
    : equality (CONJ equality)*
    ;

equalityOperator
    : EQ
    | NEQ
    ;

equality
    : comparison (equalityOperator comparison)?
    ;

comparisonOperator
    : LT
    | GT
    | LE
    | GE
    ;

comparison
    : infixExpr (comparisonOperator infixExpr)?
    ;

inOperator
    : IN
    ;

isOperator
    : IS
    ;

infixExpr
    : elvisExpr (inOperator elvisExpr | isOperator type)*
    ;

elvisOperator
    : ELVIS
    ;

elvisExpr
    : bitwiseOr (elvisOperator bitwiseOr)*
    ;

bitwiseOr
    : bitwiseXor (BIT_OR bitwiseXor)*
    ;

bitwiseXor
    : bitwiseAnd (BIT_XOR bitwiseAnd)*
    ;

bitwiseAnd
    : bitwiseShift (BIT_AND bitwiseShift)*
    ;

bitwiseShiftOperator
    : LSHIFT
    | RSHIFT
    ;

bitwiseShift
    : addition (bitwiseShiftOperator addition)*
    ;

additionOperator
    : ADD
    | SUB
    ;

addition
    : multiplication (additionOperator multiplication)*
    ;

multiplicationOperator
    : MULT
    | DIV
    | MOD
    ;

multiplication
    : asExpr (multiplicationOperator asExpr)*
    ;

asOperator
    : AS
    | AS QUEST
    ;

asExpr
    : prefixUnaryExpr (asOperator type)?
    ;

// TODO annotation
unaryPrefix
    : prefixUnaryOperator
    ;

prefixUnaryOperator
    : ADD
    | SUB
    | NOT
    | BIT_NOT
    ;

prefixUnaryExpr
    : unaryPrefix* postfixUnaryExpr
    ;

unaryPostfix
    : typeArguments
    | callSuffix
    | indexingSuffix
    | navigationSuffix
    ;

callSuffix
    : LPAREN valueArguments? RPAREN
    ;

valueArguments
    : valueArgument (COMMA valueArgument)*
    ;

valueArgument
    : ((IDENTIFIER ASSIGNMENT) | MULT)? expression
    ;

indexingSuffix
    : LBRACKET expression RBRACKET
    ;

navigationSuffix
    : memberAccessOperator IDENTIFIER
    ;

memberAccessOperator
    : QUEST? DOT
    | DOUBLE_COLON
    ;

postfixUnaryExpr
    : primaryExpr unaryPostfix*
    ;

primaryExpr
    : parenExpr
    | literalConstant
    | IDENTIFIER
//    | functionLiteral
    | collectionLiteral
    | thisLiteral
    | ifExpression
    | jumpExpression
    ;

parenExpr
    : LPAREN expression RPAREN
    ;

// TODO CharLiteral and StringLiteral
literalConstant
    : BOOLEAN_LITERAL
    | INTEGER_LITERAL
    | FLOAT_LITERAL
    | DOUBLE_LITERAL
    | NULL_LITERAL
    ;


// TODO
functionLiteral
    :
    ;

// TODO
collectionLiteral
    : LBRACKET (expression (COMMA expression)*)? RBRACKET
    ;

thisLiteral
    : THIS
    ;

ifExpression
    : IF LPAREN expression RPAREN expression (ELSE expression)?
    ;

jumpExpression
    : THROW expression
    | RETURN expression?
    | CONTINUE label?
    | BREAK label?
    ;

// TODO attach to statement
label
    : AT IDENTIFIER
    ;
