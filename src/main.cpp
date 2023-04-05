#include "matcher.h"
#include "parser.h"

#include <iostream>
#include <optional>
#include <string>

int main() {
    // std::string input_line;
    // std::cout << "enter line to parse" << std::endl;
    // while (std::getline(std::cin, input_line)) {
    //     auto maybe_res = tokenize(input_line);
    //     if (!maybe_res) {
    //         std::cout << "tokenize results: tokenize error" << std::endl;
    //         continue;
    //     }
    //     auto token_res = *maybe_res;
    //     std::cout << "tokenize results:" << std::endl;
    //     while (!token_res.empty()) {
    //         std::cout << token_res.pop() << std::endl;
    //     }
    //     // check to make sure the terminator is in
    //     std::cout << token_res.pop() << std::endl;
    //     token_res.reset_state();

    //     bool validation_res = validate(token_res);
    //     std::cout << "====================" << std::endl;
    //     std::cout << ((validation_res) ? "passed the parser"
    //                                    : "failed the parser")
    //               << std::endl;
    //     std::cout << "====================" << std::endl;

    //     std::cout << "enter line to parse" << std::endl;
    // }

    // Matcher m{"abc"};
    // auto token_stack = tokenize("abc");
    // if (!token_stack) {
    //     throw std::runtime_error("invalid regex pattern");
    // }

    // std::cout << "token stack" << std::endl;
    // std::cout << *token_stack << std::endl;

    // if (!validate(*token_stack)) {
    //     throw std::runtime_error("invalid regex pattern");
    // }
    // (*token_stack).reset_state();

    // std::cout << "empty check : " << (*token_stack).empty() << std::endl;

    // auto table = compile(*token_stack);
    // std::cout << table << std::endl;

    Matcher m{"^ab$", false};
    std::cout << "table contents:=================== " << std::endl;
    std::cout << m << std::endl;
    auto results = m.match("ab");
    if (results.empty()) {
        std::cout << "no matches" << std::endl;
    } else {
        std::cout << "match list:=======================" << std::endl;
        for (auto res : results) {
            std::cout << "{ starting offset: " << res.starting_offset << "; ";
            std::cout << "ending offset: " << res.ending_offset << "}"
                      << std::endl;
        }
    }
}
