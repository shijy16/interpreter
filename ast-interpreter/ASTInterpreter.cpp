//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {}
   virtual ~InterpreterVisitor() {}


   //visit cont int literal
   //virtual void VisitIntegerLiteral(IntegerLiteral* literal){
   //     llvm::errs()<<"VisitIntegerLiteral.\n";
   //         mEnv->integerLiteral(literal);
   //}

   virtual void VisitBinaryOperator (BinaryOperator * bop) {
       llvm::errs() << "VisitBinaryOperator.\n";
       VisitStmt(bop);
       mEnv->binop(bop);
   }
   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
       llvm::errs() << "VisitDeclRefExpr.\n";
       VisitStmt(expr);
       mEnv->declref(expr);
   }
   virtual void VisitCastExpr(CastExpr * expr) {
       llvm::errs() << "VisitCastExpr.\n";
       VisitStmt(expr);
       mEnv->cast(expr);
   }
   virtual void VisitCallExpr(CallExpr * call) {
       llvm::errs() << "VisitCallExpr.\n";
       VisitStmt(call);
       mEnv->call(call);
   }
   virtual void VisitDeclStmt(DeclStmt * declstmt) {
       llvm::errs() << "VisitDeclStmt.\n";
       mEnv->decl(declstmt);
   }
private:
   Environment * mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
          mVisitor(context, &mEnv) {
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context) {
       TranslationUnitDecl * decl = Context.getTranslationUnitDecl();
       mEnv.init(decl);

       FunctionDecl * entry = mEnv.getEntry();
       mVisitor.VisitStmt(entry->getBody());
  }
private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public: 
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

int main (int argc, char ** argv) {
   if (argc > 1) {
       clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), argv[1]);
   }
}
