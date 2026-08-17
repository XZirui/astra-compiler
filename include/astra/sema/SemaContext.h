#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/Type.h"
#include "astra/basic/DiagnosticsEngine.h"
#include "astra/sema/Type.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/SourceMgr.h>

#include <array>

namespace astra::ast {
struct ASTNode;
struct ClassDecl;
struct Declaration;
struct FunctionDecl;
struct Parameter;
struct Program;
struct Statement;
struct TypeParam;
struct VarDecl;
} // namespace astra::ast

namespace astra::sema {
class SemaBuilder;

/// The kinds of lexical scopes. Top-level and class scopes collect all names
/// before any reference is resolved (forward references are legal); the
/// remaining kinds are incremental — names become visible as their
/// declarations are visited, because binding happens at traversal time.
///
/// These scopes are an internal implementation detail of `SemaContext`, not
/// public API.
enum class ScopeKind { TopLevel, Class, Function, Block, Catch, ForEach, For };

/// The name namespaces of a scope, mirroring Kotlin: a class name and a
/// function name (or variable name) may coexist, so lookups are done per
/// namespace instead of over a single table.
///
/// Internal implementation detail of `SemaContext`.
enum class Namespace { Values, Functions, Types };

/// One lexical scope: a parent chain with three name tables. The names are
/// bound when the scope is populated; lookups walk the chain and the nearest
/// binding wins (shadowing falls out naturally). Identifiers are interned by
/// the `ASTContext`, so the tables are keyed by `IdentifierInfo *` and names
/// compare by pointer identity. Internal implementation detail of
/// `SemaContext`.
struct Scope {
  Scope *Parent = nullptr;
  ScopeKind Kind = ScopeKind::Block;
  /// The class this scope belongs to, for `Class` scopes.
  ast::ClassDecl *Class = nullptr;
  llvm::DenseMap<ast::IdentifierInfo *, ast::ASTNode *> Values;
  llvm::DenseMap<ast::IdentifierInfo *, ast::ASTNode *> Functions;
  llvm::DenseMap<ast::IdentifierInfo *, ast::ASTNode *> Types;

  /// The name table of `NS`.
  llvm::DenseMap<ast::IdentifierInfo *, ast::ASTNode *> &map(Namespace NS) {
    switch (NS) {
    case Namespace::Values:
      return Values;
    case Namespace::Functions:
      return Functions;
    case Namespace::Types:
      return Types;
    }
    llvm_unreachable("bad namespace");
  }
};

/// Holds the state and services of the first semantic-analysis phase:
/// declaration collection, scope and name resolution, and canonical type
/// resolution. Traversal lives in `SemaBuilder` (a friend), which uses the
/// services here and mutates the state directly. Results are attached to the
/// AST nodes as fields (`ast::Type::ResolvedType`, `VarExpr::Decl`,
/// `ThisExpr::EnclosingClass`). The public entry point is `sema::analyze`,
/// which constructs a context and runs it.
class SemaContext {
  ast::ASTContext &Ctx;
  basic::DiagnosticsEngine &Diags;
  Scope *CurScope = nullptr;
  /// The innermost enclosing class of the current position.
  ast::ClassDecl *CurClass = nullptr;
  std::array<BuiltinType *, 8> BuiltinCache{};
  Type *ErrorTy = nullptr;
  /// The chain of classes whose type-parameter defaults are being resolved,
  /// used to detect cyclic defaults.
  llvm::SmallVector<ast::ClassDecl *, 4> DefaultChain;

  friend class SemaBuilder;

public:
  SemaContext(ast::ASTContext &Ctx, basic::DiagnosticsEngine &Diags);

  ast::ASTContext &getASTContext() { return Ctx; }

  /// Analyze the program: collect declarations, resolve every name reference
  /// and syntactic type, reporting new diagnostics to the engine.
  void run(ast::Program *P);

private:
  Scope *pushScope(ScopeKind Kind, ast::ClassDecl *Class = nullptr);
  void popScope();
  bool bind(Namespace NS, ast::IdentifierInfo *Name, ast::ASTNode *Node);
  ast::ASTNode *lookup(Namespace NS, ast::IdentifierInfo *Name);
  bool isAccessible(ast::ASTNode *Found);

  void registerDecl(ast::Declaration *D);
  void collectClass(ast::ClassDecl *C);
  BuiltinType *getBuiltinType(ast::BuiltinType::Ty Value);
  Type *getErrorType();
  TypeParamType *getParamType(ast::TypeParam *P);
};

} // namespace astra::sema
