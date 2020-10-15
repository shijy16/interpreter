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
   Environment() : mStack(), mFree(NULL), mMalloc(NULL), mInput(NULL), mOutput(NULL), mEntry(NULL) {
   }


   /// Initialize the Environment
   void init(TranslationUnitDecl * unit) {
       //global stackframe
       mStack.push_back(StackFrame());
       for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
           //global values
           if (VarDecl * vdecl = dyn_cast<VarDecl>(*i)){
               vardecl(vdecl);
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

   FunctionDecl * getEntry() {
       return mEntry;
   }

    //bind int literal stmt and value
    //void integerLiteral(IntegerLiteral* literal){
    //    Expr* literalExpr = dyn_cast<Expr>(literal);
    //    int value = (int)literal->getValue().getLimitedValue();
    //    mStack.back().bindStmt(literalExpr,value);
    //}
    
   void expr(Expr* expr){
       //integerLiteral
        if(IntegerLiteral* integerLiteral = dyn_cast<IntegerLiteral>(expr)){
            int value  = (int)integerLiteral->getValue().getSExtValue();
            mStack.back().bindStmt(expr,value);
        }
   }


   /// !TODO Support comparison operation
   void binop(BinaryOperator *bop) {
       Expr * left = bop->getLHS();
       Expr * right = bop->getRHS();

   //    llvm::errs() << "binop.\n";
       if (bop->isAssignmentOp()) {
           expr(right);
           int val = mStack.back().getStmtVal(right);
           mStack.back().bindStmt(left, val);
           if (DeclRefExpr * declexpr = dyn_cast<DeclRefExpr>(left)) {
               Decl * decl = declexpr->getFoundDecl();
               mStack.back().bindDecl(decl, val);

           }
       }
   }
   
   //handle var delarations.
    void vardecl(VarDecl* vdecl){
        if(vdecl->getType().getTypePtr()->isIntegerType()){
            int value = 0;
            if(vdecl->hasInit()){
                Expr* e = vdecl->getInit();
                expr(e);
                value = mStack.back().getStmtVal(e);
            }
            mStack.back().bindDecl(vdecl,value);
        }else{
            mStack.back().bindDecl(vdecl, 0);
        }

    }


   void decl(DeclStmt * declstmt) {
       llvm::errs() << "\tdecl\n";
       for (DeclStmt::decl_iterator it = declstmt->decl_begin(), ie = declstmt->decl_end();
               it != ie; ++ it) {
           Decl * decl = *it; 
           if (VarDecl * vdecl = dyn_cast<VarDecl>(decl)) { 
                vardecl(vdecl);
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

   /// !TODO Support Function Call
   void call(CallExpr * callexpr) {
       llvm::errs() << "\t in call\n";
       mStack.back().setPC(callexpr);
       int val = 0;
       FunctionDecl * callee = callexpr->getDirectCallee();
       if (callee == mInput) {
           llvm::errs() << "\t call INPUT\n";
           llvm::errs() << "Please Input an Integer Value : ";
          scanf("%d", &val);

          mStack.back().bindStmt(callexpr, val);
       } else if (callee == mOutput) {
           llvm::errs() << "\tcall PRINT\n";
           Expr * decl = callexpr->getArg(0);
           val = mStack.back().getStmtVal(decl);
           llvm::errs() << "==========OUTPUT:" << val <<"\n";
       } else {
           /// You could add your code here for Function call Return
       }
   }
};


