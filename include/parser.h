#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "ast.h"
#include "token.h"

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

#define TRACE_PARSER ParseTracer _tracer(__FUNCTION__)

class Parser {
private:
    const std::vector<Token>& tokens;
    size_t pos = 0;

    Token GetNextTok();

    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);

    void error(const std::string& message);

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

    std::unique_ptr<ExprNode> parseEquation();

};

class SyntaxError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};