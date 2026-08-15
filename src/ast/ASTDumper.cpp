#include "astra/ast/ASTDumper.h"

#include "astra/ast/Program.h"
#include <llvm/ADT/SmallString.h>

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
  assert(false && "Unknown ast::Op.");
  return llvm::StringRef();
}

llvm::StringRef ASTDumper::getBuiltinTypeName(BuiltinType::Ty Type) {
  switch (Type) {
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
  }
  assert(false && "Unknown BuiltinType.");
  return llvm::StringRef();
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
    auto *Fn = static_cast<const FunctionDecl *>(Node);
    OS << "FunctionDecl '" << Fn->Name->getName() << "'";
    break;
  }
  case NodeKind::VarDecl: {
    auto *Var = static_cast<const VarDecl *>(Node);
    OS << "VarDecl '" << Var->Name->getName() << "'";
    if (Var->IsMutable) {
      OS << " [mutable]";
    }
    break;
  }
  case NodeKind::Parameter: {
    auto *Param = static_cast<const Parameter *>(Node);
    OS << "Parameter '" << Param->Name->getName() << "'";
    break;
  }
  case NodeKind::TypeRef: {
    auto *Ref = static_cast<const TypeRef *>(Node);
    OS << "TypeRef '" << Ref->Name->getName() << "'";
    break;
  }
  case NodeKind::BuiltinType: {
    auto *Ty = static_cast<const BuiltinType *>(Node);
    OS << "BuiltinType '" << getBuiltinTypeName(Ty->Type) << "'";
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
    auto *Stmt = static_cast<const AssignmentStmt *>(Node);
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
    auto *Stmt = static_cast<const ForEachStmt *>(Node);
    OS << "ForEachStmt '" << Stmt->VarName << "'";
    break;
  }
  case NodeKind::WhileStmt:
    OS << "WhileStmt";
    break;
  case NodeKind::DoWhileStmt:
    OS << "DoWhileStmt";
    break;
  case NodeKind::NullLiteral:
    OS << "NullLiteral";
    break;
  case NodeKind::BoolLiteral: {
    auto *Lit = static_cast<const BoolLiteral *>(Node);
    OS << "BoolLiteral '" << (Lit->Value ? "true" : "false") << "'";
    break;
  }
  case NodeKind::IntLiteral: {
    auto *Lit = static_cast<const IntLiteral *>(Node);
    OS << "IntLiteral '";
    llvm::SmallString<32> Buf;
    Lit->Value.toString(Buf, 10);
    OS << Buf << "'";
    break;
  }
  case NodeKind::FloatLiteral: {
    auto *Lit = static_cast<const FloatLiteral *>(Node);
    OS << "FloatLiteral '";
    llvm::SmallString<64> Buf;
    Lit->Value.toString(Buf);
    OS << Buf << "'";
    break;
  }
  case NodeKind::VarExpr: {
    auto *E = static_cast<const VarExpr *>(Node);
    OS << "VarExpr '" << E->Name << "'";
    break;
  }
  case NodeKind::UnaryExpr: {
    auto *E = static_cast<const UnaryExpr *>(Node);
    OS << "UnaryExpr '" << getOpSymbol(E->Operator) << "'";
    break;
  }
  case NodeKind::BinaryExpr: {
    auto *E = static_cast<const BinaryExpr *>(Node);
    OS << "BinaryExpr '" << getOpSymbol(E->Operator) << "'";
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
    auto *E = static_cast<const ContinueExpr *>(Node);
    OS << "ContinueExpr";
    if (E->Pos) {
      OS << " '" << E->Pos->Name->getName() << "'";
    }
    break;
  }
  case NodeKind::BreakExpr: {
    auto *E = static_cast<const BreakExpr *>(Node);
    OS << "BreakExpr";
    if (E->Pos) {
      OS << " '" << E->Pos->Name->getName() << "'";
    }
    break;
  }
  case NodeKind::CallExpr:
    OS << "CallExpr";
    break;
  case NodeKind::IndexExpr:
    OS << "IndexExpr";
    break;
  case NodeKind::MemberExpr: {
    auto *E = static_cast<const MemberExpr *>(Node);
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
    auto *E = static_cast<const AsExpr *>(Node);
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
    auto *Lbl = static_cast<const Label *>(Node);
    OS << "Label '" << Lbl->Name->getName() << "'";
    break;
  }
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
    auto *Obj = static_cast<const TopLevelObject *>(Node);
    pushChild(Children, "Decl", Obj->Decl);
    break;
  }
  case NodeKind::FunctionDecl: {
    auto *Fn = static_cast<const FunctionDecl *>(Node);
    appendChildren(Children, Fn->Parameters);
    pushChild(Children, "ReturnType", Fn->ReturnType);
    pushChild(Children, "Body", Fn->Body);
    break;
  }
  case NodeKind::VarDecl: {
    auto *Var = static_cast<const VarDecl *>(Node);
    pushChild(Children, "VarType", Var->VarType);
    pushChild(Children, "Value", Var->Value);
    break;
  }
  case NodeKind::Parameter: {
    auto *Param = static_cast<const Parameter *>(Node);
    pushChild(Children, "Type", Param->Type);
    pushChild(Children, "DefaultValue", Param->DefaultValue);
    break;
  }
  case NodeKind::ArrayType: {
    auto *Ty = static_cast<const ArrayType *>(Node);
    pushChild(Children, "ElementType", Ty->ElementType);
    pushChild(Children, "Size", Ty->Size);
    break;
  }
  case NodeKind::FunctionType: {
    auto *Ty = static_cast<const FunctionType *>(Node);
    appendChildren(Children, Ty->Parameters);
    pushChild(Children, "ReturnType", Ty->ReturnType);
    break;
  }
  case NodeKind::Block: {
    auto *Blk = static_cast<const Block *>(Node);
    appendChildren(Children, Blk->Statements);
    break;
  }
  case NodeKind::DeclStatement: {
    auto *Stmt = static_cast<const DeclStatement *>(Node);
    pushChild(Children, "Declaration", Stmt->Declaration);
    break;
  }
  case NodeKind::ExprStmt: {
    auto *Stmt = static_cast<const ExprStmt *>(Node);
    pushChild(Children, "Expression", Stmt->Expression);
    break;
  }
  case NodeKind::AssignmentStmt: {
    auto *Stmt = static_cast<const AssignmentStmt *>(Node);
    pushChild(Children, "LHS", Stmt->LHS);
    pushChild(Children, "RHS", Stmt->RHS);
    break;
  }
  case NodeKind::IfStmt: {
    auto *Stmt = static_cast<const IfStmt *>(Node);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Then", Stmt->Then);
    pushChild(Children, "Else", Stmt->Else);
    break;
  }
  case NodeKind::ForStmt: {
    auto *Stmt = static_cast<const ForStmt *>(Node);
    appendChildren(Children, Stmt->InitStmts);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Update", Stmt->Update);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::ForEachStmt: {
    auto *Stmt = static_cast<const ForEachStmt *>(Node);
    pushChild(Children, "Scope", Stmt->Scope);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::WhileStmt: {
    auto *Stmt = static_cast<const WhileStmt *>(Node);
    pushChild(Children, "Condition", Stmt->Condition);
    pushChild(Children, "Body", Stmt->Body);
    break;
  }
  case NodeKind::DoWhileStmt: {
    auto *Stmt = static_cast<const DoWhileStmt *>(Node);
    pushChild(Children, "Body", Stmt->Body);
    pushChild(Children, "Condition", Stmt->Condition);
    break;
  }
  case NodeKind::IfExpr: {
    auto *E = static_cast<const IfExpr *>(Node);
    pushChild(Children, "Condition", E->Condition);
    pushChild(Children, "Then", E->Then);
    pushChild(Children, "Else", E->Else);
    break;
  }
  case NodeKind::ThrowExpr: {
    auto *E = static_cast<const ThrowExpr *>(Node);
    pushChild(Children, "Content", E->Content);
    break;
  }
  case NodeKind::ReturnExpr: {
    auto *E = static_cast<const ReturnExpr *>(Node);
    pushChild(Children, "Value", E->Value);
    break;
  }
  case NodeKind::UnaryExpr: {
    auto *E = static_cast<const UnaryExpr *>(Node);
    pushChild(Children, "Operand", E->Operand);
    break;
  }
  case NodeKind::BinaryExpr: {
    auto *E = static_cast<const BinaryExpr *>(Node);
    pushChild(Children, "LHS", E->LHS);
    pushChild(Children, "RHS", E->RHS);
    break;
  }
  case NodeKind::CallExpr: {
    auto *E = static_cast<const CallExpr *>(Node);
    pushChild(Children, "Callee", E->Callee);
    appendChildren(Children, E->Arguments);
    break;
  }
  case NodeKind::IndexExpr: {
    auto *E = static_cast<const IndexExpr *>(Node);
    pushChild(Children, "Base", E->Base);
    pushChild(Children, "Index", E->Index);
    break;
  }
  case NodeKind::MemberExpr: {
    auto *E = static_cast<const MemberExpr *>(Node);
    pushChild(Children, "Base", E->Base);
    break;
  }
  case NodeKind::IsExpr: {
    auto *E = static_cast<const IsExpr *>(Node);
    pushChild(Children, "Operand", E->Operand);
    pushChild(Children, "CheckType", E->CheckType);
    break;
  }
  case NodeKind::AsExpr: {
    auto *E = static_cast<const AsExpr *>(Node);
    pushChild(Children, "Operand", E->Operand);
    pushChild(Children, "TargetType", E->TargetType);
    break;
  }
  case NodeKind::CollectionExpr: {
    auto *E = static_cast<const CollectionExpr *>(Node);
    appendChildren(Children, E->Elements);
    break;
  }
  // Leaves without children.
  case NodeKind::NullLiteral:
  case NodeKind::BoolLiteral:
  case NodeKind::IntLiteral:
  case NodeKind::FloatLiteral:
  case NodeKind::VarExpr:
  case NodeKind::ThisExpr:
  case NodeKind::ContinueExpr:
  case NodeKind::BreakExpr:
  case NodeKind::TypeRef:
  case NodeKind::BuiltinType:
  case NodeKind::Label:
    break;
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

void ASTDumper::dump(const Program *Program) {
  assert(Program && "Cannot dump a null Program.");
  dumpHeader(Program);

  llvm::SmallVector<Child, 8> Children;
  for (auto *Obj : Program->Objects) {
    Children.push_back({llvm::StringRef(), Obj});
  }
  dumpChildren(Children, "");
}

void dump(const Program *Program, llvm::raw_ostream &OS) {
  ASTDumper Dumper(OS);
  Dumper.dump(Program);
}

} // namespace astra::ast
