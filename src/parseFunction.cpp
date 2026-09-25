#include "include/ast.h"
#include "include/parser.h"

std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
    std::string FuncName = peek().lexeme;
    consume(TokenType::Identifier, "Syntax error: Expected function name in prototype");

    consume(TokenType::LParen, "Syntax error: Expected '(' in prototype");

    std::vector<std::string> ArgNames;
    
    while(peek().type == TokenType::KwInt) {
        advance();

        ArgNames.push_back(peek().lexeme);
        
        advance();
        if(peek().type == TokenType::Comma) {
            advance();
        }
    }
    consume(TokenType::RParen, "Syntax error: Expected ')' in prototype");

    return std::make_unique<PrototypeAST>(FuncName, std::move(ArgNames));
}

std::unique_ptr<FunctionAST> Parser::parseDefinition() {
    advance();
    auto Prototype = parsePrototype();
    if(!Prototype) return nullptr;

    if(peek().type == TokenType::LBrace) {
        advance();
        auto Body = parseBlock();
        if(!Body) return nullptr;

        return std::make_unique<FunctionAST>(std::move(Prototype), std::move(Body));
    }

    error("Expected '{' in function definition");
    return nullptr;
}