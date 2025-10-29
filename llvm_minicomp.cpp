// llvm_minicomp.cpp
// Build with: 
// clang++ llvm_minicomp.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native` -O2 -o llvm_minicomp

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cctype>
#include <cstdlib>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;
using namespace std;

/* ---------- Lexer ---------- */
enum TokenKind { TK_EOF, TK_NUMBER, TK_IDENT, TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
                 TK_LPAREN, TK_RPAREN, TK_SEMI, TK_ASSIGN, TK_LET, TK_PRINT, TK_UNKNOWN };

struct Token { TokenKind kind; string text; };

struct Lexer {
    string src; size_t i=0;
    Lexer(const string &s): src(s) {}
    char peek() { return i<src.size() ? src[i] : '\0'; }
    char get() { return i<src.size() ? src[i++] : '\0'; }
    void skip_ws(){while(isspace(peek())) get();}
    Token next() {
        skip_ws(); char c=peek();
        if(c=='\0') return {TK_EOF,""};
        if(isalpha(c)||c=='_'){string id; while(isalnum(peek())||peek()=='_') id.push_back(get());
            if(id=="let") return {TK_LET,id};
            if(id=="print") return {TK_PRINT,id};
            return {TK_IDENT,id};}
        if(isdigit(c)){string num; while(isdigit(peek())) num.push_back(get()); return {TK_NUMBER,num};}
        get();
        switch(c){case '+': return {TK_PLUS,"+"}; case '-': return {TK_MINUS,"-"};
                    case '*': return {TK_MUL,"*"}; case '/': return {TK_DIV,"/"}; case '(': return {TK_LPAREN,"("};
                    case ')': return {TK_RPAREN,")"}; case ';': return {TK_SEMI,";"}; case '=': return {TK_ASSIGN,"="};
                    default: return {TK_UNKNOWN,string(1,c)};}
    }
};

/* ---------- AST ---------- */
struct Expr { virtual ~Expr()=default; };
using ExprPtr=unique_ptr<Expr>;

struct NumberExpr: Expr { int value; NumberExpr(int v): value(v){} };
struct VarExpr: Expr { string name; VarExpr(string n): name(move(n)){} };
struct BinaryExpr: Expr { char op; ExprPtr lhs,rhs; BinaryExpr(char o,ExprPtr l,ExprPtr r):op(o),lhs(move(l)),rhs(move(r)){} };

struct Stmt { virtual ~Stmt()=default; };
using StmtPtr=unique_ptr<Stmt>;
struct LetStmt: Stmt { string name; ExprPtr rhs; LetStmt(string n,ExprPtr r):name(move(n)),rhs(move(r)){} };
struct PrintStmt: Stmt { ExprPtr expr; PrintStmt(ExprPtr e):expr(move(e)){} };

/* ---------- Parser ---------- */
struct Parser {
    vector<Token> toks; size_t pos=0;
    Parser(Lexer &lex){while(true){Token t=lex.next(); toks.push_back(t); if(t.kind==TK_EOF) break;}}
    Token peek(){return toks[pos];} Token get(){return toks[pos++];}
    bool accept(TokenKind k){if(peek().kind==k){get();return true;} return false;}
    void expect(TokenKind k){if(!accept(k)){cerr<<"Parse error: expected token "<<(int)k<<" got '"<<peek().text<<"'\n"; exit(1);}}
    ExprPtr parseFactor(){Token t=peek();
        if(accept(TK_NUMBER)) return make_unique<NumberExpr>(stoi(t.text));
        if(accept(TK_IDENT)) return make_unique<VarExpr>(t.text);
        if(accept(TK_LPAREN)){auto e=parseExpr(); expect(TK_RPAREN); return e;}
        cerr<<"Unexpected token in factor: "<<t.text<<"\n"; exit(1);
    }
    ExprPtr parseTerm(){auto node=parseFactor();
        while(true){if(accept(TK_MUL)){auto r=parseFactor(); node=make_unique<BinaryExpr>('*',move(node),move(r));}
                    else if(accept(TK_DIV)){auto r=parseFactor(); node=make_unique<BinaryExpr>('/',move(node),move(r));} else break;}
        return node;
    }
    ExprPtr parseExpr(){auto node=parseTerm();
        while(true){if(accept(TK_PLUS)){auto r=parseTerm(); node=make_unique<BinaryExpr>('+',move(node),move(r));}
                    else if(accept(TK_MINUS)){auto r=parseTerm(); node=make_unique<BinaryExpr>('-',move(node),move(r));} else break;}
        return node;
    }
    vector<StmtPtr> parseAll(){
        vector<StmtPtr> out;
        while(peek().kind!=TK_EOF){
            if(accept(TK_LET)){Token id=get(); if(id.kind!=TK_IDENT){cerr<<"Expected identifier after let\n";exit(1);}
                expect(TK_ASSIGN); auto e=parseExpr(); expect(TK_SEMI); out.push_back(make_unique<LetStmt>(id.text,move(e)));}
            else if(accept(TK_PRINT)){auto e=parseExpr(); expect(TK_SEMI); out.push_back(make_unique<PrintStmt>(move(e)));}
            else if(peek().kind==TK_SEMI){get();} else {cerr<<"Unexpected token '"<<peek().text<<"'\n"; exit(1);}
        }
        return out;
    }
};

/* ---------- LLVM Codegen ---------- */
struct LLVMCompiler {
    LLVMContext context;
    unique_ptr<Module> module;
    IRBuilder<> builder;
    unordered_map<string, AllocaInst*> namedValues;

    LLVMCompiler(): module(make_unique<Module>("my_module", context)), builder(context) {}

    AllocaInst* createEntryBlockAlloca(Function* func, const string &name){
        IRBuilder<> tmpB(&func->getEntryBlock(), func->getEntryBlock().begin());
        return tmpB.CreateAlloca(Type::getInt32Ty(context), nullptr, name);
    }

    Value* compileExpr(Expr* e){
        if(auto n=dynamic_cast<NumberExpr*>(e)) return ConstantInt::get(Type::getInt32Ty(context), n->value);
        if(auto v=dynamic_cast<VarExpr*>(e)){
            auto it=namedValues.find(v->name);
            if(it==namedValues.end()){cerr<<"Unknown variable "<<v->name<<"\n"; exit(1);}
            return builder.CreateLoad(Type::getInt32Ty(context), it->second, v->name);
        }
        if(auto b=dynamic_cast<BinaryExpr*>(e)){
            Value* l=compileExpr(b->lhs.get());
            Value* r=compileExpr(b->rhs.get());
            switch(b->op){
                case '+': return builder.CreateAdd(l,r,"addtmp");
                case '-': return builder.CreateSub(l,r,"subtmp");
                case '*': return builder.CreateMul(l,r,"multmp");
                case '/': return builder.CreateSDiv(l,r,"divtmp");
                default: cerr<<"Unknown binary op\n"; exit(1);
            }
        }
        cerr<<"Unknown expr in compileExpr\n"; exit(1);
    }

    void compileStmt(Stmt* s, Function* mainFunc){
        if(auto ls=dynamic_cast<LetStmt*>(s)){
            Value* val=compileExpr(ls->rhs.get());
            AllocaInst* alloca=createEntryBlockAlloca(mainFunc, ls->name);
            builder.CreateStore(val, alloca);
            namedValues[ls->name]=alloca;
        } else if(auto ps=dynamic_cast<PrintStmt*>(s)){
            Value* val=compileExpr(ps->expr.get());
            // call printf
            FunctionType* printfType=FunctionType::get(Type::getInt32Ty(context), PointerType::get(Type::getInt8Ty(context),0), true);
            FunctionCallee printfFunc=module->getOrInsertFunction("printf", printfType);
            Value* fmtStr=builder.CreateGlobalStringPtr("%d\n");
            builder.CreateCall(printfFunc, {fmtStr,val});
        } else {
            cerr<<"Unknown stmt in compileStmt\n"; exit(1);
        }
    }

    void compile(vector<StmtPtr> &stmts){
        FunctionType* mainType=FunctionType::get(Type::getInt32Ty(context), false);
        Function* mainFunc=Function::Create(mainType, Function::ExternalLinkage, "main", module.get());
        BasicBlock* entry=BasicBlock::Create(context,"entry",mainFunc);
        builder.SetInsertPoint(entry);

        for(auto &s: stmts) compileStmt(s.get(), mainFunc);

        builder.CreateRet(ConstantInt::get(Type::getInt32Ty(context),0));
    }

    void dumpIR(){ module->print(outs(), nullptr); }
};

/* ---------- Main ---------- */
int main(int argc, char **argv){
    if(argc<2){ cerr<<"Usage: "<<argv[0]<<" <source_file>\n"; return 1; }
    ifstream in(argv[1]); if(!in){cerr<<"Cannot open "<<argv[1]<<"\n"; return 1;}
    string src((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());

    Lexer lex(src);
    Parser parser(lex);
    auto stmts=parser.parseAll();

    LLVMCompiler comp;
    comp.compile(stmts);
    comp.dumpIR(); // prints LLVM IR to stdout
}

