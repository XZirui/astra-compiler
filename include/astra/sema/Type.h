#pragma once

#include "astra/ast/Type.h"

#include <llvm/ADT/ArrayRef.h>

namespace astra::ast {
struct ClassDecl;
struct Expr;
struct TypeParam;
} // namespace astra::ast

namespace astra::sema {
/// The resolved types produced by semantic analysis. These are the canonical
/// counterparts of the syntactic `ast::Type` nodes: name references are
/// resolved to their declarations, type arguments are validated and defaulted,
/// and type parameters are represented by `TypeParamType`. Instances are
/// allocated in the `ASTContext` arena and never freed.
enum class TypeKind { Builtin, Class, TypeParam, Array, Function, Error };

/// Base class of all resolved types. Defaults to `Error` so that a freshly
/// allocated instance is a safe fallback.
struct Type {
  TypeKind Kind = TypeKind::Error;
};

struct BuiltinType : Type {
  ast::BuiltinType::Ty Value;
  BuiltinType() { Kind = TypeKind::Builtin; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::Builtin; }
};

/// A reference to a class with its (resolved) type arguments. `TypeArgs`
/// always has one entry per declared type parameter; missing ones are filled
/// with the parameter defaults.
struct ClassType : Type {
  ast::ClassDecl *Decl = nullptr;
  llvm::ArrayRef<Type *> TypeArgs;
  ClassType() { Kind = TypeKind::Class; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::Class; }
};

/// A reference to a declared type parameter, e.g. `T` inside `class Box<T>`.
struct TypeParamType : Type {
  ast::TypeParam *Param = nullptr;
  TypeParamType() { Kind = TypeKind::TypeParam; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::TypeParam; }
};

/// A resolved array type. `Size` points at the syntactic size expression
/// (`nullptr` if none was written); constant folding of the size is left to a
/// later constant-evaluation phase.
struct ArrayType : Type {
  Type *Element = nullptr;
  ast::Expr *Size = nullptr;
  ArrayType() { Kind = TypeKind::Array; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::Array; }
};

/// A resolved function type.
struct FunctionType : Type {
  llvm::ArrayRef<Type *> Params;
  Type *Return = nullptr;
  FunctionType() { Kind = TypeKind::Function; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::Function; }
};

/// The error sentinel used when a type fails to resolve, so that the rest of
/// the program is still analyzed without cascading errors.
struct ErrorType : Type {
  ErrorType() { Kind = TypeKind::Error; }
  static bool classof(const Type *T) { return T->Kind == TypeKind::Error; }
};

/// Structural type equality: two builtins are equal when their enum values
/// match, two class types when they name the same declaration with equal type
/// arguments, array types when element and size match (integer literal sizes
/// are compared by value), and so on. Every `ErrorType` equals every other.
bool sameType(const Type *A, const Type *B);

} // namespace astra::sema
