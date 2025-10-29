//
// Created by Tom Otero on 10/28/25.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "llvm/IR/IRBuilder.h"

#include "llvm_minicomp.hpp"


using namespace llvm;
using namespace std;

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