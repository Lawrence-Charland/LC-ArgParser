#include <iostream>

#include <argparser.hpp>

using namespace argparser;

int main(int argc, char **argv)
{
    // clang-format off
    ap_instance cmd({
        ap_entry(
            "arrA",
            ap_type::option,
            {
            {ap_defkey::explanations,"arrA (--arrA or -A)"},
            {ap_defkey::short_name,"A"},
            {ap_defkey::long_name,"arrA"},
            {ap_defkey::min_value_count,"1"},
            {ap_defkey::max_value_count,"5"},
            }
        ),
        ap_entry(
            "arrB",
            ap_type::option,
            {
            {ap_defkey::explanations,"arrB (--arrB or -A)"},
            {ap_defkey::short_name,"B"},
            {ap_defkey::long_name,"arrB"},
            {ap_defkey::min_value_count,"1"},
            {ap_defkey::max_value_count,"5"},
            }
        )
    });
    // clang-format on
    cmd.pre_parse(argc, argv);
    cmd.evaluate();
    if (!cmd.fine(false))
        return 1;
    return 0;
}
