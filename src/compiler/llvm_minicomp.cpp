// llvm_minicomp.cpp
#include "../include/llvm_minicomp.hpp"

/* ---------- Lexer ---------- */
Lexer::Lexer(const string &s) : src(s), i(0) {}

char Lexer::peek() { return i < src.size() ? src[i] : '\0'; }
char Lexer::get() { return i < src.size() ? src[i++] : '\0'; }
void Lexer::skip_ws() { while (isspace(peek())) get(); }

Token Lexer::next() {
    skip_ws();
    char c = peek();
    if (c == '\0') return {TK_EOF, ""};

    if (isalpha(c) || c == '_') {
        string id;
        while (isalnum(peek()) || peek() == '_') id.push_back(get());
        if (id == "let") return {TK_LET, id};
        if (id == "print") return {TK_PRINT, id};
        return {TK_IDENT, id};
    }

    if (isdigit(c)) {
        string num;
        while (isdigit(peek())) num.push_back(get());
        return {TK_NUMBER, num};
    }

    get();
    switch (c) {
        case '+': return {TK_PLUS, "+"};
        case '-': return {TK_MINUS, "-"};
        case '*': return {TK_MUL, "*"};
        case '/': return {TK_DIV, "/"};
        case '(': return {TK_LPAREN, "("};
        case ')': return {TK_RPAREN, ")"};
        case ';': return {TK_SEMI, ";"};
        case '=': return {TK_ASSIGN, "="};
        default: return {TK_UNKNOWN, string(1, c)};
    }
}

/* ---------- AST ---------- */
NumberExpr::NumberExpr(int v) : value(v) {}
VarExpr::VarExpr(string n) : name(move(n)) {}
BinaryExpr::BinaryExpr(char o, ExprPtr l, ExprPtr r) : op(o), lhs(move(l)), rhs(move(r)) {}
LetStmt::LetStmt(string n, ExprPtr r) : name(move(n)), rhs(move(r)) {}
PrintStmt::PrintStmt(ExprPtr e) : expr(move(e)) {}

/* ---------- Parser ---------- */
Parser::Parser(Lexer &lex) : pos(0) {
    while (true) {
        Token t = lex.next();
        toks.push_back(t);
        if (t.kind == TK_EOF) break;
    }
}

Token Parser::peek() { return toks[pos]; }
Token Parser::get() { return toks[pos++]; }
bool Parser::accept(TokenKind k) { if (peek().kind == k) { get(); return true; } return false; }
void Parser::expect(TokenKind k) { if (!accept(k)) { cerr << "Parse error: expected token " << (int)k << " got '" << peek().text << "'\n"; exit(1); } }

ExprPtr Parser::parseFactor() {
    Token t = peek();
    if (accept(TK_NUMBER)) return make_unique<NumberExpr>(stoi(t.text));
    if (accept(TK_IDENT)) return make_unique<VarExpr>(t.text);
    if (accept(TK_LPAREN)) { auto e = parseExpr(); expect(TK_RPAREN); return e; }
    cerr << "Unexpected token in factor: " << t.text << "\n"; exit(1);
}

ExprPtr Parser::parseTerm() {
    auto node = parseFactor();
    while (true) {
        if (accept(TK_MUL)) node = make_unique<BinaryExpr>('*', move(node), parseFactor());
        else if (accept(TK_DIV)) node = make_unique<BinaryExpr>('/', move(node), parseFactor());
        else break;
    }
    return node;
}

ExprPtr Parser::parseExpr() {
    auto node = parseTerm();
    while (true) {
        if (accept(TK_PLUS)) node = make_unique<BinaryExpr>('+', move(node), parseTerm());
        else if (accept(TK_MINUS)) node = make_unique<BinaryExpr>('-', move(node), parseTerm());
        else break;
    }
    return node;
}

vector<StmtPtr> Parser::parseAll() {
    vector<StmtPtr> out;
    while (peek().kind != TK_EOF) {
        if (accept(TK_LET)) {
            Token id = get();
            if (id.kind != TK_IDENT) { cerr << "Expected identifier after let\n"; exit(1); }
            expect(TK_ASSIGN);
            auto e = parseExpr();
            expect(TK_SEMI);
            out.push_back(make_unique<LetStmt>(id.text, move(e)));
        } else if (accept(TK_PRINT)) {
            auto e = parseExpr();
            expect(TK_SEMI);
            out.push_back(make_unique<PrintStmt>(move(e)));
        } else if (peek().kind == TK_SEMI) { get(); }
        else { cerr << "Unexpected token '" << peek().text << "'\n"; exit(1); }
    }
    return out;
}

/* ---------- LLVM Codegen ---------- */
LLVMCompiler::LLVMCompiler() : module(make_unique<Module>("my_module", context)), builder(context) {}

AllocaInst* LLVMCompiler::createEntryBlockAlloca(Function* func, const string &name) {
    IRBuilder<> tmpB(&func->getEntryBlock(), func->getEntryBlock().begin());
    return tmpB.CreateAlloca(Type::getInt32Ty(context), nullptr, name);
}

Value* LLVMCompiler::compileExpr(Expr* e) {
    if (auto n = dynamic_cast<NumberExpr*>(e)) return ConstantInt::get(Type::getInt32Ty(context), n->value);
    if (auto v = dynamic_cast<VarExpr*>(e)) {
        auto it = namedValues.find(v->name);
        if (it == namedValues.end()) { cerr << "Unknown variable " << v->name << "\n"; exit(1); }
        return builder.CreateLoad(Type::getInt32Ty(context), it->second, v->name);
    }
    if (auto b = dynamic_cast<BinaryExpr*>(e)) {
        Value* l = compileExpr(b->lhs.get());
        Value* r = compileExpr(b->rhs.get());
        switch (b->op) {
            case '+': return builder.CreateAdd(l, r, "addtmp");
            case '-': return builder.CreateSub(l, r, "subtmp");
            case '*': return builder.CreateMul(l, r, "multmp");
            case '/': return builder.CreateSDiv(l, r, "divtmp");
            default: cerr << "Unknown binary op\n"; exit(1);
        }
    }
    cerr << "Unknown expr in compileExpr\n"; exit(1);
}

void LLVMCompiler::compileStmt(Stmt* s, Function* mainFunc) {
    if (auto ls = dynamic_cast<LetStmt*>(s)) {
        Value* val = compileExpr(ls->rhs.get());
        AllocaInst* alloca = createEntryBlockAlloca(mainFunc, ls->name);
        builder.CreateStore(val, alloca);
        namedValues[ls->name] = alloca;
    } else if (auto ps = dynamic_cast<PrintStmt*>(s)) {
        Value* val = compileExpr(ps->expr.get());
        FunctionType* printfType = FunctionType::get(Type::getInt32Ty(context), PointerType::get(Type::getInt8Ty(context), 0), true);
        FunctionCallee printfFunc = module->getOrInsertFunction("printf", printfType);
        Value* fmtStr = builder.CreateGlobalStringPtr("%d\n");
        builder.CreateCall(printfFunc, {fmtStr, val});
    } else { cerr << "Unknown stmt in compileStmt\n"; exit(1); }
}

void LLVMCompiler::compile(vector<StmtPtr> &stmts) {
    FunctionType* mainType = FunctionType::get(Type::getInt32Ty(context), false);
    Function* mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", module.get());
    BasicBlock* entry = BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(entry);

    for (auto &s : stmts) compileStmt(s.get(), mainFunc);

    builder.CreateRet(ConstantInt::get(Type::getInt32Ty(context), 0));
}

void LLVMCompiler::dumpIR() { module->print(outs(), nullptr); }
