#!/usr/bin/env python3
"""Checks the behaviour of the script parser.

Compiles the embedded C++ harness against the library sources and runs it. The
process exits with the harness exit code: 0 when every check passes.

    python3 tests/check.py

Set CXX to choose the compiler (default: c++).
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INCLUDE = ROOT / "include"
SOURCES = [ROOT / "src" / "argparser.cpp", ROOT / "src" / "ap_script.cpp"]
STRICT_FLAGS = ["-Wall", "-Wextra", "-Werror"]

HARNESS = r'''
#include <ap_script.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace argparser;

namespace
{
int checks = 0;
int failures = 0;

void check(const std::string &name, bool ok, const std::string &detail = "")
{
    ++checks;
    if (ok)
        std::cout << "[PASS] " << name << '\n';
    else
    {
        ++failures;
        std::cout << "[FAIL] " << name
                  << (detail.empty() ? "" : " -> " + detail) << '\n';
    }
}

std::string exception_kind(const std::exception &error)
{
    if (dynamic_cast<const std::invalid_argument *>(&error))
        return "invalid_argument";
    if (dynamic_cast<const std::runtime_error *>(&error))
        return "runtime_error";
    if (dynamic_cast<const std::logic_error *>(&error))
        return "logic_error";
    return "exception";
}

// Every script error must be a runtime_error whose message names the block.
void expect_error(const std::string &name, const std::string &script,
                  const std::string &needle)
{
    try
    {
        parse_script(script);
        check(name, false, "did not throw");
    }
    catch (const std::exception &error)
    {
        const std::string kind = exception_kind(error);
        const std::string what = error.what();
        check(name,
              kind == "runtime_error" && what.rfind("argparser: ", 0) == 0 &&
                  what.find(needle) != std::string::npos,
              kind + ": " + what);
    }
}

std::string first_explanation(const std::string &script)
{
    const auto entries = parse_script(script);
    if (entries.empty() || entries[0].explanations.empty())
        return "<missing>";
    return entries[0].explanations[0];
}

const std::string cli_script = R"({
 entry_name=verbose;
 entry_type=flag;
 short_name=v,V;
 long_name=verbose;
 explanations=plain text with spaces;
 max_repetition=3;
}
{
 entry_name=output;
 entry_type=option;
 short_name=o;
 long_name=output;
 min_value_count=1;
 max_value_count=1;
}
{
 entry_name=files;
 entry_type=positional;
})";

void test_grammar()
{
    const auto entries = parse_script(cli_script);
    check("grammar: one entry per block", entries.size() == 3,
          std::to_string(entries.size()));
    check("grammar: names and types",
          entries.size() == 3 && entries[0].name == "verbose" &&
              entries[0].type == ap_type::flag &&
              entries[1].name == "output" &&
              entries[1].type == ap_type::option &&
              entries[2].type == ap_type::positional);
    check("grammar: names and counts",
          entries.size() == 3 && entries[0].short_names.count('v') == 1 &&
              entries[0].short_names.count('V') == 1 &&
              entries[0].long_names.count("verbose") == 1 &&
              entries[0].max_repetition == 3 &&
              entries[1].valcnt_range.first == 1 &&
              entries[1].valcnt_range.second == 1);
    check("grammar: unquoted value keeps its spaces",
          entries.size() == 3 && entries[0].explanations.size() == 1 &&
              entries[0].explanations[0] == "plain text with spaces");
    check("grammar: blocks back to back",
          parse_script("{entry_name=a;entry_type=flag;}"
                       "{entry_name=b;entry_type=flag;}")
                  .size() == 2);
    check("grammar: duplicate entry_name keeps the last one",
          parse_script("{entry_name=a;entry_name=b;entry_type=flag;}")
                  .at(0)
                  .name == "b");
}

void test_quote_free_values()
{
    // There is no quote syntax: '"' is an ordinary character.
    check("values: quotes are literal",
          first_explanation(
              R"({entry_name=a;entry_type=option;explanations="a b"})") ==
              "\"a b\"");
    check("values: unbalanced quote is literal",
          first_explanation(
              R"({entry_name=a;entry_type=option;explanations=oops"})") ==
              "oops\"");
    check("values: only the first '=' splits",
          first_explanation(
              "{entry_name=a;entry_type=option;explanations=x=1=y}") ==
              "x=1=y");
    check("values: '{' is literal",
          first_explanation(
              "{entry_name=a;entry_type=option;explanations=a{b}") == "a{b");
    check("values: spaces around '=' and ';' are trimmed",
          parse_script("{ entry_name = a ; entry_type = option ; "
                       "long_name = out ; }")
              .at(0)
              .long_names.count("out") == 1);

    // ';' and '}' still terminate a value.
    expect_error("values: ';' ends the statement",
                 "{entry_name=a;entry_type=option;explanations=a;b}",
                 "entry #0: \"b\" is not a definition key.");
    expect_error("values: '}' ends the block",
                 "{entry_name=a;entry_type=option;explanations=a}b}",
                 "expected '{'");
}

void test_whitespace()
{
    check("whitespace: trailing space after ';'",
          parse_script("{entry_name=a;entry_type=flag; }").size() == 1);
    check("whitespace: leading space",
          parse_script("  \n\t{entry_name=a;entry_type=flag;}").size() == 1);
    check("whitespace: whitespace-only script",
          parse_script("  \n\t ").empty());
    check("whitespace: empty script", parse_script("").empty());
    check("whitespace: CRLF block",
          parse_script("{\r\nentry_name=a;\r\nentry_type=flag;\r\n}").size() ==
              1);
}

void test_errors()
{
    expect_error("errors: empty entry_name",
                 "{entry_name=;entry_type=flag;}",
                 "entry #0: \"entry_name\" must not be empty.");
    expect_error("errors: empty entry_type", "{entry_name=a;entry_type=;}",
                 "entry #0: \"entry_type\" is empty; expected");
    expect_error("errors: unknown entry_type",
                 "{entry_name=a;entry_type=opton;}",
                 "entry #0: unknown entry type \"opton\"");
    expect_error("errors: missing entry_name", "{entry_type=flag;}",
                 "entry #0: missing \"entry_name\".");
    expect_error("errors: missing entry_type", "{entry_name=a;}",
                 "entry #0: missing \"entry_type\".");
    expect_error("errors: empty block", "{}",
                 "entry #0: missing \"entry_name\".");
    expect_error("errors: unknown definition key",
                 "{entry_name=a;entry_type=flag;}"
                 "{entry_name=b;entry_type=flag;oops=1;}",
                 "entry #1: \"oops\" is not a definition key.");
    expect_error("errors: statement without a key",
                 "{entry_name=a;entry_type=flag;=1;}",
                 "entry #0: a statement is missing its key before '='.");
    expect_error("errors: garbage before the first block",
                 "x{entry_name=a;entry_type=flag;}",
                 "entry #0: expected '{'");
    expect_error("errors: unclosed block", "{entry_name=a;entry_type=flag;",
                 "entry #0: block opened at offset 0 is missing its closing "
                 "'}'.");
    expect_error("errors: nested braces",
                 "{{entry_name=a;entry_type=flag;}}", "entry #1: expected '{'");
    // Raised by ap_entry, which names the entry and the problem.
    expect_error("errors: invalid value for an empty-valued key",
                 "{entry_name=a;entry_type=flag;no_consume=1;}",
                 "no_consume is not assignable");
}

void test_definition_keys()
{
    struct KeyCase
    {
        ap_defkey key;
        const char *assignment;
    };
    const KeyCase keys[] = {
        {ap_defkey::group, "group=g"},
        {ap_defkey::explanations, "explanations=e"},
        {ap_defkey::no_consume, "no_consume="},
        {ap_defkey::long_name, "long_name=ln"},
        {ap_defkey::short_name, "short_name=s"},
        {ap_defkey::min_positional_count, "min_positional_count=1"},
        {ap_defkey::max_positional_count, "max_positional_count=1"},
        {ap_defkey::barrier, "barrier=|"},
        {ap_defkey::no_variadic, "no_variadic="},
        {ap_defkey::min_value_count, "min_value_count=1"},
        {ap_defkey::max_value_count, "max_value_count=1"},
        {ap_defkey::max_repetition, "max_repetition=1"},
    };
    for (const auto &item : keys)
    {
        bool ok = false;
        try
        {
            const auto entries = parse_script(
                std::string("{entry_name=e;entry_type=option;") +
                item.assignment + "}");
            const ap_entry &entry = entries.at(0);
            switch (item.key)
            {
            case ap_defkey::group:
                ok = entry.group.count("g") == 1;
                break;
            case ap_defkey::explanations:
                ok = entry.explanations.size() == 1 &&
                     entry.explanations[0] == "e";
                break;
            case ap_defkey::no_consume:
                ok = !entry.consume;
                break;
            case ap_defkey::long_name:
                ok = entry.long_names.count("ln") == 1;
                break;
            case ap_defkey::short_name:
                ok = entry.short_names.count('s') == 1;
                break;
            case ap_defkey::min_positional_count:
                ok = entry.positional_range.first == 1;
                break;
            case ap_defkey::max_positional_count:
                ok = entry.positional_range.second == 1;
                break;
            case ap_defkey::barrier:
                ok = entry.barriers.count("|") == 1;
                break;
            case ap_defkey::no_variadic:
                ok = entry.no_variadic;
                break;
            case ap_defkey::min_value_count:
                ok = entry.valcnt_range.first == 1;
                break;
            case ap_defkey::max_value_count:
                ok = entry.valcnt_range.second == 1;
                break;
            case ap_defkey::max_repetition:
                ok = entry.max_repetition == 1;
                break;
            }
        }
        catch (const std::exception &error)
        {
            std::cout << "  (" << item.assignment << ": " << error.what()
                      << ")\n";
        }
        check(std::string("defkey: ") + std::string(defkey_to_string(item.key)),
              ok);
    }
}

void test_bijection()
{
    for (const auto &item : ap_defkey_names)
    {
        const auto back = to_defkey(item.first);
        check(std::string("bijection: ") + std::string(item.first),
              defkey_to_string(item.second) == item.first && back &&
                  *back == item.second);
    }
    check("bijection: unknown name", !to_defkey("ap_script").has_value());
}

// The example printed in docs/USAGE.md section 13 must keep working.
void test_guide_example()
{
    const std::string script = R"({
  entry_name = output;
  entry_type = option;
  short_name = o;
  long_name = output,out;
  explanations = where to write the result;
  min_value_count = 1;
  max_value_count = 1;
}
{
  entry_name = verbose;
  entry_type = flag;
  short_name = v,V;
  long_name = verbose;
  no_consume = ;
})";
    const auto entries = parse_script(script);
    check("guide: two entries", entries.size() == 2,
          std::to_string(entries.size()));
    check("guide: option entry",
          entries.size() == 2 && entries[0].name == "output" &&
              entries[0].type == ap_type::option &&
              entries[0].short_names.count('o') == 1 &&
              entries[0].long_names.count("output") == 1 &&
              entries[0].long_names.count("out") == 1 &&
              entries[0].explanations.size() == 1 &&
              entries[0].explanations[0] == "where to write the result");
    check("guide: flag entry",
          entries.size() == 2 && entries[1].name == "verbose" &&
              entries[1].type == ap_type::flag &&
              entries[1].short_names.count('v') == 1 &&
              entries[1].short_names.count('V') == 1 && !entries[1].consume);
}

void test_integration()
{
    ap_instance command(parse_script(cli_script));
    const std::vector<std::string> argv = {"prog", "-vv", "--output=out.txt",
                                           "a.txt", "b.txt"};
    command.pre_parse(argv);
    command.evaluate();
    check("integration: flag repetition",
          command.entry("verbose").repetition == 2);
    check("integration: option value",
          command.entry("output").content.size() == 1 &&
              command.entry("output").content[0] == "out.txt");
    check("integration: positionals",
          command.entry("files").content.size() == 2);
    check("integration: no leftovers", command.fine(false));
}
void test_forced_simple()
{
    // A word after '--' stays literal even when a name matches it.
    ap_instance app({
        ap_entry("verbose", ap_type::flag,
                 {{ap_defkey::long_name, "verbose"}}),
        ap_entry("files", ap_type::positional, {}),
    });
    app.pre_parse({"app", "--verbose", "--", "--verbose", "-v"});
    app.evaluate();
    check("forced_simple: option before the separator matches",
          app.entry("verbose").repetition == 1,
          std::to_string(app.entry("verbose").repetition));
    check("forced_simple: words after the separator stay literal",
          app.entry("files").content ==
              std::vector<std::string>{"--verbose", "-v"},
          std::to_string(app.entry("files").content.size()));
    check("forced_simple: no leftovers", app.fine(false));

    // A post-separator word is still a value for a preceding option.
    ap_instance value(
        {ap_entry("out", ap_type::option, {{ap_defkey::short_name, "o"}})});
    value.pre_parse({"app", "-o", "--", "a"});
    value.evaluate();
    check("forced_simple: option still takes a post-separator value",
          value.entry("out").content == std::vector<std::string>{"a"});

    // reparse() must not retokenize words taken after '--'.
    ap_instance dynamic({ap_entry("files", ap_type::positional, {})});
    dynamic.pre_parse({"app", "--", "--late"});
    dynamic.add_entry(ap_entry("late", ap_type::flag,
                               {{ap_defkey::long_name, "late"}}));
    dynamic.reparse();
    dynamic.evaluate();
    check("forced_simple: reparse leaves post-separator words literal",
          dynamic.entry("late").repetition == 0 &&
              dynamic.entry("files").content ==
                  std::vector<std::string>{"--late"});

    // A word before '--' is still re-examined by reparse().
    ap_instance early({ap_entry("files", ap_type::positional, {})});
    early.pre_parse({"app", "--early"});
    early.add_entry(ap_entry("early", ap_type::flag,
                             {{ap_defkey::long_name, "early"}}));
    early.reparse();
    early.evaluate();
    check("forced_simple: reparse still recognizes words before the separator",
          early.entry("early").repetition == 1);
}
} // namespace

int main()
{
    test_grammar();
    test_guide_example();
    test_quote_free_values();
    test_whitespace();
    test_errors();
    test_definition_keys();
    test_bijection();
    test_integration();
    test_forced_simple();
    std::cout << "\nchecks=" << checks << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
'''


def run(command: list[str]) -> int:
    print("$", " ".join(command), flush=True)
    try:
        return subprocess.run(command, timeout=300).returncode
    except subprocess.TimeoutExpired:
        print("error: command timed out", file=sys.stderr)
        return 1


def build(cxx: str, source: Path, binary: Path) -> int:
    """Compiles the harness with strict flags and the library with defaults."""
    objects = []
    for unit in [source, *SOURCES]:
        obj = binary.parent / (unit.stem + ".o")
        flags = STRICT_FLAGS if unit == source else []
        code = run(
            [cxx, "-std=c++17", *flags, "-I", str(INCLUDE), "-c", str(unit),
             "-o", str(obj)]
        )
        if code != 0:
            return code
        objects.append(str(obj))
    return run([cxx, *objects, "-o", str(binary)])


def main() -> int:
    cxx = os.environ.get("CXX", "c++")
    with tempfile.TemporaryDirectory(prefix="argparser-check-") as directory:
        source = Path(directory) / "check.cpp"
        binary = Path(directory) / "check"
        source.write_text(HARNESS, encoding="utf-8")
        try:
            code = build(cxx, source, binary)
        except FileNotFoundError:
            print(f"error: compiler not found: {cxx}", file=sys.stderr)
            return 1
        if code != 0:
            return code
        try:
            return run([str(binary)])
        except FileNotFoundError:
            print("error: the test binary is missing", file=sys.stderr)
            return 1


if __name__ == "__main__":
    sys.exit(main())
