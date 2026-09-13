#include "parser.h"
#include "ast.h"

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
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
        throw std::runtime_error("Expected type specifier");
    }
    std::string varName = consume(TokenType::Identifier, "Syntax error: Expected variable name after type").lexeme;

    if(match(TokenType::LBracket)) {
        auto sizeToken = consume(TokenType::Number, "Syntax error: Expected array size");

        consume(TokenType::RBracket, "Syntax error: Expected ']' after array size");
        consume(TokenType::Semicolon, "Syntax error: Expected ';' after array declaration");
        
        return std::make_unique<ArrayDeclAST>(varName, std::stoi(sizeToken.lexeme));
    }

    std::unique_ptr<ExprNode> initVal = nullptr;

    if(match(TokenType::Assign)) {
        initVal = parseExpr();
    }
    consume(TokenType::Semicolon, "Syntax error: Expected ';'");

    return std::make_unique<VarDecAST>(varName, std::move(initVal), typeName);
}

std::unique_ptr<ASTNode> Parser::parseStructDecl() {
    match(TokenType::kwStruct);
    std::string structName = consume(TokenType::Identifier, "Syntax error: Expected struct name").lexeme;

    consume(TokenType::LBrace, "Syntax error: Expected '{' after struct name");

    std::vector<FieldDef> fields;
    while(peek().type != TokenType::RBrace) {
        std::string typeName;

        if(peek().type == TokenType::KwInt) {
            typeName = advance().lexeme;
        } else {
            throw std::runtime_error("Expected field type (e.g. 'int')");
        }

        std::string fieldName = consume(TokenType::Identifier, "Syntax error: Expected variable name after type").lexeme;
        consume(TokenType::Semicolon, "Syntax error: Expected ';' after variable");

        fields.push_back({typeName, fieldName});
    }
    consume(TokenType::RBrace, "Syntax error: Expected '}'");
    consume(TokenType::Semicolon, "Syntax error: Expected ';' after struct");

    return std::make_unique<StructDeclAST>(structName, std::move(fields));
}