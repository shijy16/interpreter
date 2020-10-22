//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : public EvaluatedExprVisitor<InterpreterVisitor> {
   public:
    explicit InterpreterVisitor(const ASTContext &context, Environment *env)
        : EvaluatedExprVisitor(context), mEnv(env) {}
    virtual ~InterpreterVisitor() {}

    virtual void VisitWhileStmt(WhileStmt *whilestmt) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        Expr *cond = whilestmt->getCond();
        Visit(cond);
        int res = mEnv->expr(cond);
        while (res == 1) {
            Visit(whilestmt->getBody());
            Visit(cond);
            res = mEnv->expr(cond);
            if (mEnv->isCurFuncReturned()) {
                return;
            }
        }
    }

    virtual void VisitForStmt(ForStmt *forstmt) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }

        Stmt *initstmt = forstmt->getInit();
        if (initstmt) VisitStmt(initstmt);
        Expr *cond = forstmt->getCond();
        Expr *inc = forstmt->getInc();
        Stmt *body = forstmt->getBody();
        if (cond) {
            Visit(cond);
            int res = mEnv->expr(cond);
            while (res == 1) {
                Visit(body);
                Visit(inc);
                Visit(cond);
                res = mEnv->expr(cond);
                if (mEnv->isCurFuncReturned()) {
                    return;
                }
            }
        } else {
            while (true) {
                Visit(body);
                Visit(inc);
                if (mEnv->isCurFuncReturned()) {
                    return;
                }
            }
        }
    }

    virtual void VisitIfStmt(IfStmt *ifstmt) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        Expr *cond = ifstmt->getCond();
        Visit(cond);
        int res = mEnv->expr(cond);
        if (res == 1) {
            Visit(ifstmt->getThen());  // cannot use VisitStmt. do not know why
        } else if (ifstmt->getElse()) {
            Visit(ifstmt->getElse());
        }
    }

    virtual void VisitParenExpr(ParenExpr *pexpr) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        VisitStmt(pexpr);
        mEnv->parenexpr(pexpr);
    }

    virtual void VisitBinaryOperator(BinaryOperator *bop) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //llvm::errs() << "VisitBinaryOperator.\n";
        VisitStmt(bop);
        mEnv->binop(bop);
    }
    virtual void VisitUnaryOperator(UnaryOperator *uop) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        VisitStmt(uop);
        mEnv->unaryop(uop);
    }
    virtual void VisitDeclRefExpr(DeclRefExpr *expr) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //llvm::errs() << "VisitDeclRefExpr.\n";
        VisitStmt(expr);
        mEnv->declref(expr);
    }

    virtual void VisitArraySubscriptExpr(ArraySubscriptExpr *expr) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //printf("Visit Array\n\n");
        Visit(expr->getLHS());
        Visit(expr->getRHS());
        mEnv->arrayref(expr);
    }

    virtual void VisitCastExpr(CastExpr *expr) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //llvm::errs() << "VisitCastExpr.\n";
        VisitStmt(expr);
        mEnv->cast(expr);
    }

    virtual void VisitReturnStmt(ReturnStmt *rets) {
        //llvm::errs() << "VisitRtnStmt.\n";
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        Visit(rets->getRetValue());
        mEnv->retstmt(rets);
    }

    virtual void VisitCallExpr(CallExpr *call) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //llvm::errs() << "VisitCallExpr.\n";
        VisitStmt(call);
        mEnv->call(call);
        FunctionDecl *callee = call->getCalleeDecl()->getAsFunction();
        if (mEnv->isExternalCall(callee)) return;
        if (callee->hasBody()) {
            VisitStmt(callee->getBody());
        }
        // return here
        mEnv->ret(call);
    }

    virtual void VisitDeclStmt(DeclStmt *declstmt) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //llvm::errs() << "VisitDeclStmt.\n";
        VisitStmt(declstmt);
        mEnv->decl(declstmt);
    }

    virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *tte) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        //printf("sizeof expr\n");
        VisitStmt(tte);
        if (tte->getKind() == UETT_SizeOf) {
            mEnv->sizeofexpr(tte);
        }
    }

    virtual void VisitIntegerLiteral(IntegerLiteral *il) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        mEnv->integerLiteral(il);
    }

    virtual void VisitCharacterLiteral(CharacterLiteral *cl) {
        if (mEnv->isCurFuncReturned()) {
            return;
        }
        mEnv->characterLiteral(cl);
    }

   private:
    Environment *mEnv;
};

class InterpreterConsumer : public ASTConsumer {
   public:
    explicit InterpreterConsumer(const ASTContext &context)
        : mEnv(), mVisitor(context, &mEnv) {}
    virtual ~InterpreterConsumer() {}

    virtual void HandleTranslationUnit(clang::ASTContext &Context) {
        TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
        mEnv.init(decl);

        FunctionDecl *entry = mEnv.getEntry();
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

int main(int argc, char **argv) {
    if (argc > 1) {
        clang::tooling::runToolOnCode(
            std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction),
            argv[1]);
    }
}
