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


   virtual void VisitIfStmt(IfStmt* ifstmt){
        if(mEnv->isCurFuncReturned()){return;}
        Expr* cond = ifstmt->getCond();
        int res = mEnv->expr(cond);
        if(res == 1){
            Visit(ifstmt->getThen());   //cannot use VisitStmt. donot know why
        }else{
            Visit(ifstmt->getElse());
        }
   }

   virtual void VisitParenExpr (ParenExpr* pexpr){
        if(mEnv->isCurFuncReturned()){return;}
        VisitStmt(pexpr);
        mEnv->parenexpr(pexpr);
   }

   virtual void VisitBinaryOperator (BinaryOperator * bop) {
        if(mEnv->isCurFuncReturned()){return;}
       llvm::errs() << "VisitBinaryOperator.\n";
       VisitStmt(bop);
       mEnv->binop(bop);
   }
   virtual void VisitUnaryOperator (UnaryOperator* uop){
        if(mEnv->isCurFuncReturned()){return;}
        VisitStmt(uop);
        mEnv->unaryop(uop);
   }
   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
        if(mEnv->isCurFuncReturned()){return;}
        llvm::errs() << "VisitDeclRefExpr.\n";
       VisitStmt(expr);
       mEnv->declref(expr);
   }
   virtual void VisitCastExpr(CastExpr * expr) {
        if(mEnv->isCurFuncReturned()){return;}
       llvm::errs() << "VisitCastExpr.\n";
       VisitStmt(expr);
       mEnv->cast(expr);
   }
   virtual void VisitReturnStmt(ReturnStmt* rets){
       llvm::errs() << "VisitRtnStmt.\n";
        if(mEnv->isCurFuncReturned()){return;}
        Visit(rets->getRetValue());
        mEnv->retstmt(rets);
   }
   
   virtual void VisitCallExpr(CallExpr * call) {
        if(mEnv->isCurFuncReturned()){return;}
       llvm::errs() << "VisitCallExpr.\n";
       VisitStmt(call);
       mEnv->call(call);
       FunctionDecl* callee = call->getCalleeDecl()->getAsFunction();
       if(mEnv->isExternalCall(callee))
           return;
       if(callee->hasBody()){
           VisitStmt(callee->getBody());
       }
       //default return here
       mEnv->ret(call);
   }

   virtual void VisitDeclStmt(DeclStmt * declstmt) {
        if(mEnv->isCurFuncReturned()){return;}
       llvm::errs() << "VisitDeclStmt.\n";
       VisitStmt(declstmt);
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
