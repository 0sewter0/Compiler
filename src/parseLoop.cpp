#include "include/ast.h"
#include "include/parser.h"

std::unique_ptr<ASTNode> Parser::parseWhileLoop() {
    if(diagnostic::is_panicking) return nullptr;
    match(TokenType::kwWhile);

    consume(TokenType::LParen, "Syntax error: Expected '(' after while");
    
    auto cond = parseExpr(0);

    consume(TokenType::RParen, "Syntax error: Expected ')' after condition");

    auto body = parseStatement();

    if(diagnostic::is_panicking || !body) return nullptr;

    return std::make_unique<WhileLoopAST>(std::move(cond), std::move(body));
}