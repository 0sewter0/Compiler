#include "include/parser.h"
#include "include/ast.h"
#include "include/diagnostic.h"
#include <cerrno>
#include <cstdlib>
#include <limits>

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    if(diagnostic::is_panicking) return nullptr;
    std::string typeName;

    if(match(TokenType::kwStruct)) {
        typeName = consume(TokenType::Identifier, "Syntax error: Expected variable struct type").lexeme;
    } else if(match(TokenType::KwInt)) {
        typeName = "int";
    } else if(match(TokenType::kwFloat)) {
        typeName = "float";
    } else if(peek().type == TokenType::Identifier) {
        typeName = advance().lexeme;
    } else {
        diagnostic::recordError("Expected type specifier", peek());
        sync();
        return nullptr;
    }
    std::string varName = consume(TokenType::Identifier, "Syntax error: Expected variable name after type").lexeme;

    if(match(TokenType::LBracket)) {
        auto size = parseExpr();

        consume(TokenType::RBracket, "Syntax error: Expected ']' after array size");
        consume(TokenType::Semicolon, "Syntax error: Expected ';' after array declaration");

        if(diagnostic::is_panicking) return nullptr;
        return std::make_unique<VLADeclAST>(varName, std::move(size));
    }

    std::unique_ptr<ExprNode> initVal = nullptr;
    if(match(TokenType::Assign)) {
        initVal = parseExpr();
    }

    consume(TokenType::Semicolon, "Syntax error: Expected ';'");
    if(diagnostic::is_panicking) return nullptr;

    return std::make_unique<VarDecAST>(varName, std::move(initVal), typeName);
}

std::unique_ptr<ASTNode> Parser::parseStructDecl() {
    if(diagnostic::is_panicking) return nullptr;

    match(TokenType::kwStruct);
    std::string structName = consume(TokenType::Identifier, "Syntax error: Expected struct name").lexeme;

    consume(TokenType::LBrace, "Syntax error: Expected '{' after struct name");

    std::vector<FieldDef> fields;
    while(peek().type != TokenType::RBrace && !isAtEnd()) {
        std::string typeName;

        if(peek().type == TokenType::KwInt || peek().type == TokenType::kwFloat) {
            typeName = advance().lexeme;
        } else {
            diagnostic::recordError("Expected field type (e.g. 'int')", peek());
            sync();
            continue;
        }

        std::string fieldName = consume(TokenType::Identifier, "Syntax error: Expected variable name after type").lexeme;
        consume(TokenType::Semicolon, "Syntax error: Expected ';' after variable");
        fields.push_back({typeName, fieldName});
    }
    consume(TokenType::RBrace, "Syntax error: Expected '}'");
    consume(TokenType::Semicolon, "Syntax error: Expected ';' after struct");
    
    if(diagnostic::is_panicking) return nullptr;

    return std::make_unique<StructDeclAST>(structName, std::move(fields));
}
