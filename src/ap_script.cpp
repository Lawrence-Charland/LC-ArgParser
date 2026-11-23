#include "ap_script.hpp"
#include "argparser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace argparser
{
namespace
{
void trim_str(std::string &target)
{
    auto is_not_space = [](unsigned char c) { return !std::isspace(c); };
    target.erase(target.begin(),
                 std::find_if(target.begin(), target.end(), is_not_space));
    target.erase(
        std::find_if(target.rbegin(), target.rend(), is_not_space).base(),
        target.end());
}
[[noreturn]] void fail(const std::string &what)
{
    throw std::runtime_error("argparser: " + what);
}

std::pair<std::string, std::string> get_equation(const std::string &target)
{
    const std::size_t pos = target.find('=');
    if (pos == std::string::npos)
        return {target, ""};
    std::pair<std::string, std::string> result = {target.substr(0, pos),
                                                  target.substr(pos + 1)};
    trim_str(result.first), trim_str(result.second);
    return result;
}

std::vector<std::string> parse_braces(const std::string &script)
{
    std::vector<std::string> result;
    std::size_t i = 0;
    while (i < script.size())
    {
        if (std::isspace(static_cast<unsigned char>(script.at(i))))
        {
            ++i;
            continue;
        }
        const std::string where = "entry #" + std::to_string(result.size());
        if (script.at(i) != '{')
            fail(where + ": expected '{' at offset " + std::to_string(i) +
                 ", found '" + script.at(i) + "'.");

        const std::size_t begin = i + 1;
        const std::size_t close = script.find('}', begin);
        if (close == std::string::npos)
            fail(where + ": block opened at offset " + std::to_string(i) +
                 " is missing its closing '}'.");

        result.emplace_back(script, begin, close - begin);
        i = close + 1;
    }
    return result;
}

std::vector<std::string> separate(const std::string &values, char separator)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= values.size())
    {
        const std::size_t pos = values.find(separator, start);
        std::string member = values.substr(
            start, pos == std::string::npos ? std::string::npos : pos - start);
        trim_str(member);
        if (!member.empty())
            result.emplace_back(std::move(member));
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }
    return result;
}
} // namespace
std::vector<ap_entry> parse_script(const std::string &script)
{
    const std::vector<std::string> entries = parse_braces(script);
    std::vector<ap_entry> result;
    result.reserve(entries.size());
    for (std::size_t index = 0; index < entries.size(); index++)
    {
        const std::string where = "entry #" + std::to_string(index);
        const std::vector<std::string> sentences =
            separate(entries[index], ';');
        std::string entry_name;
        ap_type entry_type = ap_type::positional;
        bool has_entry_name = false, has_entry_type = false;
        std::vector<std::pair<ap_defkey, std::string>> def_key_values;
        for (const auto &sentence : sentences)
        {
            const auto parsed = get_equation(sentence);
            if (parsed.first.empty())
                fail(where + ": a statement is missing its key before '='.");
            if (parsed.first == "entry_name")
            {
                if (parsed.second.empty())
                    fail(where + ": \"entry_name\" must not be empty.");
                has_entry_name = true, entry_name = parsed.second;
            }
            else if (parsed.first == "entry_type")
            {
                if (parsed.second.empty())
                    fail(where +
                         ": \"entry_type\" is empty; expected \"positional\", "
                         "\"option\" or \"flag\".");
                has_entry_type = true;
                if (parsed.second == "flag")
                    entry_type = ap_type::flag;
                else if (parsed.second == "option")
                    entry_type = ap_type::option;
                else if (parsed.second == "positional")
                    entry_type = ap_type::positional;
                else
                    fail(where + ": unknown entry type \"" + parsed.second +
                         "\"; expected \"positional\", \"option\" or "
                         "\"flag\".");
            }
            else
            {
                const auto find_defkey = to_defkey(parsed.first);
                if (!find_defkey)
                    fail(where + ": \"" + parsed.first +
                         "\" is not a definition key.");
                def_key_values.emplace_back(find_defkey.value(), parsed.second);
            }
        }
        if (!has_entry_name)
            fail(where + ": missing \"entry_name\".");
        if (!has_entry_type)
            fail(where + ": missing \"entry_type\".");
        result.emplace_back(entry_name, entry_type, def_key_values);
    }
    return result;
}
} // namespace argparser

// Partially assisted by DeepSeek