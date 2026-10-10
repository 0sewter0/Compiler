#include <iostream>
#include "include/diagnostic.h"

bool diagnostic::is_panicking = false;
std::vector<Error> diagnostic::errors_;

void diagnostic::recordError(const std::string& message, const Token &t) {
    if(is_panicking) return;
    errors_.push_back(Error{t.line, t.col, 1, message});
    is_panicking = true;
}

void diagnostic::printAllErrors() {
    for(const auto err : errors_) {
        std::cerr << "Parse error at " << err.line << ":" << err.col << ", " << err.message << std::endl;
    }
}