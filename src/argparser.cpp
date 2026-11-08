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

/**
 * @brief       Splits `value` at the first `=`.
 * @param       value           The string to split.
 * @return      The part before `=` and the part after it. The second part is
 * empty when `value` contains no `=`.
 */
static std::pair<std::string, std::string> equation(const std::string &value)
{
    std::size_t breakpt = value.find('=');
    if (breakpt == std::string::npos)
        return {value, ""};
    else
        return {value.substr(0, breakpt), value.substr(breakpt + 1)};
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
                group = it.second;
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
    // Derive the registries from the entries, so that the entries stay the
    // single source of truth for the names.
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

    // Parse the cmdline
    args.clear();
    args.emplace_back(argv[0], ap_arg_style::argv_zero);
    std::string tmp;
    std::pair<std::string, std::string> tmppair;
    for (std::size_t i = 1; i < argv.size(); i++)
    {
        tmp = argv[i];
        if (tmp == "--") // Separator
            for (i++; i < argv.size(); i++)
                args.emplace_back(argv[i], ap_arg_style::simple);
        else if (tmp.find("--") == 0) // Double-dash
        {
            tmppair = equation(tmp.substr(2));
            if (long_options.find(tmppair.first) != long_options.end())
                // Preparse if it's a long option
                args.emplace_back(tmp.substr(2), ap_arg_style::double_dash);
            else // Keep still otherwise
                args.emplace_back(tmp, ap_arg_style::simple);
        }
        else if (tmp[0] == '-' && tmp.length() >= 2)
        {
            if (short_options.find(tmp[1]) == short_options.end())
                // Keep still if it's not a short option
                args.emplace_back(tmp, ap_arg_style::simple);
            else
            {
                for (std::size_t i = 1; i < tmp.length(); i++)
                    if (short_options.find(tmp[i]) !=
                        short_options.end()) // Merged
                    {
                        args.emplace_back(std::string{tmp[i]},
                                          ap_arg_style::single_dash);
                        if (short_options.at(tmp[i]) == true)
                        {
                            args.back().first += "=" + tmp.substr(i + 1);
                            break;
                        }
                    }
                    else // Sticky value
                    {
                        args.back().first += "=" + tmp.substr(i);
                        break;
                    }
            }
        }
        else // Simple
            args.emplace_back(tmp, ap_arg_style::simple);
    }
}

void ap_instance::evaluate_entry(ap_entry &entry)
{
    std::vector<std::list<std::pair<std::string, ap_arg_style>>::iterator>
        consumption;
    std::pair<std::string, std::string> tmppair;
    unsigned counter = 0;
    switch (entry.type)
    {
    case ap_type::flag:
        for (auto it = args.begin(); it != args.end(); it++)
        {
            if (it->second != ap_arg_style::single_dash &&
                it->second != ap_arg_style::double_dash)
                // Must be -xxxx or --xxx
                continue;
            tmppair = equation(it->first);
            if ((it->second == ap_arg_style::single_dash &&
                 entry.short_names.find(tmppair.first[0]) !=
                     entry.short_names.end()) ||
                (it->second == ap_arg_style::double_dash &&
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
            if (it->second != ap_arg_style::single_dash &&
                it->second != ap_arg_style::double_dash)
                // Must be -xxx or --xxx
                continue;
            tmppair = equation(it->first);
            if ((it->second == ap_arg_style::single_dash &&
                 entry.short_names.find(tmppair.first[0]) !=
                     entry.short_names.end()) ||
                (it->second == ap_arg_style::double_dash &&
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
                        if (it->second != ap_arg_style::simple ||
                            counter == entry.valcnt_range.second)
                        { // Boundary
                            it--;
                            break;
                        }
                        entry.content.push_back(it->first),
                            consumption.push_back(it), counter++;
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
        for (auto it = args.begin();
             it != args.end() && counter < entry.positional_range.second; it++)
            if (it->second == ap_arg_style::simple)
                entry.content.push_back(it->first), consumption.push_back(it),
                    counter++;
        if (counter < entry.positional_range.first) // Too few positionals
            entry.general_result = ap_result::positional_too_few_args;
        break;
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
        if (name_mapper.find(it.name) != name_mapper.end() && it.group == group)
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
            it.group == group && it.general_result != ap_result::success &&
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
        result.emplace_back((++args.begin())->first,
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

std::size_t ap_instance::get_index(const std::string &name)
{
    return name_mapper.at(name);
}

const std::string_view ap_instance::argv_zero()
{
    return args.begin()->first;
}

/**
 * @}
 */

} // namespace argparser
