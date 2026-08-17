#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/Program.h"
#include "astra/basic/DiagnosticsEngine.h"

namespace astra::sema {
/// Run the first semantic-analysis phase on an already parsed program:
/// declaration collection, scope and name resolution, and canonical type
/// resolution. Results are attached to the AST nodes as fields
/// (`ast::Type::ResolvedType`, `VarExpr::Decl`, `ThisExpr::EnclosingClass`).
///
/// New diagnostics are reported to `Diags`; the program structure is left
/// unchanged and `nullptr` is never returned, so callers must check
/// `Diags.hasErrors()` afterwards. `P` must be a non-null program produced by
/// a successful `frontend::parse` (a null program is silently ignored).
void analyze(ast::ASTContext &Ctx, ast::Program *P,
             basic::DiagnosticsEngine &Diags);
} // namespace astra::sema
