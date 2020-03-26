#ifndef TE_CSV_PARSER_HPP_INCLUDED
#define TE_CSV_PARSER_HPP_INCLUDED

#include <optional>
#include <regex>

namespace {    
    struct unit {
    };
    template<typename It>
    struct csv_parser {
        It current;
        It end;
        csv_parser(It begin, It end) : current(begin), end(end) {
        }
        auto try_parse_regex(const std::regex& regex) -> std::optional<std::match_results<It>> {
            if (std::match_results<It> match; std::regex_search(current, end, match, regex)) {
                current = match[0].second;
                return std::optional{match};
            } else {
                return std::nullopt;
            }
        }
        auto try_parse_literal(const std::string_view literal) -> std::optional<std::string_view> {
            if (auto lit_begin = std::search(current, end, literal.begin(), literal.end()); lit_begin != end) {
                current = lit_begin + literal.size();
                return std::optional{std::string_view{std::to_address(lit_begin), literal.size()}};
            } else {
                return std::nullopt;
            }
        }
        auto next_field() -> std::optional<unit> {
            static const std::regex whitespace("\\s*");
            try_parse_regex(whitespace) && try_parse_literal(",") && try_parse_regex(whitespace);
            return {{}};
        }
    public:
        auto try_parse_quoted() -> std::optional<std::string> {
            static const std::regex match_quoted_str{"\"([a-zA-Z ]*)\""};
            auto result = try_parse_regex(match_quoted_str);
            if (result) {
                next_field();
                return (*result)[1].str();
            } else {
                return std::nullopt;
            }
        }
        auto parse_quoted() {
            return *try_parse_quoted();
        }
        auto try_parse_double() -> std::optional<double> {
            const char* const current_char = std::to_address(current);
            char* double_end;
            double parsed = std::strtod(current_char, &double_end);
            if (double_end != current_char) {
                current = decltype(current){double_end};
                next_field();
                return std::optional{parsed};
            } else {
                return std::nullopt;
            }
        }
        auto parse_double() {
            return *try_parse_double();
        }
        auto skip() -> std::optional<unit> {
            try_parse_quoted() || try_parse_double();
            return {{}};
        }
    };
}

#endif
