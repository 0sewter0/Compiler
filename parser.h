#pragma once
#include "ast.h"
#include "token.h"

#include <vector>
#include <memory>

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

    
    std::unique_ptr<ExprNode> parsePrimary();
    std::unique_ptr<ExprNode> parseTerm();
    std::unique_ptr<ExprNode> parseExpr();

    std::unique_ptr<VarDecAST> parseVarDecl();

    std::unique_ptr<IfStmtAST> parseIfStmt();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseStatement();

    std::unique_ptr<ASTNode> parseWhileLoop();

    std::unique_ptr<PrototypeAST> parsePrototype();
    std::unique_ptr<FunctionAST> parseDefinition();
    std::unique_ptr<ExprNode> parseCallExpr(std::string name);
    std::unique_ptr<ASTNode> parseReturnStmt();

    const Token& peek() const;
    Token advance();
    bool isAtEnd() const;

};

class SyntaxError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};