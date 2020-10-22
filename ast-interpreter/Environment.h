//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
//===----------------------------------------------------------------------===//
#include <stdio.h>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

class StackFrame {
    /// StackFrame maps Variable Declaration to Value
    /// Which are either integer or addresses (also represented using an Integer
    /// value)
    std::map<Decl *, long> mVars;
    std::map<Stmt *, long> mExprs;
    /// The current stmt
    Stmt *mPC;
    long retValue = 0;
    bool returned = false;

   public:
    StackFrame() : mVars(), mExprs(), mPC() {}
    bool findDecl(Decl *decl) { return mVars.find(decl) != mVars.end(); }

    void bindDecl(Decl *decl, long val) {
        // llvm::errs() << "binddecl:"<<val<<"\n";
        mVars[decl] = val;
    }

    long getDeclVal(Decl *decl) {
        assert(mVars.find(decl) != mVars.end());
        return mVars.find(decl)->second;
    }
    void bindStmt(Stmt *stmt, long val) {
        // llvm::errs() << "bindStmt "<<val<<"\n";
        mExprs[stmt] = val;
    }
    long getStmtVal(Stmt *stmt) {
        // llvm::errs() << "getStmtVal:"<<stmt<<"\n";
        assert(mExprs.find(stmt) != mExprs.end());
        return mExprs[stmt];
    }
    void setPC(Stmt *stmt) { mPC = stmt; }
    Stmt *getPC() { return mPC; }

    long getRetValue() { return retValue; }
    void setRetValue(long v) { retValue = v; }
    void setReturned() { returned = true; }
    bool isReturned() { return returned; }
};

/// Heap maps address to a value
class Heap {
    std::map<long *, int> block;

   public:
    long *Malloc(int size) {
        long *t = (long *)malloc(size);
        //printf("malloc %d at 0x%p.\n", size, t);
        block[t] = size;
        return t;
    }
    void Free(long *addr) {
        if (block.find(addr) == block.end()) {
            printf("Error:Free invalid address:0x%p\n", addr);
        }
        free(addr);
        //printf("free 0x%p.\n", addr);
    }
    void Update(long *addr, long val) {
        bool valid = check(addr);
        if (valid) {
            *addr = val;
            //printf("Update 0x%p to %ld.\n", addr, val);
        } else
            printf("Error:Update invalid address:0x%p\n", addr);
    }
    long Get(long *addr) {
        bool valid = check(addr);
        if (valid) {
            //printf("GET:0x%p,value:%ld.\n", addr, *addr);
            return *addr;
        } else {
            printf("Error:Get value of invalid address:0x%p\n", addr);
            return -1;
        }
    }
    bool check(long *addr) {
        bool valid = false;
        for (std::map<long *, int>::iterator iter = block.begin();
             iter != block.end(); iter++) {
            if ((long)(addr) >= (long)(iter->first) &&
                (long)addr <= (long)(iter->first) + (long)(iter->second)) {
                valid = true;
                break;
            }
        }
        return valid;
    }
};

class Environment {
    std::vector<StackFrame> mStack;

    FunctionDecl *mFree;  /// Declartions to the built-in functions
    FunctionDecl *mMalloc;
    FunctionDecl *mInput;
    FunctionDecl *mOutput;

    FunctionDecl *mEntry;

    Heap *mHeap;

   public:
    /// Get the declartions to the built-in functions
    Environment()
        : mStack(),
          mFree(NULL),
          mMalloc(NULL),
          mInput(NULL),
          mOutput(NULL),
          mEntry(NULL) {}

    /// Initialize the Environment
    void init(TranslationUnitDecl *unit) {
        // global stackframe
        mHeap = new Heap();
        mStack.push_back(StackFrame());
        for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(),
                                                e = unit->decls_end();
             i != e; ++i) {
            // global values
            if (VarDecl *vdecl = dyn_cast<VarDecl>(*i)) {
                vardecl(vdecl, &(mStack.back()));
            }
            if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*i)) {
                if (fdecl->getName().equals("FREE"))
                    mFree = fdecl;
                else if (fdecl->getName().equals("MALLOC"))
                    mMalloc = fdecl;
                else if (fdecl->getName().equals("GET"))
                    mInput = fdecl;
                else if (fdecl->getName().equals("PRINT"))
                    mOutput = fdecl;
                else if (fdecl->getName().equals("main"))
                    mEntry = fdecl;
            }
        }
        mStack.push_back(StackFrame());
    }

    bool isExternalCall(FunctionDecl *f) {
        return f == mFree || f == mMalloc || f == mInput || f == mOutput;
    }

    bool isCurFuncReturned() { return mStack.back().isReturned(); }

    FunctionDecl *getEntry() { return mEntry; }

    // bind int literal stmt and value
    void integerLiteral(IntegerLiteral *literal) {
        long value = (long)literal->getValue().getLimitedValue();
        mStack.back().bindStmt(literal, value);
    }

    void characterLiteral(CharacterLiteral *cl) {
        long value = (long)cl->getValue();
        mStack.back().bindStmt(cl, value);
    }

    long expr(Expr *exp) {
        Expr *e = exp->IgnoreImpCasts();
        if (BinaryOperator *bop = dyn_cast<BinaryOperator>(e)) {
            binop(bop);
            return mStack.back().getStmtVal(bop);
        } else if (IntegerLiteral *integerLiteral =
                       dyn_cast<IntegerLiteral>(e)) {
            long value = (long)integerLiteral->getValue().getSExtValue();
            // long value = mStack.back().getStmtVal(integerLiteral);
            return value;
        } else if (CharacterLiteral *cl = dyn_cast<CharacterLiteral>(e)) {
            long value = cl->getValue();
            return value;
        } else if (DeclRefExpr *dref = dyn_cast<DeclRefExpr>(e)) {
            declref(
                dref);  // have to do this. Global declref havn't been visited
            long value = mStack.back().getStmtVal(dref);
            return value;
        } else if (CallExpr *cexpr = dyn_cast<CallExpr>(e)) {
            long value = mStack.back().getStmtVal(cexpr);
            return value;
        } else if (UnaryOperator *uo = dyn_cast<UnaryOperator>(e)) {
            long value = mStack.back().getStmtVal(uo);
            return value;
        } else if (ParenExpr *pe = dyn_cast<ParenExpr>(e)) {
            long value = mStack.back().getStmtVal(pe);
            return value;
        } else if (ArraySubscriptExpr *ae = dyn_cast<ArraySubscriptExpr>(e)) {
            long value = mStack.back().getStmtVal(ae);
            return value;
        } else if (UnaryExprOrTypeTraitExpr *tte =
                       dyn_cast<UnaryExprOrTypeTraitExpr>(e)) {
            long value = mStack.back().getStmtVal(tte);
            return value;
        } else if (CStyleCastExpr *cce = dyn_cast<CStyleCastExpr>(e)) {
            // long addr = expr(cce->getSubExpr());
            // long* addr = (long*) t;
            long value = expr(cce->getSubExpr());
            return value;
        } else {
            printf("Expr not hanled.\n");
            return -1;
        }
    }

    void parenexpr(ParenExpr *pe) {
        Expr *e = pe->getSubExpr();
        long value = expr(e);
        mStack.back().bindStmt(pe, value);
    }

    void sizeofexpr(UnaryExprOrTypeTraitExpr *tte) {
        long value = sizeof(long);
        mStack.back().bindStmt(tte, value);
    }

    void unaryop(UnaryOperator *uop) {
        Expr *e = uop->getSubExpr();
        long value = expr(e);
        if (uop->getOpcode() == UO_Plus) {
            mStack.back().bindStmt(uop, value);
        } else if (uop->getOpcode() == UO_Minus) {
            mStack.back().bindStmt(uop, -value);
        } else if (uop->getOpcode() == UO_Deref) {
            mStack.back().bindStmt(uop, mHeap->Get((long *)value));
        }
    }

    /// !TODO Support comparison operation
    void binop(BinaryOperator *bop) {
        Expr *left = bop->getLHS();
        Expr *right = bop->getRHS();

        //    llvm::errs() << "binop.\n";
        if (bop->isAssignmentOp()) {  // =
            long val = expr(right);
            if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(left)) {
                mStack.back().bindStmt(left, val);
                Decl *decl = declexpr->getFoundDecl();
                if (mStack.back().findDecl(decl))
                    mStack.back().bindDecl(decl, val);
                else
                    mStack.front().bindDecl(decl, val);
            } else if (ArraySubscriptExpr *aexpr =
                           dyn_cast<ArraySubscriptExpr>(left)) {
                long index = expr(aexpr->getIdx());
                DeclRefExpr *declref =
                    dyn_cast<DeclRefExpr>(aexpr->getLHS()->IgnoreImpCasts());
                if (!declref) printf("ERROR: Array reference not known.\n");
                Decl *decl = declref->getFoundDecl();
                long temp = mStack.back().findDecl(decl)
                                ? mStack.back().getDeclVal(decl)
                                : mStack.front().getDeclVal(decl);
                long *arr = (long *)temp;
                arr[index] = val;
            } else if (UnaryOperator *uope = dyn_cast<UnaryOperator>(left)) {
                long lval = expr(uope->getSubExpr());
                long *addr = (long *)lval;
                mHeap->Update(addr, val);
            } else {
                printf("shouldn't be here\n");
            }
        } else if (bop->isAdditiveOp()) {  // + -
            long valr = expr(right);
            long vall = expr(left);
            long res = 0;
            if (left->getType().getTypePtr()->isPointerType() &&
                !right->getType().getTypePtr()->isPointerType()) {
                valr *= sizeof(long);
            } else if (!right->getType().getTypePtr()->isPointerType() &&
                       left->getType().getTypePtr()->isPointerType()) {
                vall *= sizeof(long);
            }
            if (bop->getOpcode() == BO_Add) {
                res = vall + valr;
            } else {
                res = vall - valr;
            }
            mStack.back().bindStmt(bop, res);
        } else if (bop->isMultiplicativeOp()) {  // * /
            long valr = expr(right);
            long vall = expr(left);
            long res = 0;
            if (bop->getOpcode() == BO_Mul) {
                res = vall * valr;
            } else {
                res = vall / valr;
            }
            mStack.back().bindStmt(bop, res);
        } else if (bop->isComparisonOp()) {  // > < >= <= == !=
            long valr = expr(right);
            long vall = expr(left);
            long res = 0;
            switch (bop->getOpcode()) {
                case BO_GT:
                    res = (vall > valr);
                    break;
                case BO_LT:
                    res = (vall < valr);
                    break;
                case BO_EQ:
                    res = (vall == valr);
                    break;
                case BO_GE:
                    res = (vall >= valr);
                    break;
                case BO_LE:
                    res = (vall <= valr);
                    break;
                case BO_NE:
                    res = (vall != valr);
                    break;
                default:
                    llvm::errs() << "Comparison Op not Identified.\n";
                    break;
            }
            mStack.back().bindStmt(bop, res);
        }
    }

    // handle var delarations.
    void vardecl(VarDecl *vdecl, StackFrame *sf) {
        if (vdecl->getType().getTypePtr()->isIntegerType() ||
            vdecl->getType().getTypePtr()->isCharType()) {
            long value = 0;
            if (vdecl->hasInit()) {
                Expr *e = vdecl->getInit();
                value = expr(e);
            }
            sf->bindDecl(vdecl, value);
        } else if (vdecl->getType().getTypePtr()->isArrayType()) {
            const ConstantArrayType *atype =
                dyn_cast<ConstantArrayType>(vdecl->getType().getTypePtr());
            int asize = atype->getSize().getSExtValue();
            if (asize <= 0) {
                llvm::errs() << "Error: Invalid Array Size " << asize << ".\n";
            }
            if (atype->getElementType().getTypePtr()->isIntegerType()) {
                long *temp = new long[asize];
                for (int i = 0; i < asize; i++) temp[i] = 0;
                sf->bindDecl(vdecl, (long)temp);
            } else if (atype->getElementType().getTypePtr()->isPointerType()) {
                long **temp = new long *[asize];
                for (int i = 0; i < asize; i++) temp[i] = 0;
                sf->bindDecl(vdecl, (long)temp);
            }
        } else if (vdecl->getType().getTypePtr()->isPointerType()) {
            long value = 0;
            if (vdecl->hasInit()) {
                Expr *e = vdecl->getInit();
                value = expr(e);
            }
            sf->bindDecl(vdecl, value);
        } else {
            sf->bindDecl(vdecl, 0);
        }
    }

    void decl(DeclStmt *declstmt) {
        for (DeclStmt::decl_iterator it = declstmt->decl_begin(),
                                     ie = declstmt->decl_end();
             it != ie; ++it) {
            Decl *decl = *it;
            if (VarDecl *vdecl = dyn_cast<VarDecl>(decl)) {
                vardecl(vdecl, &(mStack.back()));
            }
        }
    }
    void declref(DeclRefExpr *declref) {
        mStack.back().setPC(declref);
        if (declref->getType()->isIntegerType() ||
            declref->getType()->isPointerType()) {
            Decl *decl = declref->getFoundDecl();
            // global or local value
            long val = mStack.back().findDecl(decl)
                           ? mStack.back().getDeclVal(decl)
                           : mStack.front().getDeclVal(decl);
            mStack.back().bindStmt(declref, val);
        } else {
            //    printf("wtf!\n");
        }
    }

    void arrayref(ArraySubscriptExpr *aexpr) {
        long index = expr(aexpr->getIdx());
        DeclRefExpr *declref =
            dyn_cast<DeclRefExpr>(aexpr->getLHS()->IgnoreImpCasts());
        if (!declref) printf("ERROR: Array reference not known.\n");
        Decl *decl = declref->getFoundDecl();
        long temp = mStack.back().findDecl(decl)
                        ? mStack.back().getDeclVal(decl)
                        : mStack.front().getDeclVal(decl);
        long *arr = (long *)temp;
        mStack.back().bindStmt(aexpr, arr[index]);
    }

    void cast(CastExpr *castexpr) {
        mStack.back().setPC(castexpr);
        if (castexpr->getType()->isIntegerType()) {
            Expr *expr = castexpr->getSubExpr();
            long val = mStack.back().getStmtVal(expr);
            mStack.back().bindStmt(castexpr, val);
        } else if (castexpr->getType()->isPointerType()) {
            // printf("Pointer\n");
        }
    }

    void ret(CallExpr *callexpr) {
        FunctionDecl *callee = callexpr->getDirectCallee();
        if (callee->isNoReturn()) {
            mStack.pop_back();
        } else {
            long rval = mStack.back().getRetValue();
            mStack.pop_back();
            mStack.back().bindStmt(callexpr, rval);
        }
        //printf("\tret\n");
    }

    void retstmt(ReturnStmt *rstmt) {
        Expr *rexpr = rstmt->getRetValue();
        if (rexpr) {
            long rval = expr(rexpr);
            mStack.back().setRetValue(rval);
        }
        mStack.back().setReturned();
        //printf("\tretstmt\n");
    }

    /// !TODO Support Function Call
    void call(CallExpr *callexpr) {
        //printf("\tcall\n");
        mStack.back().setPC(callexpr);
        long val = 0;
        FunctionDecl *callee = callexpr->getDirectCallee();
        if (callee == mInput) {
            llvm::errs() << "Please Input an Integer Value : ";
            scanf("%ld", &val);

            mStack.back().bindStmt(callexpr, val);
        } else if (callee == mOutput) {
            Expr *e = callexpr->getArg(0);
            val = expr(e);
            llvm::errs() << val << "\n";
        } else if (callee == mMalloc) {
            Expr *e = callexpr->getArg(0);
            val = expr(e);
            long *addr = mHeap->Malloc(val);
            mStack.back().bindStmt(callexpr, (long)addr);
        } else if (callee == mFree) {
            Expr *e = callexpr->getArg(0);
            val = expr(e);
            long *addr = (long *)val;
            mHeap->Free(addr);
        }

        else {
            /// You could add your code here for Function call Return
            StackFrame calleeStack = StackFrame();
            unsigned param_num = callee->getNumParams();
            for (unsigned i = 0; i < param_num; i++) {
                Expr *e = callexpr->getArg(i);
                long val = expr(e);
                VarDecl *vd = dyn_cast<VarDecl>(callee->getParamDecl(i));
                vardecl(vd, &calleeStack);
                calleeStack.bindDecl(vd, val);
            }
            mStack.push_back(calleeStack);
        }
    }
};
