#include "astra/sema/Analyze.h"
#include "astra/sema/SemaContext.h"

namespace astra::sema {

void analyze(ast::ASTContext &Ctx, ast::Program *P,
             basic::DiagnosticsEngine &Diags) {
  if (!P)
    return;
  SemaContext Context(Ctx, Diags);
  Context.run(P);
}

} // namespace astra::sema
