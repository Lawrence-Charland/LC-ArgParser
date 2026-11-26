// SPDX-License-Identifier: MIT
/**
 * @file        argparser.cpp
 * @brief       A command line argument parser for GNU and UNIX styles.
 * Belongs to LC Projects. This is the source file.
 * @author      Lawrence Charland
 * @copyright   Copyright (c) 2026 Lawrence-Charland. Released under the MIT
 * License; see the `LICENSE` file for the full text.
 */

#include <stdexcept>

#include <argparser.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace argparser
{

/**
 * @defgroup    internal_helpers Internal helpers
 * @{
 */

/**
 * @brief       Splits `values` at every occurrence of `separator`.
 * @param       values          The string to split.
 * @param       separator       The separator character.
 * @return      The split parts. An empty `values` yields one empty part.
 */
static std::vector<std::string> separate(const std::string &values,
                                         char separator)
{
    if (values.empty())
        return {""};
    std::vector<std::string> result;
    std::size_t start = 0;
    while (true)
    {
        std::size_t pos = values.find(separator, start);
        if (pos == std::string::npos)
        {
            result.emplace_back(values.substr(start));
            break;
        }
        result.emplace_back(values.substr(start, pos - start));
        start = pos + 1;
    }
    return result;
}

static std::pair<std::string, std::string> equation(const std::string &value)
{
    std::size_t breakpt = value.find('=');
    if (breakpt == std::string::npos)
        return {value, ""};
    else
        return {value.substr(0, breakpt), value.substr(breakpt + 1)};
}

static bool is_literal(ap_arg_style style)
{
    return style == ap_arg_style::simple ||
           style == ap_arg_style::forced_simple;
}

/**
 * @}
 */

/**
 * @defgroup   entry_definition_parser Entry definition parser
 * @{
 */

ap_entry::ap_entry(
    const std::string &_name, ap_type _type,
    const std::vector<std::pair<ap_defkey, std::string>> &definitions)
    : name(_name), type(_type)
{
    std::vector<std::string> tmpvec;
    int tmpint;
    try
    {
        for (auto &it : definitions)
        {
            switch (it.first)
            {
            case ap_defkey::group:
                group.insert(it.second);
                break;
            case ap_defkey::explanations:
                explanations.push_back(it.second);
                break;
            case ap_defkey::no_consume:
                if (it.second.empty() == false)
                    throw std::runtime_error("no_consume is not assignable. "
                                             "Please leave the value empty.");
                consume = false;
                break;
            case ap_defkey::long_name:
                tmpvec = separate(it.second, ',');
                for (auto &_it : tmpvec)
                    if (_it.empty() == true)
                        throw std::runtime_error("long names cannot be empty.");
                long_names.insert(tmpvec.begin(), tmpvec.end());
                break;
            case ap_defkey::short_name:
                tmpvec = separate(it.second, ',');
                for (auto &_it : tmpvec)
                    if (_it.length() != 1)
                        throw std::runtime_error(
                            "short names must be single characters.");
                    else if (_it == "-")
                        throw std::runtime_error(
                            "short names cannot be \'-\'.");
                    else
                        short_names.insert(_it[0]);
                break;
            case ap_defkey::min_positional_count:
                tmpint = std::stoi(it.second);
                if (tmpint < 0)
                    throw std::runtime_error(
                        "min_positional_count must be >=0.");
                positional_range.first = static_cast<unsigned>(tmpint);
                break;
            case ap_defkey::max_positional_count:
                positional_range.second = std::stoi(it.second);
                if (positional_range.second == 0)
                    throw std::runtime_error(
                        "max_positional_count cannot be 0.");
                break;
            case argparser::ap_defkey::barrier:
                if (it.second.empty())
                    throw std::runtime_error("barrier cannot be empty");
                barriers.insert(it.second);
                break;
            case argparser::ap_defkey::no_variadic:
                if (it.second.empty() == false)
                    throw std::runtime_error("no_variadic is not assignable. "
                                             "Please leave the value empty.");
                no_variadic = true;
                break;
            case ap_defkey::min_value_count:
                tmpint = std::stoi(it.second);
                if (tmpint < 0)
                    throw std::runtime_error("min_value_count must be >=0.");
                valcnt_range.first = static_cast<unsigned>(tmpint);
                break;
            case ap_defkey::max_value_count:
                valcnt_range.second = std::stoi(it.second);
                if (valcnt_range.second == 0)
                    throw std::runtime_error("max_value_count cannot be 0.");
                break;
            case ap_defkey::max_repetition:
                max_repetition = std::stoi(it.second);
                if (max_repetition == 0)
                    throw std::runtime_error("max_repetition cannot be 0.");
                break;
            default:
                throw std::runtime_error("Invalid ap_defkey.");
            }
        }
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(
            "argparser: Error occurred while parsing definition of \'" + name +
            "\' :\t" + e.what());
    }
}

/**
 * @}
 */

/**
 * @defgroup    instance Instance
 * @{
 */

void ap_instance::add_entry(const ap_entry &entry, std::size_t index)
{
    if (index == SIZE_MAX)
        index = entries.size();
    if (name_mapper.find(entry.name) != name_mapper.end())
        throw std::runtime_error("The entry named \'" + entry.name +
                                 "\' already exists.");
    entries.insert(entries.begin() + index, entry);
    // Every entry at or after the insertion point has shifted back by one.
    for (auto &it : name_mapper)
        if (it.second >= index)
            ++it.second;
    name_mapper.emplace(entry.name, index);
}

void ap_instance::delete_entry(std::size_t index, bool clear_storage)
{
    if (index >= entries.size())
        throw std::runtime_error("No such index in entries.");
    name_mapper.erase(entries[index].name);
    if (clear_storage == true)
    {
        entries.erase(entries.begin() + index);
        // Every entry after the erased one has shifted forward by one.
        for (auto &it : name_mapper)
            if (it.second > index)
                --it.second;
    }
}

void ap_instance::update_entry(const ap_entry &entry)
{
    auto found = name_mapper.find(entry.name);
    if (found == name_mapper.end())
        throw std::runtime_error(
            "Entry named \'" + entry.name +
            "\' is not added yet. Did you mean \'add_entry\'?");
    entries[found->second] = entry;
}

ap_instance::ap_instance(const std::vector<ap_entry> &_entries)
{
    for (auto &it : _entries)
        add_entry(it);
}

void ap_instance::pre_parse(int argc, char **argv)
{
    pre_parse(std::vector<std::string>(argv, argv + argc));
}

void ap_instance::pre_parse(const std::vector<std::string> &argv)
{
    build_registries();

    // Parse the cmdline
    args.clear();
    args.emplace_back(argv[0], argv[0], ap_arg_style::argv_zero);
    for (std::size_t i = 1; i < argv.size(); i++)
    {
        if (argv[i] == "--") // Separator
        {
            // Everything past the separator is taken literally and must stay
            // that way through `reparse()`.
            for (i++; i < argv.size(); i++)
                args.emplace_back(argv[i], argv[i],
                                  ap_arg_style::forced_simple);
            break;
        }
        for (auto &it : tokenize(argv[i]))
            args.push_back(std::move(it));
    }
}

void ap_instance::build_registries()
{
    short_options.clear();
    long_options.clear();
    for (std::size_t i = 0; i < entries.size(); i++)
    {
        auto found = name_mapper.find(entries[i].name);
        if (found == name_mapper.end()) // Inactive entry
            continue;
        found->second = i; // Keep the index in step after insertions/erasures
        switch (entries[i].type)
        {
        case ap_type::flag:
            for (auto &it : entries[i].short_names)
                short_options.emplace(it, false);
            break;
        case ap_type::option:
            for (auto &it : entries[i].short_names)
                short_options.emplace(it, true);
            break;
        default:;
        }
        long_options.insert(entries[i].long_names.begin(),
                            entries[i].long_names.end());
    }
}

std::vector<ap_arg_token> ap_instance::tokenize(const std::string &word) const
{
    std::vector<ap_arg_token> tokens;
    if (word.find("--") == 0) // Double-dash
    {
        if (long_options.find(equation(word.substr(2)).first) !=
            long_options.end())
            // Preparse if it's a long option
            tokens.emplace_back(word, word.substr(2),
                                ap_arg_style::double_dash);
        else // Keep still otherwise
            tokens.emplace_back(word, word, ap_arg_style::simple);
    }
    else if (word[0] == '-' && word.length() >= 2)
    {
        if (short_options.find(word[1]) == short_options.end())
            // Keep still if it's not a short option
            tokens.emplace_back(word, word, ap_arg_style::simple);
        else
            for (std::size_t i = 1; i < word.length(); i++)
                if (short_options.find(word[i]) !=
                    short_options.end()) // Merged
                {
                    tokens.emplace_back(word, std::string{word[i]},
                                        ap_arg_style::single_dash);
                    if (short_options.at(word[i]) == true)
                    {
                        tokens.back().token += "=" + word.substr(i + 1);
                        break;
                    }
                }
                else // Sticky value
                {
                    tokens.back().token += "=" + word.substr(i);
                    break;
                }
    }
    else // Simple
        tokens.emplace_back(word, word, ap_arg_style::simple);
    return tokens;
}

void ap_instance::reparse()
{
    build_registries();
    for (auto token = args.begin(); token != args.end();)
    {
        // `forced_simple` words were taken after `--`; leave them alone.
        if (token->style == ap_arg_style::simple)
        {
            auto retokenize = tokenize(token->token);
            token = args.erase(token);
            token = args.insert(token, retokenize.begin(), retokenize.end());
            for (auto &it : retokenize)
                token++;
        }
        else
            token++;
    }
}

void ap_instance::evaluate_entry(ap_entry &entry)
{
    std::vector<std::list<ap_arg_token>::iterator> consumption;
    std::pair<std::string, std::string> tmppair;
    unsigned counter = 0;
    switch (entry.type)
    {
    case ap_type::flag:
        for (auto it = args.begin(); it != args.end(); it++)
        {
            if (it->style != ap_arg_style::single_dash &&
                it->style != ap_arg_style::double_dash)
                // Must be -xxxx or --xxx
                continue;
            tmppair = equation(it->token);
            if ((it->style == ap_arg_style::single_dash &&
                 entry.short_names.find(tmppair.first[0]) !=
                     entry.short_names.end()) ||
                (it->style == ap_arg_style::double_dash &&
                 entry.long_names.find(tmppair.first) !=
                     entry.long_names.end()))
            {
                if (tmppair.second.empty() == false) // xxx=yyy is not allowed
                    entry.general_result = ap_result::flag_unexpected_values;
                entry.repetition++, entry.content.push_back(tmppair.first),
                    consumption.push_back(it);
            }
        }
        if (entry.max_repetition >= 0 &&
            entry.repetition > entry.max_repetition)
            entry.general_result = ap_result::flag_too_many_repetition;
        break;
    case ap_type::option:
        for (auto it = args.begin(); it != args.end(); it++)
        {
            if (it->style != ap_arg_style::single_dash &&
                it->style != ap_arg_style::double_dash)
                // Must be -xxx or --xxx
                continue;
            tmppair = equation(it->token);
            if ((it->style == ap_arg_style::single_dash &&
                 entry.short_names.find(tmppair.first[0]) !=
                     entry.short_names.end()) ||
                (it->style == ap_arg_style::double_dash &&
                 entry.long_names.find(tmppair.first) !=
                     entry.long_names.end()))
            {
                consumption.push_back(it);
                if (entry.valcnt_range.second >= 0 &&
                    counter >= entry.valcnt_range.second)
                    // Too much values (redundant keys)
                    entry.general_result = ap_result::option_too_many_values;
                else if (tmppair.second.empty() == false) // xxx=yyy
                    entry.content.push_back(tmppair.second), counter++;
                else // xxx y1 y2 ... yn [next-key]
                {
                    for (it++; it != args.end(); it++)
                    {
                        if (!is_literal(it->style) ||
                            counter == entry.valcnt_range.second)
                        { // Boundary
                            it--;
                            break;
                        }
                        entry.content.push_back(it->token),
                            consumption.push_back(it), counter++;
                        if (entry.no_variadic)
                            break;
                    }
                }
            }
            if (it == args.end())
                break;
        }
        if (counter < entry.valcnt_range.first) // Too few values
            entry.general_result = ap_result::option_too_few_values;
        break;
    case ap_type::positional:
    {
        auto it = args.begin();
        bool barrier_hit = false;
        for (; it != args.end() &&
               counter < static_cast<unsigned>(entry.positional_range.second);
             it++)
            if (is_literal(it->style))
            {
                if (entry.barriers.find(it->token) != entry.barriers.end())
                {
                    consumption.push_back(it);
                    barrier_hit = true;
                    break;
                }
                else
                    entry.content.push_back(it->token),
                        consumption.push_back(it), counter++;
            }
        // The count limit may have stopped the collection before any barrier
        // was met. Keep looking for the next barrier, and consume at most one
        // per entry.
        if (barrier_hit == false)
            for (; it != args.end(); it++)
                if (is_literal(it->style) &&
                    entry.barriers.find(it->token) != entry.barriers.end())
                {
                    consumption.push_back(it);
                    break;
                }
        if (counter < entry.positional_range.first) // Too few positionals
            entry.general_result = ap_result::positional_too_few_args;
        break;
    }
    default:
        throw std::runtime_error("Invalid ap_type");
    }
    if (entry.general_result == ap_result::unparsed)
        entry.general_result = ap_result::success;
    if (entry.consume == true)
        for (auto &it : consumption)
            args.erase(it);
}

void ap_instance::evaluate(const std::string &group)
{
    for (auto &it : entries)
        if (name_mapper.find(it.name) != name_mapper.end() &&
            it.group.find(group) != it.group.end())
            evaluate_entry(it);
}

void ap_instance::evaluate()
{
    for (auto &it : entries)
        if (name_mapper.find(it.name) != name_mapper.end())
            evaluate_entry(it);
}

void ap_instance::reset_results()
{
    for (auto &it : entries)
    {
        it.general_result = ap_result::unparsed;
        it.content.clear();
        it.repetition = 0;
    }
}

/**
 * @}
 */

/**
 * @defgroup    inquiry Inquiry
 * @{
 */

bool ap_instance::fine(bool ignore_redundant)
{
    if (args.size() > 1 && ignore_redundant == false) // Redundant arguments
        return false;
    for (auto &it : entries)
        if (name_mapper.find(it.name) != name_mapper.end() &&
            it.general_result != ap_result::success &&
            it.general_result != ap_result::unparsed)
            return false;
    return true;
}

bool ap_instance::fine(const std::string &group)
{
    for (auto &it : entries)
        if (name_mapper.find(it.name) != name_mapper.end() &&
            it.group.find(group) != it.group.end() &&
            it.general_result != ap_result::success &&
            it.general_result != ap_result::unparsed)
            return false;
    return true;
}

std::vector<std::pair<std::string, ap_result>> ap_instance::errors()
{
    std::vector<std::pair<std::string, ap_result>> result;
    for (auto &it : entries)
        if (name_mapper.find(it.name) != name_mapper.end() &&
            it.general_result != ap_result::success &&
            it.general_result != ap_result::unparsed)
            result.emplace_back(it.name, it.general_result);
    if (args.size() > 1) // Redundant arguments
        result.emplace_back((++args.begin())->pretty_name,
                            ap_result::invalid_arguments);
    return result;
}

const ap_entry &ap_instance::entry(const std::string &name)
{
    return entries[name_mapper.at(name)];
}

const ap_entry &ap_instance::entry(std::size_t index)
{
    return entries[index];
}

ap_entry &ap_instance::entry_mutable(const std::string &name)
{
    return entries[name_mapper.at(name)];
}

ap_entry &ap_instance::entry_mutable(std::size_t index)
{
    return entries[index];
}

std::size_t ap_instance::get_index(const std::string &name)
{
    return name_mapper.at(name);
}

const std::string_view ap_instance::argv_zero()
{
    return args.begin()->token;
}

std::unordered_set<std::string> ap_instance::ls_options()
{
    build_registries();
    std::unordered_set<std::string> result;
    for (auto &look : short_options)
        result.insert(std::string("-") + look.first);
    for (auto &look : long_options)
        result.insert("--" + look);
    return result;
}

/**
 * @}
 */

} // namespace argparser
