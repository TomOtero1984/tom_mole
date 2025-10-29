// llvm_minicomp.h
#pragma once

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

struct Token {
    TokenKind kind;
    string text;
};

struct Lexer {
    string src;
    size_t i;

    Lexer(const string &s);
    char peek();
    char get();
    void skip_ws();
    Token next();
};

/* ---------- AST ---------- */
struct Expr { virtual ~Expr() = default; };
using ExprPtr = unique_ptr<Expr>;

struct NumberExpr : Expr { int value; NumberExpr(int v); };
struct VarExpr    : Expr { string name; VarExpr(string n); };
struct BinaryExpr : Expr { char op; ExprPtr lhs,rhs; BinaryExpr(char o, ExprPtr l, ExprPtr r); };

struct Stmt { virtual ~Stmt() = default; };
using StmtPtr = unique_ptr<Stmt>;

struct LetStmt   : Stmt { string name; ExprPtr rhs; LetStmt(string n, ExprPtr r); };
struct PrintStmt : Stmt { ExprPtr expr; PrintStmt(ExprPtr e); };

/* ---------- Parser ---------- */
struct Parser {
    vector<Token> toks;
    size_t pos;

    Parser(Lexer &lex);
    Token peek();
    Token get();
    bool accept(TokenKind k);
    void expect(TokenKind k);

    ExprPtr parseFactor();
    ExprPtr parseTerm();
    ExprPtr parseExpr();
    vector<StmtPtr> parseAll();
};

/* ---------- LLVM Codegen ---------- */
struct LLVMCompiler {
    LLVMContext context;
    unique_ptr<Module> module;
    IRBuilder<> builder;
    unordered_map<string, AllocaInst*> namedValues;

    LLVMCompiler();

    AllocaInst* createEntryBlockAlloca(Function* func, const string &name);
    Value* compileExpr(Expr* e);
    void compileStmt(Stmt* s, Function* mainFunc);
    void compile(vector<StmtPtr> &stmts);
    void dumpIR();
};
