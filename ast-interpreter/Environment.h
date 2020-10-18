//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
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
   /// Which are either integer or addresses (also represented using an Integer value)
   std::map<Decl*, int> mVars;
   std::map<Stmt*, int> mExprs;
   /// The current stmt
   Stmt * mPC;
   int retValue = 0;
   bool returned = false;
public:
   StackFrame() : mVars(), mExprs(), mPC() {
   }
   bool findDecl(Decl* decl){
       return mVars.find(decl) != mVars.end();
   }

   void bindDecl(Decl* decl, int val) {
      //llvm::errs() << "binddecl:"<<val<<"\n";
      mVars[decl] = val;
   } 

   int getDeclVal(Decl * decl) {
      assert (mVars.find(decl) != mVars.end());
      return mVars.find(decl)->second;
   }
   void bindStmt(Stmt * stmt, int val) {
       //llvm::errs() << "bindStmt "<<val<<"\n";
       mExprs[stmt] = val;
   }
   int getStmtVal(Stmt * stmt) {
       //llvm::errs() << "getStmtVal:"<<stmt<<"\n";
       assert (mExprs.find(stmt) != mExprs.end());
       return mExprs[stmt];
   }
   void setPC(Stmt * stmt) {
       mPC = stmt;
   }
   Stmt * getPC() {
       return mPC;
   }

    int getRetValue(){
        return retValue;
    }
    void setRetValue(int v){
        retValue = v;
    }
    void setReturned(){
        returned = true;
    }
    bool isReturned(){
        return returned;
    }
};

/// Heap maps address to a value
/*
class Heap {
public:
   int Malloc(int size) ;
   void Free (int addr) ;
   void Update(int addr, int val) ;
   int get(int addr);
};
*/

class Environment {
   std::vector<StackFrame> mStack;

   FunctionDecl * mFree;                /// Declartions to the built-in functions
   FunctionDecl * mMalloc;
   FunctionDecl * mInput;
   FunctionDecl * mOutput;

   FunctionDecl * mEntry;
public:
   /// Get the declartions to the built-in functions
   Environment(): mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   }


   /// Initialize the Environment
   void init(TranslationUnitDecl * unit) {
       //global stackframe
       mStack.push_back(StackFrame());
       for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
           //global values
           if (VarDecl * vdecl = dyn_cast<VarDecl>(*i)){
               vardecl(vdecl,&(mStack.back()));
            }
           if (FunctionDecl * fdecl = dyn_cast<FunctionDecl>(*i) ) {
               if (fdecl->getName().equals("FREE")) mFree = fdecl;
               else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
               else if (fdecl->getName().equals("GET")) mInput = fdecl;
               else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
               else if (fdecl->getName().equals("main")) mEntry = fdecl;
           }
       }
       mStack.push_back(StackFrame());
   }

   bool isExternalCall(FunctionDecl* f){
        return f == mFree || f == mMalloc || f == mInput || f == mOutput;
   }

   bool isCurFuncReturned(){
        return mStack.back().isReturned();
   }

   FunctionDecl * getEntry() {
       return mEntry;
   }

    //bind int literal stmt and value
    //void integerLiteral(IntegerLiteral* literal){
    //    Expr* literalExpr = dyn_cast<Expr>(literal);
    //    int value = (int)literal->getValue().getLimitedValue();
    //    mStack.back().bindStmt(literalExpr,value);
    //}
    
   int expr(Expr* e){
       e = e->IgnoreImpCasts();//magic
        if(BinaryOperator* bop = dyn_cast<BinaryOperator>(e)){
            binop(bop);
            return mStack.back().getStmtVal(bop);
        } else if (IntegerLiteral* integerLiteral = dyn_cast<IntegerLiteral>(e)){
            int value  = (int)integerLiteral->getValue().getSExtValue();
            return value;
        } else if (DeclRefExpr* dref = dyn_cast<DeclRefExpr>(e)){
            declref(dref);
            int value = mStack.back().getStmtVal(dref);
            return value;
        } else if (CallExpr* cexpr = dyn_cast<CallExpr>(e)){
            int value = mStack.back().getStmtVal(cexpr);
            return value;
        } else if (UnaryOperator* uo = dyn_cast<UnaryOperator>(e)){
            int value = mStack.back().getStmtVal(uo);
            return value;
        } else if (ParenExpr* pe = dyn_cast<ParenExpr>(e)){
            int value = mStack.back().getStmtVal(pe);
            return value;
        }
        else {
            return -1;
        }
   }

   void parenexpr(ParenExpr* pe){
        Expr* e = pe->getSubExpr();
        int value = expr(e);
        mStack.back().bindStmt(pe,value);
   }

   void unaryop(UnaryOperator* uop){
        Expr* e = uop->getSubExpr();
        int value = expr(e);
        mStack.back().bindStmt(uop, -value);
   }


   /// !TODO Support comparison operation
   void binop(BinaryOperator *bop) {
       Expr * left = bop->getLHS();
       Expr * right = bop->getRHS();


   //    llvm::errs() << "binop.\n";
       if (bop->isAssignmentOp()) {             // =
           int val = expr(right);
           mStack.back().bindStmt(left, val);
           if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
               Decl * decl = declexpr->getFoundDecl();
               mStack.back().bindDecl(decl, val);

           }
       }else if(bop->isAdditiveOp()){           // + -
            int valr = expr(right);
            int vall = expr(left);
            int res = 0;
            if(bop->getOpcode() == BO_Add){
                res = vall + valr;
            }else{
                res = vall - valr;
            }
            mStack.back().bindStmt(bop,res);
       }else if(bop->isMultiplicativeOp()){     // * /
            int valr = expr(right);
            int vall = expr(left);
            int res = 0;
            if(bop->getOpcode() == BO_Mul){
                res = vall * valr;
            }else{
                res = vall / valr;
            }
            mStack.back().bindStmt(bop,res);
       }else if(bop->isComparisonOp()){         // > < >= <= == !=
            int valr = expr(right);
            int vall = expr(left);
            int res = 0;
            switch(bop->getOpcode()){
                case BO_GT: res = (vall > valr);break;
                case BO_LT: res = (vall < valr);break;
                case BO_EQ: res = (vall == valr);break;
                case BO_GE: res = (vall >= valr);break;
                case BO_LE: res = (vall <= valr);break;
                case BO_NE: res = (vall != valr);break;
                default: llvm::errs()<<"Comparison Op not Identified.\n";break;
            }
            mStack.back().bindStmt(bop,res);
       }
   }
   
   //handle var delarations.
    void vardecl(VarDecl* vdecl,StackFrame* sf){
        if(vdecl->getType().getTypePtr()->isIntegerType()){
            int value = 0;
            if(vdecl->hasInit()){
                Expr* e = vdecl->getInit();
                value = expr(e);
                printf("vardecl %d\n",value);
            }
            sf->bindDecl(vdecl,value);
        }else{
            sf->bindDecl(vdecl, 0);
        }

    }


   void decl(DeclStmt * declstmt) {
       llvm::errs() << "\tdecl\n";
       for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end();
               it != ie; ++ it) {
           Decl * decl = *it; 
           if (VarDecl * vdecl = dyn_cast<VarDecl>(decl)) { 
                vardecl(vdecl,&(mStack.back()));
           }
       }
   }
   void declref(DeclRefExpr * declref) {
       llvm::errs() << "\t in declref\n";
       mStack.back().setPC(declref);
       if (declref->getType()->isIntegerType()) {
            Decl* decl = declref->getFoundDecl();
            //global or local value
            int val = mStack.back().findDecl(decl) ? mStack.back().getDeclVal(decl):mStack.front().getDeclVal(decl);
            printf("\tdecl::%d\n",val);
            mStack.back().bindStmt(declref, val);
       }
   }

   void cast(CastExpr * castexpr) {
       mStack.back().setPC(castexpr);
       if (castexpr->getType()->isIntegerType()) {
           Expr * expr = castexpr->getSubExpr();
           int val = mStack.back().getStmtVal(expr);
           mStack.back().bindStmt(castexpr, val );
       }
   }

   void ret(CallExpr* callexpr){
        FunctionDecl * callee = callexpr->getDirectCallee();
        if(callee->isNoReturn()){
            mStack.pop_back();
        } else {
            int rval = mStack.back().getRetValue();
            printf("pop stack:%d\n",rval);
            mStack.pop_back();
            mStack.back().bindStmt(callexpr,rval);
        }
        printf("ret here\n");
   }

   void retstmt(ReturnStmt* rstmt){
       Expr* rexpr = rstmt->getRetValue();
       if(rexpr){
            int rval = expr(rexpr);
            mStack.back().setRetValue(rval);
       }
       mStack.back().setReturned();
       printf("retstmt\n");
   }

   /// !TODO Support Function Call
   void call(CallExpr * callexpr) {
       llvm::errs() << "\t in call\n";
       mStack.back().setPC(callexpr);
       int val = 0;
       FunctionDecl * callee = callexpr->getDirectCallee();
       if (callee == mInput) {
            llvm::errs() << "Please Input an Integer Value : ";
            scanf("%d", &val);

            mStack.back().bindStmt(callexpr, val);
       } else if (callee == mOutput) {
           Expr * e = callexpr->getArg(0);
           val = expr(e);
           llvm::errs() << "==========OUTPUT:" << val <<"\n";
       } else {
           /// You could add your code here for Function call Return
            StackFrame calleeStack = StackFrame();
            unsigned param_num = callee->getNumParams();
            for(unsigned i = 0;i < param_num;i++){
                Expr * e = callexpr->getArg(i);
                int val = expr(e);
                VarDecl* vd = dyn_cast<VarDecl> (callee->getParamDecl(i));
                vardecl(vd,&calleeStack);
                calleeStack.bindDecl(vd,val);
            }
            mStack.push_back(calleeStack);
       }
   }
};


