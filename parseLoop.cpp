#include "ast.h"
#include "parser.h"

std::unique_ptr<ASTNode> Parser::parseWhileLoop() {
    match(TokenType::kwWhile);

    consume(TokenType::LParen, "Syntax error: Expected '(' after while");

    auto cond = parseExpr();

    consume(TokenType::RParen, "Syntax error: Expected ')' after condition");

    auto body = parseStatement();

    return std::make_unique<WhileLoopAST>(std::move(cond), std::move(body));
}