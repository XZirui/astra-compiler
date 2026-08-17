#include "astra/ast/ASTDumper.h"

#include "astra/ast/Program.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

namespace astra::ast {

void ASTDumper::pushChild(llvm::SmallVectorImpl<Child> &Children,
                          llvm::StringRef Label, const ASTNode *Node) {
  if (Node) {
    Children.push_back({Label, Node});
  }
}

llvm::StringRef ASTDumper::getOpSymbol(Op Operator) {
  switch (Operator) {
  case Op::Add:
    return "+";
  case Op::Sub:
    return "-";
  case Op::Mult:
    return "*";
  case Op::Div:
    return "/";
  case Op::Mod:
    return "%";
  case Op::Not:
    return "!";
  case Op::Eq:
    return "==";
  case Op::Neq:
    return "!=";
  case Op::LShift:
    return "<<";
  case Op::RShift:
    return ">>";
  case Op::BitAnd:
    return "&";
  case Op::BitXor:
    return "^";
  case Op::BitOr:
    return "|";
  case Op::BitNot:
    return "~";
  case Op::Lt:
    return "<";
  case Op::Gt:
    return ">";
  case Op::Le:
    return "<=";
  case Op::Ge:
    return ">=";
  case Op::Disj:
    return "||";
  case Op::Conj:
    return "&&";
  case Op::In:
    return "in";
  case Op::Is:
    return "is";
  case Op::As:
    return "as";
  case Op::AsQuest:
    return "as?";
  case Op::Elvis:
    return "?:";
  case Op::Assignment:
    return "=";
  }
  llvm_unreachable("Unknown ast::Op.");
}

llvm::StringRef ASTDumper::getBuiltinTypeName(BuiltinType::Ty Value) {
  switch (Value) {
  case BuiltinType::Void:
    return "Void";
  case BuiltinType::Bool:
    return "Bool";
  case BuiltinType::Int:
    return "Int";
  case BuiltinType::Long:
    return "Long";
  case BuiltinType::Float:
    return "Float";
  case BuiltinType::Double:
    return "Double";
  case BuiltinType::Char:
    return "Char";
  case BuiltinType::String:
    return "String";
  }
  llvm_unreachable("Unknown BuiltinType.");
}

llvm::StringRef ASTDumper::getVisibilityName(Visibility Vis) {
  switch (Vis) {
  case Visibility::Public:
    return "public";
  case Visibility::Private:
    return "private";
  case Visibility::Protected:
    return "protected";
  }
  llvm_unreachable("Unknown Visibility.");
}

void ASTDumper::dumpHeader(const ASTNode *Node) {
  switch (Node->getKind()) {
  case NodeKind::Program:
    OS << "Program";
    break;
  case NodeKind::TopLevelObject:
    OS << "TopLevelObject";
    break;
  case NodeKind::FunctionDecl: {
    auto *Fn = llvm::cast<const FunctionDecl>(Node);
    OS << "FunctionDecl '" << Fn->Name->getName() << "' ["
       << getVisibilityName(Fn->Vis) << "]";
    break;
  }
  case NodeKind::ClassDecl: {
    auto *Cls = llvm::cast<const ClassDecl>(Node);
    OS << "ClassDecl '" << Cls->Name->getName() << "' ["
       << getVisibilityName(Cls->Vis) << "]";
    break;
  }
  case NodeKind::VarDecl: {
    auto *Var = llvm::cast<const VarDecl>(Node);
    OS << "VarDecl '" << Var->Name->getName() << "' ["
       << getVisibilityName(Var->Vis) << "]";
    if (Var->IsMutable) {
      OS << " [mutable]";
    }
    break;
  }
  case NodeKind::Parameter: {
    auto *Param = llvm::cast<const Parameter>(Node);
    OS << "Parameter '" << Param->Name->getName() << "'";
    break;
  }
  case NodeKind::TypeRef: {
    auto *Ref = llvm::cast<const TypeRef>(Node);
    OS << "TypeRef '" << Ref->Name->getName() << "'";
    // `Box<>` and `Box` differ: the empty list forces default parameters.
    if (Ref->ExplicitTypeArgs) {
      OS << " '<>'";
    }
    break;
  }
  case NodeKind::BuiltinType: {
    auto *B = llvm::cast<const BuiltinType>(Node);
    OS << "BuiltinType '" << getBuiltinTypeName(B->Value) << "'";
    break;
  }
  case NodeKind::ArrayType:
    OS << "ArrayType";
    break;
  case NodeKind::FunctionType:
    OS << "FunctionType";
    break;
  case NodeKind::Block:
    OS << "Block";
    break;
  case NodeKind::DeclStatement:
    OS << "DeclStatement";
    break;
  case NodeKind::ExprStmt:
    OS << "ExprStmt";
    break;
  case NodeKind::AssignmentStmt: {
    auto *Stmt = llvm::cast<const AssignmentStmt>(Node);
    OS << "AssignmentStmt '" << getOpSymbol(Stmt->Operator) << "'";
    break;
  }
  case NodeKind::IfStmt:
    OS << "IfStmt";
    break;
  case NodeKind::ForStmt:
    OS << "ForStmt";
    break;
  case NodeKind::ForEachStmt: {
    auto *Stmt = llvm::cast<const ForEachStmt>(Node);
    OS << "ForEachStmt '" << Stmt->VarName << "'";
    break;
  }
  case NodeKind::WhileStmt:
    OS << "WhileStmt";
    break;
  case NodeKind::DoWhileStmt:
    OS << "DoWhileStmt";
    break;
  case NodeKind::TryStmt:
    OS << "TryStmt";
    break;
  case NodeKind::NullLiteral:
    OS << "NullLiteral";
    break;
  case NodeKind::BoolLiteral: {
    auto *Lit = llvm::cast<const BoolLiteral>(Node);
    OS << "BoolLiteral '" << (Lit->Value ? "true" : "false") << "'";
    break;
  }
  case NodeKind::IntLiteral: {
    auto *Lit = llvm::cast<const IntLiteral>(Node);
    OS << "IntLiteral '";
    llvm::SmallString<32> Buf;
    Lit->Value.toString(Buf, 10);
    OS << Buf << "'";
    break;
  }
  case NodeKind::FloatLiteral: {
    auto *Lit = llvm::cast<const FloatLiteral>(Node);
    OS << "FloatLiteral '";
    llvm::SmallString<64> Buf;
    Lit->Value.toString(Buf);
    OS << Buf << "'";
    break;
  }
  case NodeKind::StringLiteral: {
    auto *Lit = llvm::cast<const StringLiteral>(Node);
    OS << "StringLiteral '";
    for (char C : Lit->Value) {
      // Escape backslashes, line breaks and other control characters so the
      // tree output stays on one line per node.
      switch (C) {
      case '\\':
        OS << "\\\\";
        break;
      case '\n':
        OS << "\\n";
        break;
      case '\r':
        OS << "\\r";
        break;
      case '\t':
        OS << "\\t";
        break;
      default: {
        auto Byte = static_cast<unsigned char>(C);
        if (Byte < 0x20 || Byte == 0x7F) {
          static constexpr char Hex[] = "0123456789abcdef";
          OS << "\\x" << Hex[Byte >> 4] << Hex[Byte & 0xF];
        } else {
          OS << C;
        }
        break;
      }
      }
    }
    OS << "'";
    break;
  }
  case NodeKind::CharLiteral: {
    auto *Lit = llvm::cast<const CharLiteral>(Node);
    OS << "CharLiteral ";
    if (Lit->Value >= 32 && Lit->Value < 127) {
      OS << "'" << static_cast<char>(Lit->Value) << "'";
    } else {
      // Non-printable values dump as their code point, like `IntLiteral`.
      OS << Lit->Value;
    }
    break;
  }
  case NodeKind::VarExpr: {
    auto *E = llvm::cast<const VarExpr>(Node);
    OS << "VarExpr '" << E->Name->getName() << "'";
    break;
  }
  case NodeKind::UnaryExpr: {
    auto *E = llvm::cast<const UnaryExpr>(Node);
    OS << "UnaryExpr '" << getOpSymbol(E->Operator) << "'";
    break;
  }
  case NodeKind::BinaryExpr: {
    auto *E = llvm::cast<const BinaryExpr>(Node);
    OS << "BinaryExpr '" << getOpSymbol(E->Operator) << "'";
    break;
  }
  case NodeKind::ComparisonChainExpr: {
    auto *E = llvm::cast<const ComparisonChainExpr>(Node);
    OS << "ComparisonChainExpr";
    for (Op Operator : E->Operators) {
      OS << " '" << getOpSymbol(Operator) << "'";
    }
    break;
  }
  case NodeKind::IfExpr:
    OS << "IfExpr";
    break;
  case NodeKind::ThrowExpr:
    OS << "ThrowExpr";
    break;
  case NodeKind::ReturnExpr:
    OS << "ReturnExpr";
    break;
  case NodeKind::ContinueExpr: {
    auto *E = llvm::cast<const ContinueExpr>(Node);
    OS << "ContinueExpr";
    if (E->Pos) {
      OS << " '" << E->Pos->Name->getName() << "'";
    }
    break;
  }
  case NodeKind::BreakExpr: {
    auto *E = llvm::cast<const BreakExpr>(Node);
    OS << "BreakExpr";
    if (E->Pos) {
      OS << " '" << E->Pos->Name->getName() << "'";
    }
    break;
  }
  case NodeKind::CallExpr: {
    auto *E = llvm::cast<const CallExpr>(Node);
    OS << "CallExpr";
    // `foo<>()` and `foo()` differ: the empty list forces default parameters.
    if (E->ExplicitTypeArgs) {
      OS << " '<>'";
    }
    break;
  }
  case NodeKind::IndexExpr:
    OS << "IndexExpr";
    break;
  case NodeKind::MemberExpr: {
    auto *E = llvm::cast<const MemberExpr>(Node);
    OS << "MemberExpr '" << E->Member << "'";
    if (E->NullSafe) {
      OS << " [nullsafe]";
    }
    break;
  }
  case NodeKind::IsExpr:
    OS << "IsExpr";
    break;
  case NodeKind::AsExpr: {
    auto *E = llvm::cast<const AsExpr>(Node);
    OS << "AsExpr";
    if (E->NullSafe) {
      OS << " [nullsafe]";
    }
    break;
  }
  case NodeKind::ThisExpr:
    OS << "ThisExpr";
    break;
  case NodeKind::CollectionExpr:
    OS << "CollectionExpr";
    break;
  case NodeKind::Label: {
    auto *Lbl = llvm::cast<const Label>(Node);
    OS << "Label '" << Lbl->Name->getName() << "'";
    break;
  }
  case NodeKind::TypeParam: {
    auto *Param = llvm::cast<const TypeParam>(Node);
    OS << "TypeParam '" << Param->Name->getName() << "'";
    break;
  }
  case NodeKind::CatchClause: {
    auto *Clause = llvm::cast<const CatchClause>(Node);
    OS << "CatchClause '" << Clause->Param->Name->getName() << "'";
    break;
  }
  default:
    llvm_unreachable("Unknown AST node kind.");
  }
  OS << '\n';
}

void ASTDumper::collectChildren(const ASTNode *Node,
                                llvm::SmallVectorImpl<Child> &Children) {
  switch (Node->getKind()) {
  case NodeKind::Program:
    // Handled directly by `dump`.
    break;
  case NodeKind::TopLevelObject: {
    auto *Obj = llvm::cast<const TopLevelObject>(Node);
    pushChild(Children, "Decl", Obj->Decl);
    break;
  }
  case NodeKind::FunctionDecl: {
    auto *Fn = llvm::cast<const FunctionDecl>(Node);
    appendChildren(Children, Fn->Parameters);
    pushChild(Children, "ReturnType", Fn->ReturnType);
    pushChild(Children, "Body", Fn->Body);
    break;
  }
  case NodeKind::ClassDecl: {
    auto *Cls = llvm::cast<const ClassDecl>(Node);
    appendChildren(Children, Cls->TypeParams);
    appendChildren(Children, Cls->Members);
    break;
  }
  case NodeKind::VarDecl: {
    auto *Var = llvm::cast<const VarDecl>(Node);
    pushChild(Children, "VarType", Var->VarType);
    pushChild(Children, "Value", Var->Value);
    break;
  }
  case NodeKind::Parameter: {
    auto *Param = llvm::cast<const Parameter>(Node);
    pushChild(Children, "Type", Param->ParamType);
    pushChild(Children, "DefaultValue", Param->DefaultValue);
    break;
  }
  case NodeKind::ArrayType: {
    auto *Ty = llvm::cast<const ArrayType>(Node);
    pushChild(Children, "ElementType", Ty->ElementType);
    pushChild(Children, "Size", Ty->Size);
    break;
  }
  case NodeKind::FunctionType: {
    auto *Ty = llvm::cast<const FunctionType>(Node);
    appendChildren(Children, Ty->Parameters);
    pushChild(Children, "ReturnType", Ty->ReturnType);
    break;
  }
  case NodeKind::Block: {
    auto *Blk = llvm::cast<const Block>(Node);
    appendChildren(Children, Blk->Statements);
    break;
  }
  case NodeKind::DeclStatement: {
    auto *Stmt = llvm::cast<const DeclStatement>(Node);
    pushChild(Children, "Declaration", Stmt->Decl);
    break;
  }
  case NodeKind::ExprStmt: {
    auto *Stmt = llvm::cast<const ExprStmt>(Node);
    pushChild(Children, "Expression", Stmt->Expression);
    break;
  }
  case NodeKind::AssignmentStmt: {
    auto *Stmt = llvm::cast<const AssignmentStmt>(Node);
    pushChild(Children, "LHS", Stmt->LHS);
    pushChild(Children, "RHS", Stmt->RHS);
    break;
  }
  case NodeKind::IfStmt: {
    auto *Stmt = llvm::cast<const IfStmt>(Node);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Then", Stmt->Then);
    pushChild(Children, "Else", Stmt->Else);
    break;
  }
  case NodeKind::ForStmt: {
    auto *Stmt = llvm::cast<const ForStmt>(Node);
    appendChildren(Children, Stmt->InitStmts);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Update", Stmt->Update);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::ForEachStmt: {
    auto *Stmt = llvm::cast<const ForEachStmt>(Node);
    pushChild(Children, "Scope", Stmt->Scope);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::WhileStmt: {
    auto *Stmt = llvm::cast<const WhileStmt>(Node);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::DoWhileStmt: {
    auto *Stmt = llvm::cast<const DoWhileStmt>(Node);
    pushChild(Children, "Body", Stmt->Body);
    pushChild(Children, "Condition", Stmt->Condition);
    break;
  }
  case NodeKind::TryStmt: {
    auto *Stmt = llvm::cast<const TryStmt>(Node);
    pushChild(Children, "Body", Stmt->Body);
    appendChildren(Children, Stmt->CatchClauses);
    pushChild(Children, "Finally", Stmt->Finally);
    break;
  }
  case NodeKind::IfExpr: {
    auto *E = llvm::cast<const IfExpr>(Node);
    pushChild(Children, "Condition", E->Condition);
    pushChild(Children, "Then", E->Then);
    pushChild(Children, "Else", E->Else);
    break;
  }
  case NodeKind::ThrowExpr: {
    auto *E = llvm::cast<const ThrowExpr>(Node);
    pushChild(Children, "Content", E->Content);
    break;
  }
  case NodeKind::ReturnExpr: {
    auto *E = llvm::cast<const ReturnExpr>(Node);
    pushChild(Children, "Value", E->Value);
    break;
  }
  case NodeKind::UnaryExpr: {
    auto *E = llvm::cast<const UnaryExpr>(Node);
    pushChild(Children, "Operand", E->Operand);
    break;
  }
  case NodeKind::BinaryExpr: {
    auto *E = llvm::cast<const BinaryExpr>(Node);
    pushChild(Children, "LHS", E->LHS);
    pushChild(Children, "RHS", E->RHS);
    break;
  }
  case NodeKind::ComparisonChainExpr: {
    auto *E = llvm::cast<const ComparisonChainExpr>(Node);
    appendChildren(Children, E->Operands);
    break;
  }
  case NodeKind::CallExpr: {
    auto *E = llvm::cast<const CallExpr>(Node);
    pushChild(Children, "Callee", E->Callee);
    // Type arguments precede the argument list in source order.
    appendChildren(Children, E->TypeArgs);
    appendChildren(Children, E->Arguments);
    break;
  }
  case NodeKind::IndexExpr: {
    auto *E = llvm::cast<const IndexExpr>(Node);
    pushChild(Children, "Base", E->Base);
    pushChild(Children, "Index", E->Index);
    break;
  }
  case NodeKind::MemberExpr: {
    auto *E = llvm::cast<const MemberExpr>(Node);
    pushChild(Children, "Base", E->Base);
    break;
  }
  case NodeKind::IsExpr: {
    auto *E = llvm::cast<const IsExpr>(Node);
    pushChild(Children, "Operand", E->Operand);
    pushChild(Children, "CheckType", E->CheckType);
    break;
  }
  case NodeKind::AsExpr: {
    auto *E = llvm::cast<const AsExpr>(Node);
    pushChild(Children, "Operand", E->Operand);
    pushChild(Children, "TargetType", E->TargetType);
    break;
  }
  case NodeKind::CollectionExpr: {
    auto *E = llvm::cast<const CollectionExpr>(Node);
    appendChildren(Children, E->Elements);
    break;
  }
  case NodeKind::TypeRef: {
    auto *Ref = llvm::cast<const TypeRef>(Node);
    appendChildren(Children, Ref->TypeArgs);
    break;
  }
  case NodeKind::TypeParam: {
    auto *Param = llvm::cast<const TypeParam>(Node);
    pushChild(Children, "DefaultType", Param->DefaultType);
    break;
  }
  case NodeKind::CatchClause: {
    auto *Clause = llvm::cast<const CatchClause>(Node);
    pushChild(Children, "Param", Clause->Param);
    pushChild(Children, "Body", Clause->Body);
    break;
  }
  // Leaves without children.
  case NodeKind::NullLiteral:
  case NodeKind::BoolLiteral:
  case NodeKind::IntLiteral:
  case NodeKind::FloatLiteral:
  case NodeKind::StringLiteral:
  case NodeKind::CharLiteral:
  case NodeKind::VarExpr:
  case NodeKind::ThisExpr:
  case NodeKind::ContinueExpr:
  case NodeKind::BreakExpr:
  case NodeKind::BuiltinType:
  case NodeKind::Label:
    break;
  default:
    llvm_unreachable("Unknown AST node kind.");
  }
}

void ASTDumper::dumpChildren(const llvm::SmallVectorImpl<Child> &Children,
                             llvm::StringRef Prefix) {
  for (size_t I = 0; I < Children.size(); ++I) {
    const auto &[Label, Node] = Children[I];
    bool IsLast = I + 1 == Children.size();
    if (Label.empty()) {
      dumpNode(Node, Prefix, IsLast);
    } else {
      OS << Prefix << (IsLast ? "`- " : "|- ") << Label << '\n';
      llvm::SmallString<32> SubPrefix(Prefix);
      SubPrefix += IsLast ? "  " : "| ";
      dumpNode(Node, SubPrefix, true);
    }
  }
}

void ASTDumper::dumpNode(const ASTNode *Node, llvm::StringRef Prefix,
                         bool IsLast) {
  OS << Prefix << (IsLast ? "`- " : "|- ");
  dumpHeader(Node);
  llvm::SmallString<32> ChildPrefix(Prefix);
  ChildPrefix += IsLast ? "  " : "| ";

  llvm::SmallVector<Child, 8> Children;
  collectChildren(Node, Children);
  dumpChildren(Children, ChildPrefix);
}

void ASTDumper::dump(const Program *P) {
  assert(P && "Cannot dump a null Program.");
  dumpHeader(P);

  llvm::SmallVector<Child, 8> Children;
  for (auto *Obj : P->Objects) {
    Children.push_back({llvm::StringRef(), Obj});
  }
  dumpChildren(Children, "");
}

void dump(const Program *P, llvm::raw_ostream &OS) {
  ASTDumper Dumper(OS);
  Dumper.dump(P);
}

} // namespace astra::ast
