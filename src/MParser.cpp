#include "include/ast.h"
#include "include/parser.h"
#include "include/token.h"

bool isRightAssoc(TokenType type) {
    return type == TokenType::Caret;
} 

int precedenceOf(TokenType type) {
    switch(type) {
        case TokenType::Assign: return 1;

        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::LT:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::GE: return 2;

        case TokenType::Plus:
        case TokenType::Minus: return 10;
        case TokenType::Star:
        case TokenType::Slash: return 20;
        case TokenType::Caret: return 30;
        
        default: return -1;  
    }
}

std::unique_ptr<ExprNode> Parser::parsePrimary() {
    TRACE_PARSER;
    switch(peek().type) {
        case TokenType::Number: {
            std::string lexeme = advance().lexeme;
            if (lexeme.find('.') != std::string::npos) {
                return std::make_unique<FloatExprAST>(std::stof(lexeme));
            }
            return std::make_unique<NumberExprAST>(std::stoi(lexeme));
        }
        case TokenType::Identifier: {
            std::string name = advance().lexeme;
            std::unique_ptr<ExprNode> expr = std::make_unique<VariableExprAST>(name);

            while(true) {
                if(match(TokenType::Point)) {
                    std::string fieldName = consume(TokenType::Identifier, "Expected field name after '.'").lexeme;
                    expr = std::make_unique<StructAccessAST>(std::move(expr), fieldName);
                } else if(match(TokenType::LBracket)) {
                    auto indexExpr = parseExpr();
                    consume(TokenType::RBracket, "Syntax error: Expected ']'");
                    expr = std::make_unique<ArrayAccessAST>(std::move(expr), std::move(indexExpr));
                } else if(match(TokenType::LParen)) {
                    return parseCallExpr(name);
                } else {
                    break;
                }
            }
            return expr;
        }
        case TokenType::Minus: {
            advance();

            auto operand = parseExpr(25);
            return std::make_unique<UnaryMinusExprAST>('-', std::move(operand));
        }
        case TokenType::LParen: {
            advance();
            auto inner = parseExpr();
            consume(TokenType::RParen, "Syntax error: Expected ')'");
            return inner;
        }
        default:
            throw std::runtime_error("Expected number, variable or '('");
    }
}

std::unique_ptr<ExprNode> Parser::parseExpr(int precedence) {
    TRACE_PARSER;
    auto lhs = parsePrimary();

    while(true) {
        int prec = precedenceOf(peek().type);
        if(prec < precedence) break;

        Token op = advance();

        int nextMinPrec = isRightAssoc(op.type) ? prec : prec + 1;
        std::unique_ptr<ExprNode> rhs = parseExpr(nextMinPrec);

        lhs = std::make_unique<BinaryExprAST>(op.lexeme, std::move(lhs), std::move(rhs));
    }
    return lhs;
}
