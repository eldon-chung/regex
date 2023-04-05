#pragma once

#include "TransitionTable.h"
#include "parser.h"

#include <algorithm>
#include <array>
#include <list>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <vector>

struct Result {
    size_t offset;
    size_t length;
};

class Matcher {
    struct State {
        std::unordered_set<TransitionTable::State, TransitionTable::State::Hash>
            fa_states;
        size_t starting_offset;
        size_t ending_offset;
    };

    struct Result {
        size_t starting_offset;
        size_t ending_offset;
    };

    TransitionTable table;
    std::vector<State> active_matches;

    // to disable multi-line matching. it will retain the newlines
    // to help match against ^
    static std::vector<std::string_view> break_into_lines(std::string_view sv) {
        std::vector<std::string_view> to_ret;
        while (!sv.empty()) {
            size_t next_newl = sv.find('\n');
            if (next_newl == std::string::npos) {
                to_ret.push_back(sv);
                sv = "";
            } else {
                // this should be ok. out_of_range is only thrown
                // when (next_newl + 1) > sv.size()
                to_ret.push_back(sv.substr(0, next_newl + 1));
                sv = sv.substr(next_newl + 1);
            }
        }
        return to_ret;
    }

  public:
    Matcher(std::string_view pattern, bool reverse = false) {
        // should we really be validating the token stack in the matcher?
        auto token_stack = tokenize(pattern);
        if (!token_stack) {
            throw std::runtime_error("invalid tokenization");
        }

        if (!validate(*token_stack)) {
            throw std::runtime_error("invalid regex pattern");
        }
        (*token_stack).reset_state();
        table = compile(*token_stack, reverse);
    }

    std::vector<Result> match(std::string_view str) {
        std::vector<std::string_view> line_list = break_into_lines(str);
        std::vector<Result> total_results;
        size_t running_base_offset = 0;
        std::cerr << "num lines " << line_list.size() << std::endl;

        for (auto const &line : line_list) {
            std::cerr << "matching against line " << line << std::endl;
            auto line_res = match_line(line);

            // append the results
            std::transform(line_res.begin(), line_res.end(),
                           std::back_inserter(total_results),
                           [running_base_offset](Result res) -> Result {
                               return {res.starting_offset +
                                           running_base_offset,
                                       res.ending_offset + running_base_offset};
                           });

            running_base_offset += line.length();
        }

        return total_results;
    }

    friend std::ostream &operator<<(std::ostream &os, Matcher const &matcher);

  private:
    std::vector<Result> match_line(std::string_view str) {
        std::vector<Result> to_return;
        std::vector<State> next_active_states;

        // used to merge sets
        std::unordered_set<TransitionTable::State, TransitionTable::State::Hash>
            temp_union;

        // ok how to simulate bol?
        // add a state first, then feed in BOL

        // useful lambdas
        auto add_new_active_matches = [&](size_t idx) {
            // add a new active_match here
            std::cerr << "pushing back new starting states";
            std::cerr << "size: " << table.starting_states.size() << std::endl;
            active_matches.push_back(
                {{table.starting_states.begin(), table.starting_states.end()},
                 idx,
                 idx});
        };

        auto progress_states = [&](char char_to_match, size_t curr_idx) {
            for (auto &ac_st : active_matches) {
                // clear out the scratch space
                temp_union.clear();

                // go through all the possible states of that active match
                for (auto const &fa_state : ac_st.fa_states) {
                    // get all the possible next states
                    auto const &next_states =
                        table.get_transition(fa_state, char_to_match);

                    // append into temp union
                    std::for_each(next_states.begin(), next_states.end(),
                                  [&temp_union](auto const &next_state) {
                                      temp_union.insert(next_state);
                                  });
                }

                // if it's not empty, we update the match state list
                if (!temp_union.empty()) {
                    // overwrite the state list
                    ac_st.fa_states.swap(temp_union);
                    // see if any are matching

                    // the latter condition avoids empty string matches
                    if (table.is_accepting(ac_st.fa_states) &&
                        curr_idx > ac_st.starting_offset) {
                        to_return.push_back({ac_st.starting_offset, curr_idx});
                    }
                    // move this into next_active_state;
                    next_active_states.push_back(std::move(ac_st));
                }
            }
            active_matches = std::move(next_active_states);
            next_active_states.clear();
        };

        // feed in bol
        add_new_active_matches(0);
        progress_states(2, 0);

        // main iteration
        for (size_t str_idx = 0; str_idx < str.length(); ++str_idx) {
            std::cerr << "iter" << std::endl;
            add_new_active_matches(str_idx);
            print_active_states();
            progress_states(str[str_idx], str_idx);
        }
        print_active_states();
        // feed in eol
        progress_states(10, str.length());

        return to_return;
    }

  private:
    void print_active_states() const {
        std::cerr << "active matches: =============" << std::endl;
        for (auto const &ac_st : active_matches) {
            std::cerr << "===============================" << std::endl;
            std::cerr << "starting_offset: " << ac_st.starting_offset << ";";
            std::cerr << "ending_offset: " << ac_st.ending_offset << ";";
            std::cerr << "ac states: " << std::endl;
            for (auto const &fa_state : ac_st.fa_states) {
                std::cerr << fa_state << std::endl;
            }
            std::cerr << "===============================" << std::endl;
        }
        std::cerr << "===============================" << std::endl;
    }
};

inline std::ostream &operator<<(std::ostream &os, Matcher const &matcher) {
    os << matcher.table << std::endl;
    return os;
}
