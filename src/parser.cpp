
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

            if (regex_string[idx] == 's' || regex_string[idx] == 'S' ||
                regex_string[idx] == 'w' || regex_string[idx] == 'W' ||
                regex_string[idx] == 'd' || regex_string[idx] == 'D') {
                token_stack.push(
                    {N_RESERV_SET, S_RESERV_SET, regex_string[idx]});
            } else if (regex_string[idx] == 'b' || regex_string[idx] == 'B') {
                token_stack.push({N_BOUNDARY, S_CHARACTER, regex_string[idx]});
            } else {
                token_stack.push({N_CHARACTER, S_CHARACTER, regex_string[idx]});
            }
            continue;
        }

        switch (regex_string[idx]) {
        case '(':
            token_stack.push({N_LPAREN, S_CHARACTER, regex_string[idx]});
            break;
        case ')':
            token_stack.push({N_RPAREN, S_CHARACTER, regex_string[idx]});
            break;
        case '^':
            if (idx > 0 && regex_string[idx - 1] == '[') {
                token_stack.push({N_CHARACTER, S_NEG, regex_string[idx]});
            } else {
                token_stack.push({N_CHARACTER, S_CHARACTER, regex_string[idx]});
            }
            break;
        case '$':
            token_stack.push({N_EOL, S_CHARACTER, regex_string[idx]});
            break;
        case '.':
            token_stack.push({N_RESERV_SET, S_CHARACTER, regex_string[idx]});
            break;
        case '|':
            token_stack.push({N_OR, S_CHARACTER, regex_string[idx]});
            break;
        case '[':
            token_stack.push({N_LSET, S_LSET, regex_string[idx]});
            break;
        case ']':
            token_stack.push({N_RSET, S_RSET, regex_string[idx]});
            break;
        case '+':
        case '*':
        case '?':
            token_stack.push({N_POST_MODIFIER, S_CHARACTER, regex_string[idx]});
            break;
        case '-':
            token_stack.push({N_CHARACTER, S_RANGE, regex_string[idx]});
            break;
        default:
            token_stack.push({N_CHARACTER, S_CHARACTER, regex_string[idx]});
        }
    }
    token_stack.push({N_TERMINATOR, S_TERMINATOR, '\0'});
    return token_stack;
}

bool validate_set(TokenStack &token_stack) {
    using enum Token::SetType;

    // negation operations
    token_stack.expect(S_NEG);
    while (!token_stack.empty() && !token_stack.expect(S_RSET)) {
        // rule: we always treat dash as a range operator
        Token t = token_stack.pop();

        // ranges have to be surrounded by two members
        if (t.set_type == S_RANGE) {
            return false;
        }

        if (t.set_type == S_RESERV_SET) {
            continue;
        }

        // the remaining case is that it's a member
        assert(t.set_type == S_CHARACTER);
        // peek to see if it's the range op
        if (token_stack.expect(S_RANGE)) {
            if (!(token_stack.expect(S_CHARACTER))) {
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
    if (token_stack.expect(N_RPAREN)) {
        // defer back to the higher level of parsing
        return true;
    }

    // remove the BOL if it exists
    token_stack.expect(N_BOL);

    // pre-boundary values
    while (token_stack.expect(N_BOUNDARY)) {
    }

    if (token_stack.expect(N_LPAREN)) {
        // parse the sub expression
        if (!validate_helper(token_stack)) {
            return false;
        }

        // handle the post modifiers
        token_stack.expect(N_POST_MODIFIER);

        // post-boundary values
        while (token_stack.expect(N_BOUNDARY)) {
        }

        // remove the EOL
        token_stack.expect(N_EOL);

        return validate_helper(token_stack);
    }

    if (token_stack.expect(N_LSET)) {
        if (!validate_set(token_stack)) {
            return false;
        }

        token_stack.expect(N_POST_MODIFIER);
        while (token_stack.expect(N_BOUNDARY)) {
        }
        token_stack.expect(N_EOL);
        return validate_helper(token_stack);
    }

    if (token_stack.expect(N_OR)) {
        return validate_helper(token_stack);
    }

    // remaining case, sequence of dots and chars
    while (true) {
        // expect a char or a set
        if (!token_stack.expect(N_CHARACTER, N_RESERV_SET)) {
            return false;
        }

        // optionally a post modifier
        token_stack.expect(N_POST_MODIFIER);

        // then maybe a boundary (and there can be multiple)
        while (token_stack.expect(N_BOUNDARY)) {
            ;
        }
    }

    // remove the EOL if it exists
    token_stack.expect(N_EOL);

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
        return;
        break;
    }
    // remove the token if we saw it
    token_stack.expect(N_POST_MODIFIER);
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
    switch (t.base_character) {
    case 's': {
        return {' ', '\t'};
        break;
    }
    case 'S': {
        return remove_from_base_set({{'\t', ' '}});
    }
    case 'd': {
        return create_from_ranges({{'0', '9'}});
    }
    case 'D': {
        return remove_from_base_set({{'0', '9'}});
    }
    case 'w': {
        return create_from_ranges(
            {{'0', '9'}, {'_', '_'}, {'a', 'z'}, {'A', 'Z'}});
    }
    case 'W': {
        return remove_from_base_set(
            {{'0', '9'}, {'A', 'Z'}, {'_', '_'}, {'a', 'z'}});
    }
    default:
        return {};
    }
}

void compile_set(TableBuilder &table_builder, TokenStack &token_stack) {
    using enum Token::SetType;

    // negation operations
    bool neg_mode = token_stack.expect(S_NEG);
    std::vector<char> char_set;
    while (!token_stack.empty() && token_stack.expect(S_RSET)) {
        // rule: we always treat dash as a range operator
        Token t = token_stack.pop();

        // ranges have to be surrounded by two members
        assert(t.set_type != S_RANGE);

        if (t.set_type == S_RESERV_SET) {
            auto set = compile_reserved_set(t);
            char_set.insert(char_set.end(), set.begin(), set.end());
            continue;
        }

        // the remaining case is that it's a member
        assert(t.set_type == S_CHARACTER);
        // peek to see if it's the range op
        if (token_stack.expect(S_RANGE)) {
            Token s = token_stack.pop();
            assert(s.set_type == S_CHARACTER);
            // now we create the range for t to s
            auto set =
                create_from_ranges({{t.base_character, s.base_character}});
            char_set.insert(char_set.end(), set.begin(), set.end());
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
    // and optionally a word boundary
    using enum Token::NormalType;

    Token char_token = token_stack.pop();
    std::vector<char> char_set;
    if (char_token.normal_type == N_CHARACTER) {
        char_set.push_back(char_token.base_character);
    } else {
        assert(char_token.normal_type == N_RESERV_SET);
        char_set = compile_reserved_set(char_token);
    }
    table_builder.add_char_set_mode(char_set);

    switch (token_stack.peek().base_character) {
    case '*':
        table_builder.star_modify();
        break;
    case '+':
        table_builder.plus_modify();
        break;
    case '?':
        table_builder.question_modify();
        break;
    default:
        break;
    }
    // \b?
}

// should we just throw exception if it doesn't compile?
void compile_helper(TableBuilder &table_builder, TokenStack &token_stack) {
    if (token_stack.empty()) {
        return;
    }

    using enum Token::NormalType;
    if (token_stack.peek().normal_type == RPAREN) {
        return;
    }
    TableBuilder curr_table;

    // if not start a new instance of table_builder
    if (token_stack.expect(BOL)) {
        // how should we handle bol? make a marker?
        curr_table.bol_modify();
    }

    while (true) {
        if (token_stack.expect(BOUNDARY)) {
            curr_table.boundary_prefix_modify();
        } else if (token_stack.expect(NON_BOUNDARY)) {
            curr_table.non_boundary_prefix_modify();
        } else {
            break;
        }
    }

    if (token_stack.expect(LPAREN)) {
        // parse the sub expression
        compile_helper(curr_table, token_stack);
        token_stack.expect(RPAREN);

        // need to lookahead here, unfortunately.
        compile_post_modifier(curr_table, token_stack);

        table_builder += curr_table;
        // now compile the rest of the pattern.
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

    while (true) {
        Token::NormalType t_type = token_stack.peek().normal_type;
        assert(t_type == CHARACTER || t_type == DOT ||
               t_type == SET_WHITESPACE || t_type == SET_NON_WHITESPACE ||
               t_type == SET_DIGIT || t_type == SET_NON_DIGIT ||
               t_type == SET_WORD || t_type == SET_NON_WORD);

        // get the char, the post modifier and optionally
        // the boundary markers
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
