
#include "parser.h"
#include "Token.h"
#include "matcher.h"

#include <assert.h>

#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

// returns std::nullopt if there are tokenizing issues..?
std::optional<TokenStack> tokenize(std::string_view regex_string) {
    TokenStack token_stack;
    for (size_t idx = 0; idx < regex_string.length(); ++idx) {
        if (regex_string[idx] == '\\') {
            // need to take the next char in faithfully
            ++idx;
            if (idx >= regex_string.length()) {
                return {};
            }
            token_stack.push({Token::Type::CHARACTER, regex_string[idx]});
            continue;
        }

        switch (regex_string[idx]) {
        case '(':
            token_stack.push({Token::Type::LPAREN, regex_string[idx]});
            break;
        case ')':
            token_stack.push({Token::Type::RPAREN, regex_string[idx]});
            break;
        case '^':
            token_stack.push({Token::Type::BOL, regex_string[idx]});
            break;

        case '$':
            token_stack.push({Token::Type::EOL, regex_string[idx]});
            break;

        case '+':
            token_stack.push({Token::Type::PLUS, regex_string[idx]});
            break;

        case '.':
            token_stack.push({Token::Type::DOT, regex_string[idx]});
            break;
        case '|':
            token_stack.push({Token::Type::OR, regex_string[idx]});
            break;
        case '[':
            token_stack.push({Token::Type::LSET, regex_string[idx]});
            break;
        case ']':
            token_stack.push({Token::Type::RSET, regex_string[idx]});
            break;
        case '*':
            token_stack.push({Token::Type::STAR, regex_string[idx]});
            break;
        case '?':
            token_stack.push({Token::Type::QUESTION, regex_string[idx]});
            break;
        default:
            token_stack.push({Token::Type::CHARACTER, regex_string[idx]});
        }
    }
    token_stack.push({Token::Type::PATTERN_TERMINATOR, '\0'});
    return token_stack;
}

void validate_set(TokenStack &token_stack) {
    using enum Token::Type;
    // negation operations
    token_stack.expect(BOL);
    // take everything literally, parse ^ differently.
    while (!token_stack.empty() && token_stack.except(RSET)) {
        ;
    }
}

bool validate_helper(TokenStack &token_stack) {
    std::cerr << "curr token " << token_stack.peek() << std::endl;

    if (token_stack.empty()) {
        return true;
    }

    using enum Token::Type;
    if (token_stack.peek().type == RPAREN) {
        // defer back to the higher level of parsing
        return true;
    }

    // remove the BOL if it exists
    token_stack.expect(BOL);
    if (token_stack.expect(LPAREN)) {
        std::cerr << "got a (" << std::endl;

        // parse the sub expression
        bool subexpr = validate_helper(token_stack);

        if (!subexpr || !token_stack.expect(RPAREN)) {
            std::cerr << "expected a )"
                      << "instead got" << token_stack.peek() << std::endl;
            return false;
        }

        // handle the post modifiers
        token_stack.expect(PLUS, STAR, EOL, QUESTION);
        return validate_helper(token_stack);
    }

    if (token_stack.expect(LSET)) {
        validate_set(token_stack);
        if (!token_stack.expect(RSET)) {
            return false;
        }
        token_stack.expect(PLUS, STAR, EOL, QUESTION);
        return validate_helper(token_stack);
    }

    if (token_stack.expect(OR)) {
        std::cerr << "got an or" << std::endl;
        return validate_helper(token_stack);
    }

    // we should not have post modifiers here
    if (token_stack.expect(PLUS, STAR, EOL, QUESTION)) {
        std::cerr << "we should not have post modifiers here" << std::endl;
        return false;
    }

    std::cerr << "char case?" << token_stack.peek() << std::endl;
    // remaining case, sequence of dots and chars
    while (token_stack.expect(CHARACTER, DOT)) {
        std::cerr << "optional post modifier" << token_stack.peek()
                  << std::endl;
        // optionally there might be a post modifier
        token_stack.expect(PLUS, STAR, QUESTION);
    }

    std::cerr << "out of char case: " << token_stack.peek() << std::endl;

    if (token_stack.expect(PLUS, STAR, QUESTION)) {
        std::cerr << "no more post modifiers" << token_stack.peek()
                  << std::endl;
        return false;
    }

    return validate_helper(token_stack);
}

void compile_post_modifier(TableBuilder &table_builder,
                           TokenStack &token_stack) {
    // handle the post modifiers
    using enum Token::Type;
    switch (token_stack.peek().type) {
    case PLUS:
        table_builder.plus_modify();
        break;
    case STAR:
        table_builder.star_modify();
        break;
    case EOL:
        table_builder.eol_modify();
        break;
    case QUESTION:
        table_builder.question_modify();
        break;
    default:
        // do nothing if it's neither of these
        break;
    }
    // remove the token if we saw it
    token_stack.expect(PLUS, STAR, EOL, QUESTION);
}

void compile_set(TableBuilder &table_builder, TokenStack &token_stack) {
    using enum Token::Type;
    // negation operations
    bool neg_mode = token_stack.expect(BOL);
    // take everything literally, parse ^ differently.

    std::vector<char> char_set;
    for (Token t = token_stack.peek(); t.type != RSET;
         token_stack.pop(), t = token_stack.peek()) {
        char_set.push_back(t.base_character);
    }

    if (neg_mode) {
        table_builder.add_char_neg_set_mode(char_set);
    } else {
        table_builder.add_char_set_mode(char_set);
    }
}

void compile_char(TableBuilder &table_builder, TokenStack &token_stack) {
    std::cerr << "compile_char called" << std::endl;

    // expect a char and optionally a post char modifier
    using enum Token::Type;
    assert(token_stack.peek().type == CHARACTER ||
           token_stack.peek().type == DOT);

    Token char_token = token_stack.pop();
    std::cerr << "next char_token: " << char_token << std::endl;
    if (char_token.type == CHARACTER) {
        // peek to see if there's a modifier
        switch (token_stack.peek().type) {
        case STAR:
            table_builder.add_star_char(char_token.base_character);
            break;
        case PLUS:
            table_builder.add_plus_char(char_token.base_character);
            break;
        case QUESTION:
            table_builder.add_question_char(char_token.base_character);
            break;
        default:
            table_builder.add_char(char_token.base_character);
            break;
        }
    } else {
        assert(char_token.type == DOT);
        switch (token_stack.peek().type) {
        case STAR:
            table_builder.add_dot_star();
            break;
        case PLUS:
            table_builder.add_dot_plus();
            break;
        case QUESTION:
            table_builder.add_dot_question();
            break;
        default:
            table_builder.add_dot_char();
            break;
        }
    }
    // if it was in the stack we can remove it now
    token_stack.expect(STAR, PLUS, QUESTION);
}

// should we just throw exception if it doesn't compile?
void compile_helper(TableBuilder &table_builder, TokenStack &token_stack) {
    using enum Token::Type;
    if (token_stack.empty()) {
        std::cerr << "token stack is empty, returning" << std::endl;
        return;
    }

    if (token_stack.expect(RPAREN)) {
        std::cerr << "rparen returning" << std::endl;
        return;
    }
    // if not start a new instance of table_builder
    TableBuilder curr_table;
    if (token_stack.expect(BOL)) {
        // how should we handle bol? make a marker?
        curr_table.bol_modify();
    }

    if (token_stack.expect(LPAREN)) {
        // parse the sub expression
        std::cerr << "seeing an LPAREN" << std::endl;

        compile_helper(curr_table, token_stack);
        token_stack.expect(RPAREN);
        compile_post_modifier(curr_table, token_stack);

        table_builder += curr_table;
        // now compile the rest of the pattern
        compile_helper(table_builder, token_stack);
        return;
    }

    if (token_stack.expect(LSET)) {
        std::cerr << "seeing an LSET" << std::endl;
        compile_set(curr_table, token_stack);
        token_stack.expect(RSET);
        compile_post_modifier(curr_table, token_stack);
        table_builder += curr_table;
        compile_helper(table_builder, token_stack);
        return;
    }

    if (token_stack.peek().type == OR) {
        std::cerr << "seeing an OR" << std::endl;
        token_stack.pop();
        compile_helper(curr_table, token_stack);
        table_builder |= curr_table;
        return;
    }

    if (token_stack.peek().type == RPAREN) {
        return;
    }

    assert(token_stack.peek().type == CHARACTER ||
           token_stack.peek().type == DOT);
    while (token_stack.peek().type == CHARACTER ||
           token_stack.peek().type == DOT) {
        std::cerr << "calling compile char" << std::endl;
        compile_char(curr_table, token_stack);
    }
    // compile_post_modifier(curr_table, token_stack);
    std::cerr << "appending table" << std::endl;
    table_builder += curr_table;
    compile_helper(table_builder, token_stack);
    return;
}

TransitionTable compile(TokenStack &token_stack, bool reverse) {
    // start a table builder
    TableBuilder table_builder;
    compile_helper(table_builder, token_stack);
    if (reverse) {
        table_builder.reverse_table();
    }
    table_builder.shrink_to_fit();
    return std::move(table_builder.get_table());
}

bool validate(TokenStack &token_stack) {
    return validate_helper(token_stack) && token_stack.empty();
}