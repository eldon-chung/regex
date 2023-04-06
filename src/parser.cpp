
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
    using enum Token::NormalType;
    using enum Token::SetType;
    TokenStack token_stack;
    for (size_t idx = 0; idx < regex_string.length(); ++idx) {
        if (regex_string[idx] == '\\') {
            // need to take the next char in faithfully
            ++idx;
            if (idx >= regex_string.length()) {
                return {};
            }

            switch (regex_string[idx]) {
            case 's':
                token_stack.push({CHARACTER, WHITESPACE, regex_string[idx]});
                break;
            case 'S':
                token_stack.push(
                    {CHARACTER, NON_WHITESPACE, regex_string[idx]});
                break;
            case 'd':
                token_stack.push({CHARACTER, DIGIT, regex_string[idx]});
                break;
            case 'D':
                token_stack.push({CHARACTER, NON_DIGIT, regex_string[idx]});
                break;
            case 'w':
                token_stack.push({CHARACTER, WORD, regex_string[idx]});
                break;
            case 'W':
                token_stack.push({CHARACTER, NON_WORD, regex_string[idx]});
                break;
            case 'b':
                token_stack.push({BOUNDARY, MEMBER, regex_string[idx]});
                break;
            case 'B':
                token_stack.push({NON_BOUNDARY, MEMBER, regex_string[idx]});
                break;
            default:
                token_stack.push({CHARACTER, MEMBER, regex_string[idx]});
                break;
            }

            // need to address the set types later
            continue;
        }

        switch (regex_string[idx]) {
        case '(':
            token_stack.push({LPAREN, MEMBER, regex_string[idx]});
            break;
        case ')':
            token_stack.push({RPAREN, MEMBER, regex_string[idx]});
            break;
        case '^':
            token_stack.push({BOL, NEG, regex_string[idx]});
            break;
        case '$':
            token_stack.push({EOL, MEMBER, regex_string[idx]});
            break;

        case '+':
            token_stack.push({PLUS, MEMBER, regex_string[idx]});
            break;
        case '.':
            token_stack.push({DOT, MEMBER, regex_string[idx]});
            break;
        case '|':
            token_stack.push({OR, MEMBER, regex_string[idx]});
            break;
        case '[':
            token_stack.push({LSET, MEMBER, regex_string[idx]});
            break;
        case ']':
            token_stack.push({RSET, MEMBER, regex_string[idx]});
            break;
        case '*':
            token_stack.push({STAR, MEMBER, regex_string[idx]});
            break;
        case '?':
            token_stack.push({QUESTION, MEMBER, regex_string[idx]});
            break;
        case '-':
            token_stack.push({CHARACTER, RANGE, regex_string[idx]});
            break;
        default:
            token_stack.push({CHARACTER, MEMBER, regex_string[idx]});
        }
    }
    token_stack.push({NORMAL_TERMINATOR, SET_TERMINATOR, '\0'});
    return token_stack;
}

bool validate_set(TokenStack &token_stack) {
    using enum Token::SetType;
    using enum Token::NormalType;
    // negation operations
    token_stack.expect(NEG);
    while (!token_stack.empty() && token_stack.peek().normal_type != RSET) {
        // rule: we always treat dash as a range operator
        Token t = token_stack.pop();

        // ranges have to be surrounded by two members
        if (t.set_type == RANGE) {
            return false;
        }

        if (t.set_type == WHITESPACE || t.set_type == NON_WHITESPACE ||
            t.set_type == DIGIT || t.set_type == NON_DIGIT ||
            t.set_type == WORD || t.set_type == NON_WORD || t.set_type == NEG) {
            continue;
        }

        // the remaining case is that it's a member
        assert(t.set_type == MEMBER);
        // peek to see if it's the range op
        if (token_stack.expect(RANGE)) {
            if (!(token_stack.expect(MEMBER, NEG))) {
                return false;
            }
        }
    }
    return true;
}

bool validate_helper(TokenStack &token_stack) {
    if (token_stack.empty()) {
        return true;
    }

    using enum Token::NormalType;
    if (token_stack.peek().normal_type == RPAREN) {
        // defer back to the higher level of parsing
        return true;
    }

    // remove the BOL if it exists
    token_stack.expect(BOL);
    if (token_stack.expect(LPAREN)) {
        // parse the sub expression
        bool subexpr = validate_helper(token_stack);
        if (!subexpr || !token_stack.expect(RPAREN)) {
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
        token_stack.expect(PLUS, STAR, QUESTION);
        token_stack.expect(EOL);
        return validate_helper(token_stack);
    }

    if (token_stack.expect(OR)) {
        return validate_helper(token_stack);
    }

    // we should not have post modifiers here
    if (token_stack.expect(PLUS, STAR, EOL, QUESTION)) {
        return false;
    }

    // remaining case, sequence of dots and chars
    while (token_stack.expect(CHARACTER)) {
        // optionally there might be a post modifier
        token_stack.expect(PLUS, STAR, QUESTION);
    }

    if (token_stack.expect(PLUS, STAR, QUESTION)) {
        return false;
    }

    // remove the EOL if it exists
    token_stack.expect(EOL);
    return validate_helper(token_stack);
}

void compile_post_modifier(TableBuilder &table_builder,
                           TokenStack &token_stack) {
    // handle the post modifiers
    using enum Token::NormalType;
    switch (token_stack.peek().base_character) {
    case '+':
        table_builder.plus_modify();
        break;
    case '*':
        table_builder.star_modify();
        break;
    case '?':
        table_builder.question_modify();
        break;
    default:
        // do nothing if it's neither of these
        break;
    }
    // remove the token if we saw it
    token_stack.expect(PLUS, STAR, QUESTION);
}

std::vector<char> create_base_set() {
    std::vector<char> base_set;
    for (char c = 32; c < 127; ++c) {
        base_set.push_back(c);
    }

    base_set.push_back(9);
    return base_set;
};

std::vector<char>
create_from_ranges(std::vector<std::pair<char, char>> ranges) {
    // assume the ranges are disjoint probably.
    // on pair i guess it does lexi ordering
    std::sort(ranges.begin(), ranges.end());
    std::vector<char> to_ret;
    for (auto ran : ranges) {
        if (ran.second < ran.first) {
            auto temp = ran.first;
            ran.first = ran.second;
            ran.second = temp;
        }

        for (char c = ran.first; c <= ran.second; ++c) {
            to_ret.push_back(c);
        }
    }
    return to_ret;
};

std::vector<char>
remove_from_base_set(std::vector<std::pair<char, char>> ranges) {
    std::sort(ranges.begin(), ranges.end(),
              [](auto const &a, auto const &b) { return a.first < b.first; });
    std::vector<char> to_ret;

    // the special case
    if (ranges.front().first != '\t') {
        to_ret.push_back('\t');
    }

    char c = 32;
    for (auto ran : ranges) {
        std::cerr << "ran.first: " << ran.first;
        std::cerr << "ran.second: " << ran.second << std::endl;
        while (c < ran.first) {
            to_ret.push_back(c);
            ++c;
        }

        if (c >= ran.first) {
            c = ran.second + 1;
        }
    }

    while (c < 127) {
        to_ret.push_back(c);
        ++c;
    }

    return to_ret;
};

std::vector<char> compile_reserved_set(Token t) {
    using enum Token::SetType;
    switch (t.set_type) {
    case WHITESPACE: {
        return {' ', '\t'};
        break;
    }
    case NON_WHITESPACE: {
        std::cerr << "hi im being called" << std::endl;
        return remove_from_base_set({{'\t', ' '}});
    }
    case DIGIT: {
        return create_from_ranges({{'0', '9'}});
    }
    case NON_DIGIT: {
        return remove_from_base_set({{'0', '9'}});
    }
    case WORD: {
        return create_from_ranges(
            {{'0', '9'}, {'_', '_'}, {'a', 'z'}, {'A', 'Z'}});
    }
    case NON_WORD: {
        return remove_from_base_set(
            {{'0', '9'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}});
    }
    default:
        return {};
    }
}

void compile_set(TableBuilder &table_builder, TokenStack &token_stack) {
    using enum Token::SetType;
    using enum Token::NormalType;
    // negation operations

    bool neg_mode = token_stack.expect(NEG);

    using enum Token::SetType;
    using enum Token::NormalType;
    // negation operations
    token_stack.expect(NEG);

    std::vector<char> char_set;
    while (!token_stack.empty() && token_stack.peek().normal_type != RSET) {
        // rule: we always treat dash as a range operator
        Token t = token_stack.pop();

        // ranges have to be surrounded by two members
        assert(t.set_type != RANGE);

        if (t.set_type == NEG) {
            char_set.push_back(t.base_character);
            continue;
        }
        std::cerr << "hi am i here" << std::endl;
        std::cerr << t << std::endl;
        if (t.set_type == WHITESPACE || t.set_type == NON_WHITESPACE ||
            t.set_type == DIGIT || t.set_type == NON_DIGIT ||
            t.set_type == WORD || t.set_type == NON_WORD || t.set_type == NEG) {
            std::cerr << "hi am i also here" << std::endl;

            auto set = compile_reserved_set(t);
            char_set.insert(char_set.end(), set.begin(), set.end());
            continue;
        }

        if (token_stack.expect(RANGE)) {
            Token rb = token_stack.pop();
            // use this next char as a range
            assert(rb.set_type == MEMBER || rb.set_type == NEG);
            auto set =
                create_from_ranges({{t.base_character, rb.base_character}});
            char_set.insert(char_set.end(), set.begin(), set.end());
            for (char c : set) {
                char_set.push_back(c);
            }
        }
    }

    if (neg_mode) {
        table_builder.add_char_neg_set_mode(char_set);
    } else {
        table_builder.add_char_set_mode(char_set);
    }
}

void compile_char(TableBuilder &table_builder, TokenStack &token_stack) {
    // expect a char and optionally a post char modifier
    using enum Token::NormalType;
    assert(token_stack.peek().normal_type == CHARACTER ||
           token_stack.peek().normal_type == DOT);

    Token char_token = token_stack.pop();
    if (char_token.normal_type == CHARACTER || char_token.normal_type == DOT) {
        switch (token_stack.peek().normal_type) {
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
        assert(char_token.normal_type == DOT);
        switch (token_stack.peek().normal_type) {
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
    using enum Token::NormalType;
    if (token_stack.empty()) {
        return;
    }

    if (token_stack.expect(RPAREN)) {
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
        compile_helper(curr_table, token_stack);
        token_stack.expect(RPAREN);
        compile_post_modifier(curr_table, token_stack);

        table_builder += curr_table;
        // now compile the rest of the pattern
        compile_helper(table_builder, token_stack);
        return;
    }

    if (token_stack.expect(LSET)) {
        compile_set(curr_table, token_stack);
        token_stack.expect(RSET);
        compile_post_modifier(curr_table, token_stack);
        table_builder += curr_table;
        if (token_stack.expect(EOL)) {
            table_builder.eol_modify();
        }
        compile_helper(table_builder, token_stack);
        return;
    }

    if (token_stack.peek().normal_type == OR) {
        token_stack.pop();
        compile_helper(curr_table, token_stack);
        table_builder |= curr_table;
        return;
    }

    if (token_stack.peek().normal_type == RPAREN) {
        return;
    }

    assert(token_stack.peek().normal_type == CHARACTER ||
           token_stack.peek().normal_type == DOT);
    while (token_stack.peek().normal_type == CHARACTER ||
           token_stack.peek().normal_type == DOT) {
        compile_char(curr_table, token_stack);
    }
    // compile_post_modifier(curr_table, token_stack);
    table_builder += curr_table;

    if (token_stack.expect(EOL)) {
        table_builder.eol_modify();
    }

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
