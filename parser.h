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
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}
    std::unique_ptr<ASTNode> parse();
};