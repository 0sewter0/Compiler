#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include <string>

#include "ast.h"
#include "token.h"
#include "diagnostic.h"

class ParseTracer {
    static inline int depth = 0;
    std::string funcname;
public:
    ParseTracer(const std::string& name) : funcname(name) {
        std::cout << std::string(depth*2, ' ') << "-> " << funcname << std::endl;
        depth++;
    }

    ~ParseTracer() {
        depth--;
        std::cout << std::string(depth*2, ' ') << "<- " << funcname << std::endl;
    }
};

#ifdef PARSER_TRACE
#define TRACE_PARSER ParseTracer _tracer(__FUNCTION__)
#else
#define TRACE_PARSER ((void)0)
#endif

class Parser {
private:
    const std::vector<Token>& tokens;
    size_t pos = 0;
    Token GetNextTok();

    bool match(TokenType type);
    Token consume(TokenType type, const char* message);

public:
    explicit Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();
    std::unique_ptr<ASTNode> parseTopLevel();

    std::unique_ptr<ExprNode> parseExpr(int precedence = PREC_NONE);
    std::unique_ptr<ExprNode> parsePrimary();

    std::unique_ptr<ASTNode> parseVarDecl();

    std::unique_ptr<IfStmtAST> parseIfStmt();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseStatement();

    std::unique_ptr<ASTNode> parseWhileLoop();

    std::unique_ptr<PrototypeAST> parsePrototype();
    std::unique_ptr<FunctionAST> parseDefinition();
    std::unique_ptr<ExprNode> parseCallExpr(std::string name);
    std::unique_ptr<ASTNode> parseReturnStmt();

    std::unique_ptr<ASTNode> parseStructDecl();

    const Token& peek() const;
    Token advance();
    Token lookAhead(int n);
    bool isAtEnd() const;

    void sync();

    std::unique_ptr<EquationAST> parseEquation();

};
