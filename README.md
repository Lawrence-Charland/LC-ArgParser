# argparser

A small command-line argument parser for GNU/UNIX-style options, written in C++17.

English | [简体中文](./README.zh_CN.md)

> **This is a personal practice project and its first release.**
> It was written for learning and everyday use. It has not been audited, fuzzed,
> or battle-tested in production, so expect rough edges and treat the behaviour
> as work in progress.
>
> **I do not intend to do explicit version management.**
> There is no semantic versioning promise, no release tags, and no changelog.
> The `VERSION 0.1.0` in `CMakeLists.txt` is only a nominal value that CMake
> needs for the shared library's `VERSION`/`SOVERSION`; it is not a
> compatibility contract. The API may change at any time — pin a commit if you
> need something stable.

## Features

- Entry-based description of the command line: `positional`, `option`, `flag`.
- GNU/UNIX-style syntax:
  - long names: `--verbose`, `--output=out.txt`, `--output out.txt`
  - short names: `-v`, bundled `-vv`, mixed `-vo out.txt`
  - `--` terminates option parsing
- Several long/short names per entry (`{"v,V"}` registers both).
- Group-based dispatch for subcommands: `evaluate("subA")`, `fine("subA")`.
- Validation: minimum/maximum positional count, minimum/maximum option value
  count, maximum flag repetition.
- Barriers for positionals: a positional entry stops collecting at a registered
  token such as `|` or `END`; see [Barriers](#barriers).
- Results are stored on the entries themselves (`general_result`, `content`,
  `repetition`), so no separate result object is needed. `content` serves all
  three types; see [What `content` holds](#what-content-holds).
- Flags are recorded by name, not merely counted, so aliases such as `-v` and
  `--verbose` stay distinguishable.
- Replaceable error messages: `describe()` plus overridable `AP_DESCRIBE_*`
  macros.
- Doxygen comments with usage snippets in the header.

## Requirements

- A C++17 compiler (only GCC has been used so far).
- CMake 3.10+ (optional — `argparser.cpp` and a driver of your own can also be
  compiled directly).

## Project layout

```text
argparser/
├── include/argparser.hpp   public header: declarations + Doxygen comments
├── src/argparser.cpp       implementation
├── src/main.cpp            scratch driver for manual testing (not part of the API)
├── CMakeLists.txt
└── CMakePresets.json       "linux-gcc-debug" / "linux-gcc-release" presets
```

`main.cpp` is a test case, not an example: a throwaway program used to exercise
the parser by hand. It may be rewritten, extended or removed at any time, so do
not build tooling or expectations on it. The snippets in this README and in the
header are the documentation; the demo binary is not.

## Build and run

The commands below build the demo target `app`. In your own project, replace the
target and source list with your own; the library itself is not tied to
`main.cpp`.

With the bundled CMake preset:

```sh
cmake --preset linux-gcc-release
cmake --build build/linux-gcc-release --target app
./build/linux-gcc-release/app -A 1 2 -B 3
```

`linux-gcc-debug` is the Debug counterpart. Both presets build into
`build/<preset>` (see `CMakePresets.json`).

With plain CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target app
./build/app -A 1 2 -B 3
```

Without CMake:

```sh
g++ -std=c++17 -Iinclude src/main.cpp src/argparser.cpp -o app
```

The CMake build produces a shared library target `argparser` and, when this
directory is the top-level project, the demo executable `app`. The demo is added
only in that case; pulled in with `add_subdirectory`, the project contributes
just the library target. `cmake --install` installs the library together with
the headers, and the library target exports `include/` as a public include
directory, so a consumer links `argparser` and includes `<argparser.hpp>`.

## Quick start

```cpp
#include <iostream>
#include <argparser.hpp>

using namespace argparser;

int main(int argc, char **argv)
{
    ap_instance app({
        ap_entry("verbose", ap_type::flag,
                 {{ap_defkey::short_name, "v,V"},
                  {ap_defkey::long_name, "verbose"}}),
        ap_entry("output", ap_type::option,
                 {{ap_defkey::short_name, "o"},
                  {ap_defkey::long_name, "output"},
                  {ap_defkey::max_value_count, "1"}}),
        ap_entry("input", ap_type::positional,
                 {{ap_defkey::min_positional_count, "1"}}),
    });

    app.pre_parse(argc, argv);
    app.evaluate();

    if (app.fine() == false)
    {
        for (auto &err : app.errors())
            std::cerr << err.first << ": " << describe(err.second) << '\n';
        return 1;
    }

    std::cout << "verbose: " << app.entry("verbose").repetition << '\n';
    if (app.entry("output").content.empty() == false)
        std::cout << "output: " << app.entry("output").content.front() << '\n';
    for (auto &name : app.entry("input").content)
        std::cout << "input: " << name << '\n';
}
```

```text
$ ./app -vv --output=out.txt a.txt b.txt
verbose: 2
output: out.txt
input: a.txt
input: b.txt

$ ./app
input: Not enough positional arguments
```

## How parsing works

1. `pre_parse(argc, argv)` copies `argv` and forwards it to
   `pre_parse(const std::vector<std::string> &)`, which marks every token with
   an `ap_arg_style` (`argv_zero`, `simple`, `single_dash`, `double_dash`) and
   rebuilds the name registries from the active entries. Use the vector
   overload when the arguments do not come from `main`:

   ```cpp
   app.pre_parse({"app", "-v", "--output=out.txt", "a.txt"});
   ```

2. `evaluate()` or `evaluate(group)` walks the active entries in order and
   matches tokens. Matched tokens are consumed (removed) unless the entry's
   `consume` is `false`.
3. `fine()` and `errors()` report the outcome.

## Subcommands via groups

Every entry has a `group` (default: `"default"`). Evaluate only the group that
applies to the chosen subcommand:

```cpp
app.pre_parse(argc, argv);
app.evaluate("head");
if (app.fine("head") == false)
    return 2;
if (app.entry("head").content[0] == "subA")
{
    app.evaluate("subA");
    if (app.fine("subA") == false)
        return 1;
}
```

The demo driver [main.cpp](./main.cpp) is only a test case, so the command line
it accepts will change. It does not dispatch on groups right now: it defines two
options, `-A/--arrA` and `-B/--arrB`, each taking one to five values, and both
are required because `min_value_count` is `1`:

```text
./app -A 1 2 -B 3               # exit 0
./app --arrA=7 --arrB=8         # exit 0
./app -A 1 2 3 4 5 6 -B 9       # exit 1 (max_value_count is 5)
./app -A 1 2                    # exit 1 (-B is absent, so it reports
                                #         option_too_few_values)
```

## Supported syntax at a glance

| Input                | Interpretation                                    |
| -------------------- | ------------------------------------------------- |
| `-v`                 | short flag                                        |
| `-vv`                | flag given twice (bundled short options)          |
| `-vo out.txt`        | `-v`, then `-o` taking `out.txt` as its value     |
| `-o out.txt`         | short option with a separate value                |
| `-ofile.txt`         | short option with a sticky value                  |
| `--verbose`          | long flag                                         |
| `--output=out.txt`   | long option with `=value`                         |
| `--output out.txt`   | long option with a separate value                 |
| `--`                 | end of options; everything after it is positional |
| `-`                  | positional value (a lone dash is not an option)   |

## Barriers

A positional entry may register one or more *barriers* with
`ap_defkey::barrier`. The value is the token text, compared literally against
each positional argument; repeat the key to register several tokens. When the
entry reaches a barrier, it stops collecting.

A positional is otherwise bounded by its count range alone, so a barrier is how
an entry keeps from swallowing everything that follows: it is an extra
constraint on where the collection ends.

The barrier token is **not** stored in `content`. It is consumed together with
the collected positionals (unless `consume` is `false`), so the following
entries never see it, while the arguments after it are left untouched:

```cpp
ap_instance app({ap_entry("files", ap_type::positional,
                          {{ap_defkey::barrier, "|"}}),
                 ap_entry("rest", ap_type::positional, {})});
app.pre_parse({"app", "a.txt", "b.txt", "|", "c.txt"});
app.evaluate();
// files.content == {"a.txt", "b.txt"}
// rest.content  == {"c.txt"}
```

Details worth knowing:

- Barriers exist for positionals. `option` and `flag` entries do not use them.
- An entry consumes at most one barrier. Collecting stops at the first one it
  meets; when the count limit ends the collection before that, the entry keeps
  scanning the remaining arguments and consumes the next barrier it finds,
  leaving everything after it to the following entries.
- Option values are collected greedily, so an argument right after a consumed
  barrier can still end up as a value of an earlier option; order the entries
  accordingly.
- A barrier may be the first token: the entry then collects nothing and reports
  `positional_too_few_args` if `min_positional_count` is greater than `0`. The
  barrier is consumed anyway when `consume` is `true`.
- The value is literal: unlike `long_name` and `short_name`, it is not split at
  `,`.
- An empty barrier is rejected: `{ap_defkey::barrier, ""}` makes the
  constructor throw `std::runtime_error`.
- `--` cannot serve as a barrier, because the preparser removes it before the
  entries are evaluated. Every argument after `--` is `simple`, so a barrier may
  look like an option there, e.g. `-x`.
- Leftover arguments after a barrier are simply unconsumed: `fine()` still
  returns `true` by default, while `fine(false)` and `errors()` report them.

## What `content` holds

`content` is a single `std::vector<std::string>` and it is the payload of the
parsed entry. Its meaning is decided by the entry's `type`, not by the field
itself:

| `type`       | What `content` holds                                              |
| ------------ | ----------------------------------------------------------------- |
| `positional` | the positional arguments, in command-line order                   |
| `option`     | the values collected for the option                               |
| `flag`       | one name per occurrence, without the leading dashes               |

The flag case deserves a note, because there `content` records names rather than
values. `repetition` counts the occurrences, while `content` keeps one name per
occurrence, in command-line order:

```cpp
const ap_entry &verbose = app.entry("verbose");
// with "./app -vV --verbose --verbose":
// verbose.repetition == 4
// verbose.content    == {"v", "V", "verbose", "verbose"}
```

So `-v`, `-V` and `--verbose` remain distinguishable even though they all belong
to the same entry, and `repetition` is always `content.size()`. Even
`--verbose=x` records the name, while additionally reporting
`flag_unexpected_values`.

## API at a glance

`ap_type` — the kind of argument an entry describes:

| Value        | Meaning                                              |
| ------------ | ---------------------------------------------------- |
| `positional` | identified by position, not by a name or prefix      |
| `option`     | `-x` / `--xxx` that takes values                     |
| `flag`       | `-x` / `--xxx` that takes no value; counted, and each used name is kept in `content` |

`ap_defkey` — keys used to define an entry through the `ap_entry` constructor:

| Key                        | Value                                                        |
| -------------------------- | ------------------------------------------------------------ |
| `group`                    | group name, default `"default"`                              |
| `explanations`             | description text, default empty                              |
| `no_consume`               | empty value; leave matched arguments to the next entry       |
| `long_name`                | one or more names separated by `,`                           |
| `short_name`               | one or more single characters separated by `,`               |
| `min_positional_count`     | integer `>= 0`, default `0`                                  |
| `max_positional_count`     | non-zero integer, negative or absent = unlimited, default `-1` |
| `barrier`                  | literal token that ends the positional collection; repeatable |
| `min_value_count`          | integer `>= 0`, default `0`                                  |
| `max_value_count`          | non-zero integer, negative or absent = unlimited, default `-1` |
| `max_repetition`           | non-zero integer, negative or absent = unlimited, default `-1` |

`ap_entry` — one argument definition plus its parsing results:

| Member                             | Role                                    |
| ---------------------------------- | --------------------------------------- |
| `name`, `group`, `explanations`    | identity and help text                  |
| `type`, `consume`                  | behaviour                               |
| `positional_range`, `valcnt_range`, `max_repetition` | validation limits  |
| `long_names`, `short_names`        | accepted names                          |
| `barriers`                         | tokens that end the positional collection (see [Barriers](#barriers)) |
| `general_result`                   | `ap_result`, defaults to `unparsed`     |
| `content`                          | parsing payload; its meaning depends on `type` (see [above](#what-content-holds)) |
| `repetition`                       | how many times a flag occurred          |

`ap_instance` — owns the entries and the preparsed arguments:

| Member                                        | Role                                             |
| --------------------------------------------- | ------------------------------------------------ |
| `ap_instance(entries)`                        | construct from a list of entries                 |
| `add_entry(entry, index = SIZE_MAX)`          | insert an entry (append by default)              |
| `delete_entry(index, clear_storage = false)`  | deactivate an entry                              |
| `update_entry(entry)`                         | replace an entry, matched by name                |
| `evaluate_entry(entry)`                       | evaluate one entry directly                      |
| `pre_parse(argc, argv)` / `pre_parse(tokens)` | split and mark the arguments                     |
| `evaluate(group)` / `evaluate()`              | evaluate one group / all active entries          |
| `reset_results()`                             | clear `content`, `repetition`, `general_result`  |
| `fine(ignore_redundant = true)` / `fine(group)` | success check                                  |
| `errors()`                                    | failed entries plus leftover arguments            |
| `entry(name)` / `entry(index)`                | access an entry                                  |
| `get_index(name)`                             | position of an entry                             |
| `argv_zero()`                                 | `argv[0]` as recorded by the last `pre_parse`    |

`ap_result` — `success`, `unparsed`, `positional_too_few_args`,
`option_too_few_values`, `option_too_many_values`, `flag_too_many_repetition`,
`flag_unexpected_values`, `invalid_arguments`. `describe(result)` turns any of
them into a message.

`ap_arg_style` is exposed by `pre_parse`, but it is an internal detail for most
users.

## Error handling

`describe()` maps a result to text. Any message can be replaced by defining the
matching macro before including the header:

```cpp
#define AP_DESCRIBE_INVALID_ARGUMENTS "unexpected argument"
#include <argparser.hpp>
```

Available macros: `AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS`,
`AP_DESCRIBE_OPTION_TOO_FEW_VALUES`, `AP_DESCRIBE_OPTION_TOO_MANY_VALUES`,
`AP_DESCRIBE_FLAG_TOO_MANY_REPETITION`, `AP_DESCRIBE_FLAG_UNEXPECTED_VALUES`,
`AP_DESCRIBE_INVALID_ARGUMENTS`, `AP_DESCRIBE_UNKNOWN`.

`success` and `unparsed` are not errors and are described by
`AP_DESCRIBE_UNKNOWN`.

## Caveats and known behaviour

- **Everything is plain text.** Count limits only decide how many tokens are
  collected; convert the values yourself.
- **Unrecognized tokens stay `simple`** and can be swallowed by a greedy
  positional entry: `./app -q a.txt` yields `input: -q`, `input: a.txt`, and
  `fine()` still returns `true`. Use `fine(false)` if leftover arguments should
  be reported as `invalid_arguments`.
- **Sticky short values are split greedily.** `-ofile.txt` gives `file.txt`,
  but `-oout.txt` gives `ut.txt`, because the second `o` is itself a registered
  short name. Prefer `-o value` or `--output=value`.
- **`min_value_count >= 1` makes an option required:** when the option is
  absent, the entry reports `option_too_few_values`.
- **Call `pre_parse` before `evaluate`.** `argv_zero()` before the first
  `pre_parse` is undefined behaviour.
- **`evaluate` does not clear previous results.** `content` and `repetition`
  accumulate; call `reset_results()` and `pre_parse` again to start over.
- **`entry(index)` performs no bounds check** — an out-of-range index is
  undefined behaviour.
- **Deactivated entries** (`delete_entry`) are skipped by `evaluate`, `fine`
  and `errors`, and can no longer be looked up by name, so a failed result kept
  from an earlier round no longer affects the outcome.

## Documentation

The API reference lives in the Doxygen comments of
[include/argparser.hpp](./include/argparser.hpp), including constructor
snippets. To generate HTML, run Doxygen with `INPUT = include` and
`EXTRACT_ALL = YES` (or point your own Doxyfile at the header).

## License

MIT License — see [LICENSE](./LICENSE).

Copyright (c) 2026 Lawrence-Charland.

In short: you may use, copy, modify, merge, publish, distribute, sublicense and
sell this software, as long as the copyright notice and this permission notice
are included. The software is provided "as is", without warranty of any kind.

The MIT License applies to the project as a whole and to every commit; it is not
tied to any version number.

## Author

Lawrence Charland — LC Projects. Personal practice project, first release.
