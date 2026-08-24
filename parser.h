#pragma once
#include "ast.h"
#include "token.h"
#include <vector>
#include <memory>

class Parser {
private:
    const std::vector<Token>& tokens;
    size_t pos = 0;
    const Token& peek() const;
    Token advance();
    bool isAtEnd() const;

    std::unique_ptr<ExprNode> parsePrimary();
    std::unique_ptr<ExprNode> parseTerm();
    std::unique_ptr<ExprNode> parseExpr();
    std::unique_ptr<ASTNode> parseVarDecl();
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}
    std::unique_ptr<ASTNode> parse();
};
