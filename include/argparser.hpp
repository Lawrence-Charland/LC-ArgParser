// SPDX-License-Identifier: MIT
/**
 * @file        argparser.hpp
 * @brief       A command line argument parser for GNU and UNIX styles.
 * Belongs to LC Projects. This is the header file.
 * @author      Lawrence Charland
 * @copyright   Copyright (c) 2026 Lawrence-Charland. Released under the MIT
 * License; see the `LICENSE` file for the full text.
 * @details     An `argparser::ap_instance` owns a list of
 * `argparser::ap_entry` objects, each describing one argument.
 * `argparser::ap_instance::pre_parse` splits the raw `argv` into styled tokens,
 * either from a `(argc, argv)` pair or from a `std::vector<std::string>`,
 * and `argparser::ap_instance::evaluate` matches those tokens against the
 * entries. The matched data is stored in the entries themselves, so no
 * separate result object is needed.
 */

#ifndef LCPROJECTS_ARGPARSER_HPP
#define LCPROJECTS_ARGPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * @brief       All the facilities of the argument parser.
 */
namespace argparser
{

/**
 * @brief       Argument styles, used to mark each argument to different kinds
 * (which happens in preparsing).
 */
enum class ap_arg_style
{
    /**
     * @brief       Will be treated as a positional argument, or a value of an
     * option, which depend on its position.
     */
    simple,
    /**
     * @brief       Will be treated as a short option, bundled short options, or
     * short options with sticky value.
     */
    single_dash,
    /**
     * @brief       Will be treated as a long option, or a long option in
     * `--key=value` format.
     */
    double_dash,
    /**
     * @brief       To mark the `argv[0]`, so that it will not be parsed
     * unexpectedly.
     */
    argv_zero,
};

/**
 * @brief       The kind of argument an entry describes.
 */
enum class ap_type
{
    /**
     * @brief       Positional arguments. Identified by its position
     * on the command line, not by a name or prefix.
     */
    positional,
    /**
     * @brief       Option. Named arguments introduced by a dash (`-`) or double
     * dash (`--`) that may take values.
     */
    option,
    /**
     * @brief       Flag. A named argument introduced by a dash (`-`) or
     * double dash (`--`) that takes no value, whose occurrence count is
     * recorded.
     * @see         argparser::ap_entry::content
     */
    flag,
};

/**
 * @brief       Results. A result is either `success`, `unparsed`, or one of
 * the error kinds.
 * @see         argparser::describe
 */
enum class ap_result
{
    /**
     * @brief       Nothing goes wrong in this entry. Used in entries
     * (`ap_entry::general_result`).
     */
    success,
    /**
     * @brief       Not parsed. Used in entries (`ap_entry::general_result`).
     */
    unparsed,
    /**
     * @brief       Not enough positional arguments.
     */
    positional_too_few_args,
    /**
     * @brief       Not enough values for the option.
     */
    option_too_few_values,
    /**
     * @brief       Too many values for the option.
     */
    option_too_many_values,
    /**
     * @brief       Too many flag repetitions.
     */
    flag_too_many_repetition,
    /**
     * @brief       Unexpected values for flags.
     */
    flag_unexpected_values,
    /**
     * @brief       Some arguments are not consumed (except `argv_zero`).
     */
    invalid_arguments,
};

/**
 * @brief       Definition keys, used to define an entry.
 * @details     Each key is paired with a string value in the definitions
 * passed to the `argparser::ap_entry` constructor. The pairs are applied in
 * order, and every key states whether repeating it is allowed.
 * @see         argparser::ap_entry
 */
enum class ap_defkey
{
    /**
     * @brief       The group this entry belongs to. The value is a group name.
     * Defaults to `default`.
     * @note        Effective for any type of entry.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    group,
    /**
     * @brief       The human-readable description of this entry. The value is
     * the explanation text itself, and may be empty. Defaults to empty.
     * @note        Effective for any type of entry.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    explanations,
    /**
     * @brief       Do not consume arguments. Will leave the parsed arguments
     * for the next entry.
     * @warning     Please leave the value empty.
     * @warning     May cause unexpected behaviour. Use with caution.
     * @note        Effective for any type of entry.
     * @note        Multiple definition is not recommended, although it will not
     * cause problems.
     */
    no_consume,
    /**
     * @brief       Long names of this option/flag. The value can be a name or
     * several names separated by `,`.
     * @warning     Each long name must not be empty.
     * @note        Effective for options and flags.
     * @note        Multiple definition is allowed, and will not cover the names
     * in previous definitions.
     * @note        The internal container will not record the same name twice.
     */
    long_name,
    /**
     * @brief       Short names of this option/flag. The value can be a name or
     * several names separated by `,`.
     * @warning     Each short name must be a single character.
     * @warning     Short names cannot be `-`.
     * @note        Effective for options and flags.
     * @note        Multiple definition is allowed, and will not cover the names
     * in previous definitions.
     * @note        The internal container will not record the same name twice.
     */
    short_name,
    /**
     * @brief       Minimum count of positional arguments required. The value
     * must be an integer `>=0`. Defaults to `0`.
     * @warning     The count must not be <0.
     * @note        The value is always `std::string`.
     * @note        Effective for positionals.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    min_positional_count,
    /**
     * @brief       Maximum count of positional arguments required. The value
     * must be an integer and must not be `0`. Set the value to a negative
     * number, or omit this key, to leave the count unlimited. Defaults to
     * `-1`.
     * @warning     The count must not be 0.
     * @note        The value is always `std::string`.
     * @note        Effective for positionals.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    max_positional_count,
    /**
     * @brief       Tokens that end the collection of a positional entry. The
     * value is the token text, compared verbatim against each positional
     * argument. Defaults to empty, which means no barrier.
     * @note        Effective for positionals only. A positional is otherwise
     * limited by its count range alone, so the barrier adds another way to
     * decide where it stops; options and flags do not use it.
     * @note        Multiple definition is allowed, and will not cover the
     * tokens in previous definitions.
     * @note        The barrier token is not stored in `ap_entry::content`. The
     * collection stops at it, and the token is consumed together with the
     * collected positionals (unless `consume` is `false`), so that the
     * following entries never see it. The arguments after it are left
     * untouched.
     * @note        The value is compared literally and is not split at `,`,
     * unlike `long_name` and `short_name`.
     * @warning     The value must not be empty.
     * @warning     `--` cannot be used as a barrier, because the preparser
     * removes it before the entries are evaluated. Every argument after `--`
     * is marked as `argparser::ap_arg_style::simple`, so a barrier may look
     * like an option there, e.g. `-x`.
     * @warning     The barrier is only reached while the entry is still
     * collecting values: once `max_positional_count` is fulfilled, the
     * collection stops before the barrier, which then stays available to the
     * following entries (and is reported as
     * `argparser::ap_result::invalid_arguments` if nobody consumes it).
     */
    barrier,
    /**
     * @brief       Minimum count of values for the option. The value must be an
     * integer `>=0`. Defaults to `0`.
     * @warning     The count must not be <0.
     * @note        The value is always `std::string`.
     * @note        Effective for options.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     * @warning     A value greater than `0` makes the option **required**: the
     * collected value count is compared against this minimum without checking
     * whether the option occurred, so an absent option reports
     * `argparser::ap_result::option_too_few_values` as well. Keep it at `0`
     * for an optional option.
     */
    min_value_count,
    /**
     * @brief       Maximum count of values for the option. The value must be an
     * integer and must not be `0`. Set the value to a negative number, or omit
     * this key, to leave the count unlimited. Defaults to `-1`.
     * @warning     The count must not be 0.
     * @note        The value is always `std::string`.
     * @note        Effective for options.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    max_value_count,
    /**
     * @brief       Maximum repetition for the flag. The value must be an
     * integer and must not be `0`. Set the value to a negative number, or omit
     * this key, to leave the repetition unlimited. Defaults to `-1`.
     * @warning     The count must not be 0.
     * @note        The value is always `std::string`.
     * @note        Effective for flags.
     * @note        Multiple definition is not recommended, and the last
     * definition is effective.
     */
    max_repetition,
};

/**
 * @name        Result description messages
 * The messages returned by `argparser::describe`. Define any of them before
 * including this header to replace the built-in message.
 * @{
 */
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
/**
 * @}
 */

/**
 * @brief       Describes a parse result.
 * @param       result          The result to describe.
 * @return      A human-readable message for `result`.
 * @note        The message can be replaced by defining the corresponding
 * `AP_DESCRIBE_*` macro before including this header.
 * @note        `argparser::ap_result::success` and
 * `argparser::ap_result::unparsed` are not errors, so they are described by
 * `AP_DESCRIBE_UNKNOWN`.
 * @see         argparser::ap_result
 */
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
    default:
        return AP_DESCRIBE_UNKNOWN;
    }
}

/**
 * @brief       Entry, to define how a specific cmdline argument works.
 * @note        The definition fields are initialized by the constructor and
 * may be edited afterwards. The result fields are filled by
 * `argparser::ap_instance::evaluate`.
 */
struct ap_entry
{
    /**
     * @brief       The name of the entry.
     * @warning     No two entries of the same instance may share a name.
     */
    std::string name;
    /**
     * @brief       The group the entry belongs to. Defaults to `default`.
     * @see         argparser::ap_instance::evaluate
     */
    std::string group = "default";
    /**
     * @brief       The human-readable description of the entry, typically
     * printed in the usage message. Defaults to empty.
     * @see         argparser::ap_defkey::explanations
     */
    std::vector<std::string> explanations;
    /**
     * @brief       The type of the target argument.
     * @see         argparser::ap_type
     */
    ap_type type;
    /**
     * @brief       Whether the arguments matched by this entry are removed
     * from the pool, so that the following entries cannot use them. When
     * false, the matched arguments stay available to the next entry.
     * Defaults to true.
     * @see         argparser::ap_defkey::no_consume
     */
    bool consume = true;

    /**
     * @brief       The range of the positional argument count. The first member
     * is the minimum count, and the second is the maximum count. Defaults to
     * `{0,-1}`.
     * @see         argparser::ap_defkey::min_positional_count
     * @see         argparser::ap_defkey::max_positional_count
     */
    std::pair<unsigned, int> positional_range = {0, -1};
    /**
     * @brief       The range of the value count of the option. The first member
     * is the minimum count, and the second is the maximum count. Defaults to
     * `{0,-1}`.
     * @note        A minimum greater than `0` makes the option required; see
     * `argparser::ap_defkey::min_value_count`.
     * @see         argparser::ap_defkey::min_value_count
     * @see         argparser::ap_defkey::max_value_count
     */
    std::pair<unsigned, int> valcnt_range = {0, -1};
    /**
     * @brief       The maximum repetition of the flag. A negative value means
     * unlimited. Defaults to `-1`.
     * @see         argparser::ap_defkey::max_repetition
     */
    int max_repetition = -1;
    /**
     * @brief       The long names of the option or the flag.
     * @note        Will not record the same name twice.
     * @note        The long name should not be introduced by double dash.
     * @see         argparser::ap_defkey::long_name
     */
    std::unordered_set<std::string> long_names;
    /**
     * @brief       The short names of the option or the flag.
     * @note        Will not record the same name twice.
     * @note        The short name should not be introduced by single dash.
     * Actually each name must be single character.
     * @see         argparser::ap_defkey::short_name
     */
    std::unordered_set<char> short_names;

    /**
     * @brief       The tokens that end the collection of this entry, i.e. an
     * extra constraint on where the positional arguments stop. An empty set
     * means the collection is limited only by
     * `argparser::ap_entry::positional_range`.
     * @note        Used when the entry is a positional; options and flags do
     * not consult it.
     * @note        Compared verbatim against each positional argument, and
     * never stored in `argparser::ap_entry::content`.
     * @see         argparser::ap_defkey::barrier
     */
    std::unordered_set<std::string> barriers;

    /**
     * @brief       The general parsing result of this entry. Defaults to
     * `argparser::ap_result::unparsed`, which an entry keeps until it has been
     * evaluated.
     * @see         argparser::ap_result
     * @see         argparser::ap_instance::evaluate
     */
    ap_result general_result = ap_result::unparsed;
    /**
     * @brief       The content fetched from the cmdline. For an
     * `argparser::ap_type::positional` entry it holds the positional
     * arguments, for an `argparser::ap_type::option` entry the option values,
     * and for an `argparser::ap_type::flag` entry one name per occurrence, in
     * command line order, recording which name was actually used. Names are
     * stored without the leading dashes, so `-vv --verbose` yields
     * `{"v", "v", "verbose"}` and `-vV` yields `{"v", "V"}`.
     * @see         argparser::ap_instance::evaluate
     */
    std::vector<std::string> content;
    /**
     * @brief       The number of times the flag occurred on the cmdline.
     * Unused for the other types. Defaults to 0.
     * @note        For a flag this always equals `content.size()`, since every
     * occurrence also appends its name to `content`.
     * @see         argparser::ap_entry::content
     * @see         argparser::ap_instance::evaluate
     */
    unsigned repetition = 0;

    /**
     * @brief       The constructor. An entry is to define how an argument
     * works. This constructor is typically invoked when constructing an
     * `argparser::ap_instance`.
     * @param       _name           The name of the entry.
     * @param       _type           The type of the argument.
     * @param       definitions     The definitions of how the argument works,
     * applied in order. See `argparser::ap_defkey` for which keys may be
     * repeated.
     * @throws      std::runtime_error If a definition value is invalid, such
     * as an empty long name, a short name that is not a single character, an
     * empty barrier, or a count of zero.
     * @code{.cpp}
     * using namespace argparser;
     * ap_entry example(
     *          "verbose",
     *          ap_type::flag,
     *          {
     *              {ap_defkey::short_name,"v,V"},
     *              {ap_defkey::long_name,"verbose"}
     *          });
     * @endcode
     */
    ap_entry(const std::string &_name, ap_type _type,
             const std::vector<std::pair<ap_defkey, std::string>> &definitions);
};

/**
 * @brief       Instance, to organize the entries and the cmdline arguments
 * together, and to provide some parsing methods and entry management methods.
 * @note        The sequence of the entries is the order of the evaluation.
 * @note        Call `pre_parse` before `evaluate`, so that the arguments are
 * prepared and the names of the active entries are registered.
 */
class ap_instance
{
  private:
    /**
     * @brief       The arguments generated by the preparser. Updates after
     * calling the preparser.
     * @see         argparser::ap_instance::pre_parse
     */
    std::list<std::pair<std::string, ap_arg_style>> args;
    /**
     * @brief       The entries.
     * @warning     No two entries may share a name.
     * @warning     The sequence of the entries stands for the order of the
     * evaluation.
     */
    std::vector<ap_entry> entries;
    /**
     * @brief       All the short names of the active entries, for the
     * preparser. Rebuilt by `argparser::ap_instance::pre_parse`.
     */
    // std::unordered_set<char> short_options;
    std::unordered_map<char, bool> short_options;
    /**
     * @brief       All the long names of the active entries, for the
     * preparser. Rebuilt by `argparser::ap_instance::pre_parse`.
     */
    std::unordered_set<std::string> long_options;
    /**
     * @brief       To map an entry name to the index in
     * `argparser::ap_instance::entries`. An entry whose name is not recorded
     * here is inactive: it is neither evaluated nor recognized by the
     * preparser.
     */
    std::unordered_map<std::string, std::size_t> name_mapper;

  public:
    /**
     * @brief       Constructs an instance from the given entries.
     * @param       _entries        The entries, added one by one. Their order
     * is the order of the evaluation.
     * @throws      std::runtime_error If two entries have the same name.
     * @code{.cpp}
     * // app -v -o out.txt in.txt
     * // clang-format off
     * ap_instance app(
     *     {
     *         ap_entry(
     *             "verbose",
     *             ap_type::flag,
     *             {
     *                 {ap_defkey::short_name,"v,V"},
     *                 {ap_defkey::long_name,"verbose"}
     *             }
     *         ),
     *         ap_entry(
     *             "output",
     *             ap_type::option,
     *             {
     *                 {ap_defkey::short_name,"o"},
     *                 {ap_defkey::long_name,"output"},
     *                 {ap_defkey::min_value_count,"1"},
     *                 {ap_defkey::max_value_count,"1"}
     *             }
     *         ),
     *         ap_entry(
     *             "input",
     *             ap_type::positional,
     *             {
     *                 {ap_defkey::min_positional_count,"1"}
     *             }
     *         )
     *     });
     * // clang-format on
     * app.pre_parse(argc, argv);
     * app.evaluate();
     * if (app.fine() == false)
     *     for (auto &it : app.errors())
     *         std::cerr << it.first << '\t' << describe(it.second) <<
     * std::endl;
     * @endcode
     */
    ap_instance(const std::vector<ap_entry> &_entries);
    /**
     * @brief       Adds an entry. Its names are recognized from the next
     * `pre_parse` on.
     * @param       entry           The entry to add.
     * @param       index           The position to insert at. Defaults to
     * `SIZE_MAX`, which appends the entry.
     * @throws      std::runtime_error If an entry named `entry.name` already
     * exists.
     * @warning     `index` must not exceed the number of entries. No bounds
     * check is performed, so an out-of-range `index` is undefined behaviour.
     * @note        The entries after `index` shift back by one position.
     */
    void add_entry(const ap_entry &entry, std::size_t index = SIZE_MAX);
    /**
     * @brief       Deactivates an entry, so that the preparser no longer
     * recognizes its names from the next `pre_parse` on.
     * @param       index           The position of the entry.
     * @param       clear_storage   Whether to erase the entry itself as well.
     * Defaults to false, which keeps the entry and its parsing results in
     * place, though it is not evaluated any more.
     * @throws      std::runtime_error If `index` is out of range.
     * @note        The deactivated entry is still accessible by its position,
     * but its name is no longer mapped, so `entry(name)` and `get_index`
     * cannot find it any more.
     * @note        Deactivated entries are skipped by `evaluate`, `fine` and
     * `errors`, so a failed result it kept from an earlier round no longer
     * affects the outcome.
     * @see         add_entry
     */
    void delete_entry(std::size_t index, bool clear_storage = false);
    /**
     * @brief       Replaces an existing entry with `entry`, matched by name.
     * The names of `entry` are recognized from the next `pre_parse` on.
     * @param       entry           The new entry.
     * @throws      std::runtime_error If no entry named `entry.name` has been
     * added yet.
     * @note        The parsing results (`general_result`, `content` and
     * `repetition`) are replaced as well, not preserved.
     * @see         add_entry
     */
    void update_entry(const ap_entry &entry);
    /**
     * @brief       Evaluates a single entry against the preparsed arguments,
     * and fills its `general_result`, `content` and `repetition`.
     * @param       entry           The entry to evaluate. The matched arguments
     * are consumed if its `consume` is true.
     * @throws      std::runtime_error If `entry.type` is invalid.
     * @note        The previous results of `entry` are not cleared, so
     * `content` and `repetition` grow on every call. Use `reset_results` to
     * start from scratch.
     * @note        Call `pre_parse` first, otherwise there is nothing to
     * evaluate.
     */
    void evaluate_entry(ap_entry &entry);
    /**
     * @brief       Splits `argv` into arguments, and marks each one with an
     * `argparser::ap_arg_style`.
     * @param       argc            The argument count.
     * @param       argv            The argument vector. `argv[0]` is recorded
     * as `argparser::ap_arg_style::argv_zero`.
     * @note        This overload only wraps
     * `pre_parse(const std::vector<std::string> &)`, which does the work.
     * @note        Every argument after `--` is marked as
     * `argparser::ap_arg_style::simple`.
     * @note        Replaces the arguments prepared by the previous call, and
     * rebuilds the name registries from the active entries.
     * @warning     `argc` must be at least 1, and `argv[0]` must be a valid
     * string, since it is recorded as `argparser::ap_arg_style::argv_zero`.
     * @see         argparser::ap_instance::pre_parse
     */
    void pre_parse(int argc, char **argv);
    /**
     * @brief       Splits `argv` into arguments, and marks each one with an
     * `argparser::ap_arg_style`.
     * @param       argv            The arguments. `argv[0]` is recorded as
     * `argparser::ap_arg_style::argv_zero`.
     * @note        Every argument after `--` is marked as
     * `argparser::ap_arg_style::simple`.
     * @note        Replaces the arguments prepared by the previous call, and
     * rebuilds the name registries from the active entries.
     * @warning     `argv` must not be empty, since `argv[0]` is recorded as
     * `argparser::ap_arg_style::argv_zero`.
     * @see         argparser::ap_instance::pre_parse
     */
    void pre_parse(const std::vector<std::string> &argv);
    /**
     * @brief       Evaluates the active entries of a given group, in order.
     * @param       group           The group to evaluate.
     * @note        Entries of other groups are left untouched.
     * @note        The evaluation order within the group is the entry order,
     * and the arguments matched by one entry are no longer available to the
     * next one if its `consume` is true.
     */
    void evaluate(const std::string &group);
    /**
     * @brief       Evaluates all the active entries, in order.
     * @note        The evaluation order is the entry order, and the arguments
     * matched by one entry are no longer available to the next one if its
     * `consume` is true.
     * @note        The previous results are not cleared before evaluating. Use
     * `reset_results` and `pre_parse` to start a new round.
     */
    void evaluate();
    /**
     * @brief       Resets the parsing results of all the entries.
     * @note        Clears `content`, sets `repetition` to 0 and
     * `general_result` back to `argparser::ap_result::unparsed`, so that every
     * entry starts from scratch again.
     * @note        The preparsed arguments are left untouched. Use `pre_parse`
     * to replace them.
     */
    void reset_results();
    /**
     * @brief       Tells whether the parsing succeeded.
     * @param       ignore_redundant Whether to ignore unconsumed arguments.
     * Defaults to true. When false, any argument left unconsumed (except
     * `argv[0]`) makes the result false.
     * @return      true if every entry resulted in
     * `argparser::ap_result::success` or `argparser::ap_result::unparsed`, and
     * no argument is left unconsumed when `ignore_redundant` is false.
     * @note        Deactivated entries are ignored, together with the results
     * they kept from earlier rounds.
     * @see         errors
     */
    bool fine(bool ignore_redundant = true);
    /**
     * @brief       Tells whether the parsing of a given group succeeded.
     * @param       group           The group to check.
     * @return      true if every entry in `group` resulted in
     * `argparser::ap_result::success` or `argparser::ap_result::unparsed`.
     * @note        Unconsumed arguments are not checked.
     * @see         argparser::ap_entry::group
     */
    bool fine(const std::string &group);
    /**
     * @brief       Lists the errors of the last evaluation.
     * @return      The name of each failed entry paired with its result, plus
     * one `argparser::ap_result::invalid_arguments` entry keyed by the first
     * unconsumed argument, if there is any.
     * @note        Only failed entries are listed; use `fine` for a plain
     * success check.
     * @note        Deactivated entries are not listed, even if they kept a
     * failed result from an earlier round.
     * @note        The strings are copies, so the result stays valid after
     * further evaluations.
     * @see         argparser::describe
     */
    std::vector<std::pair<std::string, ap_result>> errors();
    /**
     * @brief       Gets an entry by its name.
     * @param       name            The name of the entry.
     * @return      A reference to the entry.
     * @throws      std::out_of_range If no entry has this name.
     * @see         get_index
     */
    const ap_entry &entry(const std::string &name);
    /**
     * @brief       Gets an entry by its position.
     * @param       index           The position of the entry.
     * @return      A reference to the entry.
     * @warning     `index` must be less than the number of entries. No bounds
     * check is performed, so an out-of-range `index` is undefined behaviour.
     * @see         get_index
     */
    const ap_entry &entry(std::size_t index);
    /**
     * @brief       Gets the position of an entry.
     * @param       name            The name of the entry.
     * @return      The position of the entry in the container.
     * @throws      std::out_of_range If no entry has this name.
     * @see         argparser::ap_instance::entry
     */
    std::size_t get_index(const std::string &name);
    /**
     * @brief       Gets the program name.
     * @return      `argv[0]`, as recorded by the last `pre_parse`.
     * @warning     Calling this before the first `pre_parse` is undefined
     * behaviour.
     */
    const std::string_view argv_zero();
};

} // namespace argparser

#endif
