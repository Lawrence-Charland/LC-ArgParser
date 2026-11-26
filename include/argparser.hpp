// SPDX-License-Identifier: MIT

#ifndef LCPROJECTS_ARGPARSER_HPP
#define LCPROJECTS_ARGPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace argparser
{

inline constexpr const char *default_group = "default";

enum class ap_arg_style
{
    simple,
    forced_simple,
    single_dash,
    double_dash,
    argv_zero,
};

enum class ap_type
{
    positional,
    option,
    flag,
};

enum class ap_result
{
    success,
    unparsed,
    positional_too_few_args,
    option_too_few_values,
    option_too_many_values,
    flag_too_many_repetition,
    flag_unexpected_values,
    invalid_arguments,
};

// Single source of truth for ap_defkey: the string form of a key is exactly
// its member name. Add new keys here only.
#define AP_DEFKEY_LIST(X)                                                      \
    X(group)                                                                   \
    X(explanations)                                                            \
    X(no_consume)                                                              \
    X(long_name)                                                               \
    X(short_name)                                                              \
    X(min_positional_count)                                                    \
    X(max_positional_count)                                                    \
    X(barrier)                                                                 \
    X(no_variadic)                                                             \
    X(min_value_count)                                                         \
    X(max_value_count)                                                         \
    X(max_repetition)

enum class ap_defkey
{
#define AP_DEFKEY_ENUMERATOR(name) name,
    AP_DEFKEY_LIST(AP_DEFKEY_ENUMERATOR)
#undef AP_DEFKEY_ENUMERATOR
};

inline constexpr std::pair<std::string_view, ap_defkey> ap_defkey_names[] = {
#define AP_DEFKEY_PAIR(name) {#name, ap_defkey::name},
    AP_DEFKEY_LIST(AP_DEFKEY_PAIR)
#undef AP_DEFKEY_PAIR
};

inline constexpr std::string_view defkey_to_string(ap_defkey key)
{
    for (const auto &it : ap_defkey_names)
        if (it.second == key)
            return it.first;
    return {};
}

inline std::optional<ap_defkey> to_defkey(std::string_view name)
{
    for (const auto &it : ap_defkey_names)
        if (it.first == name)
            return it.second;
    return std::nullopt;
}

#undef AP_DEFKEY_LIST

#ifndef AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS
#define AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS "Not enough positional arguments"
#endif
#ifndef AP_DESCRIBE_OPTION_TOO_FEW_VALUES
#define AP_DESCRIBE_OPTION_TOO_FEW_VALUES "Not enough values for the option"
#endif
#ifndef AP_DESCRIBE_OPTION_TOO_MANY_VALUES
#define AP_DESCRIBE_OPTION_TOO_MANY_VALUES "Too many values for the option"
#endif
#ifndef AP_DESCRIBE_FLAG_TOO_MANY_REPETITION
#define AP_DESCRIBE_FLAG_TOO_MANY_REPETITION "Too many repetitions for the flag"
#endif
#ifndef AP_DESCRIBE_FLAG_UNEXPECTED_VALUES
#define AP_DESCRIBE_FLAG_UNEXPECTED_VALUES "Flags do not take values"
#endif
#ifndef AP_DESCRIBE_INVALID_ARGUMENTS
#define AP_DESCRIBE_INVALID_ARGUMENTS "Unrecognized or redundant argument"
#endif
#ifndef AP_DESCRIBE_UNKNOWN
#define AP_DESCRIBE_UNKNOWN "Unknown error"
#endif

inline std::string describe(ap_result result)
{
    switch (result)
    {
    case ap_result::positional_too_few_args:
        return AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS;
    case ap_result::option_too_few_values:
        return AP_DESCRIBE_OPTION_TOO_FEW_VALUES;
    case ap_result::option_too_many_values:
        return AP_DESCRIBE_OPTION_TOO_MANY_VALUES;
    case ap_result::flag_too_many_repetition:
        return AP_DESCRIBE_FLAG_TOO_MANY_REPETITION;
    case ap_result::flag_unexpected_values:
        return AP_DESCRIBE_FLAG_UNEXPECTED_VALUES;
    case ap_result::invalid_arguments:
        return AP_DESCRIBE_INVALID_ARGUMENTS;
    case ap_result::success:
        return ""; // Shouldn't be described actually.
    default:
        return AP_DESCRIBE_UNKNOWN;
    }
}

struct ap_entry
{
    // Definitions
    std::string name;
    std::unordered_set<std::string> group = {default_group};
    std::vector<std::string> explanations;
    ap_type type;
    bool consume = true;
    bool no_variadic = false;
    std::pair<unsigned, int> positional_range = {0, -1};
    std::pair<unsigned, int> valcnt_range = {0, -1};
    int max_repetition = -1;
    std::unordered_set<std::string> long_names;
    std::unordered_set<char> short_names;
    std::unordered_set<std::string> barriers;

    // Results
    ap_result general_result = ap_result::unparsed;
    std::vector<std::string> content;
    unsigned repetition = 0;

    ap_entry(const std::string &_name, ap_type _type,
             const std::vector<std::pair<ap_defkey, std::string>> &definitions);
};

struct ap_arg_token
{
    std::string pretty_name;
    std::string token;
    ap_arg_style style;

    ap_arg_token() = default;
    ap_arg_token(const std::string &_pretty_name, std::string _token,
                 ap_arg_style _style)
        : pretty_name(_pretty_name), token(std::move(_token)), style(_style)
    {
    }
};

class ap_instance
{
  private:
    std::list<ap_arg_token> args;
    std::vector<ap_entry> entries;
    std::unordered_map<char, bool> short_options;
    std::unordered_set<std::string> long_options;
    std::unordered_map<std::string, std::size_t> name_mapper;
    std::vector<ap_arg_token> tokenize(const std::string &word) const;
    void build_registries();

  public:
    ap_instance(const std::vector<ap_entry> &_entries);
    void add_entry(const ap_entry &entry, std::size_t index = SIZE_MAX);
    void delete_entry(std::size_t index, bool clear_storage = false);
    void update_entry(const ap_entry &entry);
    void pre_parse(int argc, char **argv);
    void pre_parse(const std::vector<std::string> &argv);
    void reparse();
    void evaluate(const std::string &group);
    void evaluate();
    void evaluate_entry(ap_entry &entry);
    void reset_results();
    bool fine(bool ignore_redundant = true);
    bool fine(const std::string &group);
    std::vector<std::pair<std::string, ap_result>> errors();
    const ap_entry &entry(const std::string &name);
    const ap_entry &entry(std::size_t index);
    ap_entry &entry_mutable(const std::string &name);
    ap_entry &entry_mutable(std::size_t index);
    std::size_t get_index(const std::string &name);
    const std::string_view argv_zero();
    std::unordered_set<std::string> ls_options();
};

} // namespace argparser

#endif
