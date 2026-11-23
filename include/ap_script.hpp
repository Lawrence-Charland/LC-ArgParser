#ifndef LCPROJECTS_ARGPARSER_AP_SCRIPT
#define LCPROJECTS_ARGPARSER_AP_SCRIPT

#include "argparser.hpp"

#include <vector>
namespace argparser
{
std::vector<ap_entry> parse_script(const std::string &script);
} // namespace argparser

#endif