# argparser usage guide

English | [简体中文](./USAGE.zh_CN.md)

`argparser` is a small C++17 library for GNU/UNIX-style command lines. The
public header only declares the API; this guide is the reference for how it
behaves.

- Include `<argparser.hpp>` and link the `argparser` library (or compile
  `src/argparser.cpp` together with your program).
- Everything lives in the `argparser` namespace.
- All values are plain strings. Limits are counts; the library does not convert
  numbers, paths or enums for you.

## Contents

- [1. Mental model](#1-mental-model)
- [2. Building and linking](#2-building-and-linking)
- [3. Entry types](#3-entry-types)
- [4. Defining an entry](#4-defining-an-entry)
  - [4.1 Definition keys](#41-definition-keys)
  - [4.2 Keys are not type-checked](#42-keys-are-not-type-checked)
- [5. Pre-parsing](#5-pre-parsing)
  - [5.1 Token styles](#51-token-styles)
- [6. Evaluation](#6-evaluation)
  - [6.1 Order, groups and consumption](#61-order-groups-and-consumption)
  - [6.2 Options](#62-options)
  - [6.3 Flags](#63-flags)
  - [6.4 Positionals and barriers](#64-positionals-and-barriers)
  - [6.5 Interaction between entries](#65-interaction-between-entries)
  - [6.6 Recommended multi-phase parsing](#66-recommended-multi-phase-parsing)
- [7. Results](#7-results)
  - [7.1 `ap_result`](#71-ap_result)
  - [7.2 Success checks and leftovers](#72-success-checks-and-leftovers)
  - [7.3 Starting a new round](#73-starting-a-new-round)
- [8. Entry management](#8-entry-management)
- [9. Custom error messages](#9-custom-error-messages)
- [10. Caveats](#10-caveats)
- [11. Small recipes](#11-small-recipes)
  - [Required option with two values](#required-option-with-two-values)
  - [Optional option](#optional-option)
  - [Repeatable flag with a cap](#repeatable-flag-with-a-cap)
  - [One value per occurrence](#one-value-per-occurrence)
  - [Exclusive groups](#exclusive-groups)
  - [Barrier](#barrier)
- [12. Putting it together: a subcommand parser](#full-example)
- [13. Script language](#13-script-language)

## 1. Mental model

An `ap_instance` owns two things:

- a list of `ap_entry` objects, which describe the accepted arguments;
- a pool of preparsed command-line tokens.

Parsing has three phases:

1. `pre_parse(...)` splits the command line and marks every token with an
   `ap_arg_style`.
2. `evaluate()` or `evaluate(group)` matches the tokens against the active
   entries, in entry order, and writes the outcome into each entry.
3. `fine()` / `errors()` report the result.

There is no separate result object. The results are fields of each `ap_entry`:
`general_result`, `content` and `repetition`.

```cpp
#include <argparser.hpp>
#include <iostream>

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

    app.pre_parse(argc, argv); // or: app.pre_parse({"app", "-v", "file.txt"});
    app.evaluate();

    if (app.fine() == false)
    {
        for (auto &err : app.errors())
            std::cerr << err.first << ": " << describe(err.second) << '\n';
        return 1;
    }
}
```

For a multi-subcommand program,
[section 6.6](#66-recommended-multi-phase-parsing) shows the recommended
layering, and [section 12](#full-example) is the complete program.

## 2. Building and linking

With CMake:

```cmake
add_subdirectory(path/to/argparser)
target_link_libraries(your_target PRIVATE argparser)
```

Without CMake:

```sh
g++ -std=c++17 -Ipath/to/argparser/include \
    main.cpp path/to/argparser/src/argparser.cpp -o app
```

Add `path/to/argparser/src/ap_script.cpp` if you use the script language from
[section 13](#13-script-language); the CMake target already includes it.

An installed copy exposes `<argparser.hpp>` and links as `argparser`. A C++17
compiler is required.

## 3. Entry types

| `ap_type` | Matched by | What `content` holds |
| --- | --- | --- |
| `positional` | position only | the collected positional arguments |
| `option` | `-x` / `--xxx` | the collected values |
| `flag` | `-x` / `--xxx` | one used name per occurrence, without dashes |

`repetition` is maintained for flags and is always `content.size()` there. It is
unused for options and positionals.

## 4. Defining an entry

```cpp
ap_entry(name, type, { {key, value}, ... });
```

- `name` is the identity of the entry inside an instance. Two active entries of
  the same instance may not share a name.
- The definitions are applied in order.
- Every value is a `std::string`, including counts:
  `{ap_defkey::max_value_count, "2"}`, not `2`.
- Invalid definitions make the constructor throw `std::runtime_error`, with the
  entry name in the message:
  `argparser: Error occurred while parsing definition of '<name>' : <reason>`.

The public fields can be edited after construction:

| Field | Meaning |
| --- | --- |
| `name` | entry name |
| `group` | set of groups; starts as `{"default"}` |
| `explanations` | help text lines |
| `type` | `ap_type` |
| `consume` | whether matched tokens are removed from the pool (default `true`) |
| `no_variadic` | whether an option takes at most one value per occurrence (default `false`) |
| `positional_range` | `{min, max}` positional count, default `{0, -1}` |
| `valcnt_range` | `{min, max}` option value count, default `{0, -1}` |
| `max_repetition` | flag repetition limit, default `-1` |
| `long_names`, `short_names` | accepted names, without the leading dashes |
| `barriers` | tokens that stop a positional |
| `general_result` | result written by `evaluate` |
| `content`, `repetition` | payload written by `evaluate` |

### 4.1 Definition keys

| Key | Value | Effect | Repeating the key |
| --- | --- | --- | --- |
| `group` | group name | adds another group membership | adds groups |
| `explanations` | free text | help text | appends |
| `no_consume` | must be `""` | sets `consume = false` | harmless |
| `long_name` | `name` or `a,b` | registers long names, without `--` | appends |
| `short_name` | `c` or `a,b` | registers single-character names, without `-`; `-` is rejected | appends |
| `min_positional_count` | integer `>= 0` | minimum positional count | last wins |
| `max_positional_count` | non-zero integer | maximum positional count; negative = unlimited | last wins |
| `barrier` | literal token | stops a positional collection; not split at `,`; empty is rejected | appends |
| `no_variadic` | must be `""` | an option takes at most one value per occurrence | harmless |
| `min_value_count` | integer `>= 0` | minimum total value count; `> 0` makes the option required | last wins |
| `max_value_count` | non-zero integer | maximum total value count; negative = unlimited | last wins |
| `max_repetition` | non-zero integer | maximum flag occurrences; negative = unlimited | last wins |

Keys map to their own name in both directions. `to_string(key)` returns the key
spelling and `to_defkey(name)` returns a `std::optional<ap_defkey>` (empty when
`name` is not a key). The public `ap_defkey_names` table backs both:

```cpp
for (const auto &[name, key] : ap_defkey_names)
    std::cout << name << '\n'; // group, explanations, no_consume, ...
```

Notes that are easy to get wrong:

- **Option value counts are totals.** `min_value_count` and `max_value_count`
  count values across *all* occurrences of the option, not per occurrence.
  With `max_value_count = 1`, `-o a -o b` is an error even though each
  occurrence has one value.
- **`min_value_count > 0` makes the option required.** If the option is absent,
  its value count is `0`, so the entry reports `option_too_few_values` whenever
  it is evaluated.
- **Every entry starts in `default`.** The `group` key adds memberships; it
  never removes `default`. To put an entry only in another group, assign the
  field after construction:
  `ap_entry e(...); e.group = {"build"};`
- **`explanations` always appends.** Multiple definitions accumulate; clear the
  vector yourself if you need to replace it.
- **Names must not include dashes.** `{ap_defkey::long_name, "--output"}` will
  not match `--output`; use `"output"`. Short names are one character each.
- **Barriers are literal and are not comma-split.** Use one `barrier` key per
  token.
- **Counts are parsed with `std::stoi`.** A non-numeric value throws; negative
  minimums and zero maximums are rejected (zero `max_*` is always invalid).
- **Name validation.** Empty long names, short names that are not exactly one
  character, and the short name `-` are rejected at construction.

### 4.2 Keys are not type-checked

The constructor accepts any key for any `ap_type`. Keys that do not apply are
ignored by evaluation: `max_repetition` on an option, `max_value_count` on a
flag, `barrier` on an option, and so on. Names are an exception: long names are
collected from every active entry, so a `long_name` on a positional still makes
the preparser treat that word as a long token (which the positional can then
never match). Short names are collected only from options and flags. Do not set
names on positionals.

## 5. Pre-parsing

```cpp
void pre_parse(int argc, char **argv);
void pre_parse(const std::vector<std::string> &argv);
```

Both overloads do the same work; the first one wraps the second. `argv[0]` is
recorded as the program name (`ap_arg_style::argv_zero`).

- The vector must not be empty, and `argc` must be at least `1`.
- Each call replaces the previous token pool and rebuilds the name registries
  from the active entries.
- Adding, updating or deleting an entry does not rebuild the name registries;
  the preparser recognizes the new names only from the next `pre_parse` on. A
  deleted entry stops being evaluated immediately, because its name is removed
  from the lookup table.
- The token pool itself (`ap_arg_token`: pretty name, token text, style) is an
  internal detail; `ap_instance` does not expose it.

### 5.1 Token styles

| Style | Meaning |
| --- | --- |
| `argv_zero` | the program name; never matched by an entry |
| `simple` | a positional value, an option value, or an unrecognized word |
| `single_dash` | a short option/flag, a bundled short group, or a sticky value |
| `double_dash` | a long option/flag, optionally with `=value` |

| Input | Resulting tokens (after `argv[0]`) |
| --- | --- |
| `-v` | `single_dash` `v` |
| `-vv` | two `single_dash` tokens, `v` and `v` |
| `-vV` | two `single_dash` tokens, `v` and `V` |
| `-vo out.txt` | `single_dash` `v`, `single_dash` `o`, then `simple` `out.txt` |
| `-ofile.txt` | `single_dash` `o=file.txt` (the rest is the attached value) |
| `--verbose` | `double_dash` `verbose` |
| `--output=out.txt` | `double_dash` `output=out.txt` |
| `--output out.txt` | `double_dash` `output`, then `simple` `out.txt` |
| `--` | dropped; every following word becomes `simple` |
| `-` | `simple` |
| `--unknown` | `simple` `--unknown` if no active entry registered that long name |
| `-q` | `simple` `-q` if `q` is not a registered short name |

Pre-parsing rules worth knowing:

- A long word is a `double_dash` token only when the part before `=` is a
  registered long name of an active entry. Otherwise the whole word stays
  `simple`, so it can be swallowed by an option value or a positional.
- A short word is parsed only when its first character is a registered short
  name. If the first character is unknown, the whole word stays `simple`
  (`-xv` is one simple token).
- In a bundle, every registered character becomes its own token. When a
  character belongs to a value-taking option, the rest of the word is attached
  as its value and the bundle ends (`-vo out.txt` → `v`, then `o`; `-ofile.txt`
  → `o=file.txt`).
- An unknown character after at least one known flag is attached to the
  preceding flag as an unexpected value: with `v` registered as a flag, `-vX`
  becomes `v=X`, so the flag reports `flag_unexpected_values`.
- `--` is removed. It does not prevent an option from taking the next `simple`
  token as a value: with `-o` as a value-taking option, `-o -- a` yields
  `content == {"a"}`. Entry order decides whether a positional can claim such a
  token first.
- A lone `-` is `simple`, so it can be a positional or an option value.

## 6. Evaluation

### 6.1 Order, groups and consumption

- `evaluate()` walks all active entries in entry order.
- `evaluate(group)` walks only the active entries whose `group` set contains
  `group`, still in entry order.
- An entry removes the tokens it matched unless `consume` is `false`. The next
  entry therefore sees only the tokens that are still in the pool.
- Entry order is the evaluation order. A greedy option or positional placed
  early can take tokens that a later entry wanted.
- `evaluate` does not clear previous results. `content` and `repetition` grow
  and `general_result` is overwritten only by new findings.
- Every entry always contains `"default"`. `evaluate("default")` therefore also
  runs entries that added a group with the `group` key. If you dispatch strictly
  by group, assign `group` directly or avoid `evaluate()`/`evaluate("default")`.

```cpp
ap_entry command("command", ap_type::positional,
                 {{ap_defkey::group, "cli"},
                  {ap_defkey::max_positional_count, "1"}});
ap_entry fast("fast", ap_type::flag,
              {{ap_defkey::long_name, "fast"},
               {ap_defkey::group, "build"}});
// command.group == {"default", "cli"}
// fast.group    == {"default", "build"}

app.evaluate("cli");   // evaluates command
app.evaluate("build"); // evaluates fast
```

### 6.2 Options

An option matches a `single_dash` or `double_dash` token whose name is one of
its short or long names. A matched option key is consumed even when it has no
values, unless `consume` is `false`.

Values are collected in two ways:

- **Attached:** `-ovalue` and `--opt=value` produce one value directly.
- **Separate:** after a bare `-o` / `--opt`, every following `simple` token is
  collected until a non-`simple` token or the maximum count is reached.

A short option takes *everything* after its name as the attached value, so
`-o=value` records `"=value"`, not `"value"`. Use `-o value` or `--opt=value`
when the `=` should not become part of the value.

The count checks use the total across all occurrences:

- If the total is already at `max_value_count` when another key matches, the
  entry reports `option_too_many_values`. That key (and anything attached to
  it) is still consumed, but no further values are recorded.
- If the maximum is reached while collecting separate values, collection simply
  stops; the remaining words are left in the pool and cause
  `invalid_arguments` only when leftovers are checked.
- At the end, a total below `min_value_count` reports
  `option_too_few_values`. Because the check runs even when the option never
  occurred, `min_value_count > 0` means "required".

`no_variadic` changes only the per-occurrence collection: each occurrence takes
at most one separate value. Totals still accumulate across occurrences, so the
minimum/maximum rules above are unchanged.

```cpp
ap_entry o("o", ap_type::option,
           {{ap_defkey::short_name, "o"},
            {ap_defkey::long_name, "opt"}});
// -o a b c        -> content {"a", "b", "c"}
// -o a -o b       -> content {"a", "b"}
// -oa --opt=x     -> content {"a", "x"}
```

With `{ap_defkey::no_variadic, ""}`:

```text
-o a b c     -> content {"a"}; b and c stay in the pool
-o a -o b    -> content {"a", "b"}
min=2, -o a b       -> option_too_few_values (total is 1)
min=2, -o a -o b    -> success (total is 2)
max=1, -o a -o b    -> option_too_many_values (total is already 1)
```

### 6.3 Flags

A flag matches the same way as an option, but records occurrences instead of
values:

- `repetition` counts occurrences.
- `content` gets the matched name, without dashes, in command-line order.
- `-v`, `-V` and `--verbose` can all belong to the same entry and remain
  distinguishable in `content`.
- `--verbose=x` records the name and additionally reports
  `flag_unexpected_values`.
- After counting, `repetition > max_repetition` reports
  `flag_too_many_repetition` (a negative limit means unlimited).

```cpp
// with -vv --verbose --verbose:
// repetition == 4
// content    == {"v", "v", "verbose", "verbose"}
```

### 6.4 Positionals and barriers

A positional collects `simple` tokens from the beginning of the pool, in order,
until:

- the maximum count is reached, or
- a registered barrier is met.

Rules:

- Only `simple` tokens are collected. Values consumed by an earlier option are
  not available.
- There is no "too many positionals" error. Excess words stay in the pool.
- If the collected count is below `min_positional_count`, the entry reports
  `positional_too_few_args`.
- A barrier is compared literally with the token text. It is not stored in
  `content`.
- An entry consumes at most one barrier. If the count limit stops collection
  first, the entry scans the remaining tokens and consumes the next barrier it
  finds, without adding it to `content`.
- The barrier token is consumed with the entry unless `consume` is `false`.
- `--` cannot be a barrier (it is removed by pre-parsing). Because everything
  after `--` is `simple`, an option-looking word there can be a barrier.
- `barrier` values are not split at `,`; repeat the key for several tokens.

```cpp
ap_instance app({
    ap_entry("files", ap_type::positional, {{ap_defkey::barrier, "|"}}),
    ap_entry("rest", ap_type::positional, {})});

app.pre_parse({"app", "a.txt", "b.txt", "|", "c.txt"});
app.evaluate();
// files.content == {"a.txt", "b.txt"}
// rest.content  == {"c.txt"}
```

### 6.5 Interaction between entries

Evaluation is greedy and order-dependent:

```text
option o first, then positional p:   -o a b  -> o {"a"}, p {"b"}
positional p first, then option o:   -o a b  -> p {"a", "b"}, o {}
```

`no_consume` makes an entry fill its `content` but leave the tokens in the pool,
so a later entry can match them again. This is powerful and easy to misuse:
the same word may be reported both as a value and as a leftover unless a later
entry consumes it.

### 6.6 Recommended multi-phase parsing

A program with subcommands is easiest to maintain when parsing is split into
explicit layers:

1. Keep all entries in one `def_args()` helper. Put the command word and the
   global flags in a `general` group, and put each subcommand's entries in its
   own group. Add an internal group for switches that should only be parsed in
   a special mode.
2. Call `pre_parse` once.
3. `evaluate("general")`, then check early-exit flags such as `--help` and
   `--version` by their `repetition` **before** validating anything else. They
   should work even when the rest of the command line is wrong.
4. Call `fine()` after the general layer. Leftovers are ignored here because
   they are probably meant for the subcommand.
5. Read the command word from the positional `content`.
6. `evaluate(<subcommand group>)`, then call `fine(false)` to reject leftovers
   at the final layer.
7. Treat flags as booleans (`repetition != 0`) and options/positionals as value
   lists (`content`).

Two rules keep the layers predictable:

- **Evaluate explicit groups, never `evaluate()`.** Every entry also belongs to
  `default`; `evaluate()` would run every subcommand's entries at once.
- **Give every entry an `explanations` line.** The same text can label the
  entry in error messages, so `errors()` output stays readable.

The full program in [section 12](#full-example) follows this flow.

## 7. Results

### 7.1 `ap_result`

| Value | Meaning | Where it comes from |
| --- | --- | --- |
| `unparsed` | not evaluated yet | initial value of `general_result` |
| `success` | nothing went wrong | set at the end of evaluation when the result is still `unparsed` |
| `positional_too_few_args` | positional total below the minimum | positional check |
| `option_too_few_values` | option total below the minimum | option check (also when the option is absent) |
| `option_too_many_values` | another key matched after the maximum was reached | option check |
| `flag_too_many_repetition` | flag total above `max_repetition` | flag check |
| `flag_unexpected_values` | a flag occurrence carried `=value` | flag check |
| `invalid_arguments` | tokens were left unconsumed | synthesized by `errors()`, never stored |

An entry has one `general_result`. If several problems apply, the later check
overwrites the earlier one.

`describe(result)` returns a message for any result. `success` and `unparsed`
are not errors and are described by `AP_DESCRIBE_UNKNOWN`.

### 7.2 Success checks and leftovers

- `fine()` (default `ignore_redundant = true`): `true` when every active entry
  is `success` or `unparsed`. Leftover tokens are ignored.
- `fine(false)`: additionally requires that no token remains after `argv[0]`.
- `fine(group)`: checks only entries in `group`, and ignores leftovers.
- `errors()`: returns every active entry whose result is neither `success` nor
  `unparsed`, plus **one** `invalid_arguments` entry for the first leftover
  token, keyed by its original spelling. It reports only the first leftover, not
  all of them. For a token split out of a short bundle, that spelling is the
  whole bundle as typed. The returned names are copies, so they stay valid
  after later evaluations.

`errors()` is not group-scoped. After `evaluate("build")`, entries from other
groups that kept an error from an earlier evaluation are still listed.

### 7.3 Starting a new round

- `reset_results()` clears `content`, sets `repetition = 0` and
  `general_result = unparsed` for every entry, including inactive ones. These
  are also the initial values of a freshly constructed entry. The token pool is
  not touched.
- `pre_parse(...)` replaces the token pool and rebuilds the name registries.
- A normal new round is `reset_results()` followed by `pre_parse(...)`, then
  `evaluate(...)`.

## 8. Entry management

```cpp
void add_entry(const ap_entry &entry, std::size_t index = SIZE_MAX);
void delete_entry(std::size_t index, bool clear_storage = false);
void update_entry(const ap_entry &entry);

const ap_entry &entry(const std::string &name);
const ap_entry &entry(std::size_t index);
std::size_t get_index(const std::string &name);
```

- `add_entry` appends by default. Passing `index` inserts there and shifts the
  following entries; the name must not belong to an active entry. `index` must
  not exceed the current size (no bounds check, so a larger index is undefined
  behaviour).
- `delete_entry` deactivates the entry: its name is removed from the lookup
  table, so `evaluate`, `fine` and `errors` skip it from then on. With
  `clear_storage = false` the entry and its results stay accessible by index;
  with `true` the entry is erased and later indices shift down. An out-of-range
  index throws `std::runtime_error`.
- `update_entry` replaces the entry with the same name, including its results.
  It throws if the name is not active.
- `entry(name)` and `get_index(name)` throw `std::out_of_range` when the name is
  not active.
- `entry(index)` performs no bounds check; an out-of-range index is undefined
  behaviour.
- Added, updated and deleted names are recognized by the preparser only from
  the next `pre_parse`.
- The `ap_instance(entries)` constructor is equivalent to adding every entry in
  order; it throws when two names collide.
- `evaluate_entry(entry)` evaluates one entry directly against the current pool.
  It fills that entry's results and consumes its matched tokens when `consume`
  is true. Most programs only need `evaluate()` / `evaluate(group)`.

```cpp
// entries is an existing std::vector<ap_entry>
ap_instance app(entries);
app.add_entry(ap_entry("color", ap_type::option,
                       {{ap_defkey::long_name, "color"}}));
app.update_entry(ap_entry("color", ap_type::option,
                          {{ap_defkey::long_name, "colour"}}));
app.delete_entry(app.get_index("color")); // deactivate, keep the storage
```

## 9. Custom error messages

`describe()` uses macros that can be overridden before including the header:

| Macro | Default |
| --- | --- |
| `AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS` | `"Not enough positional arguments"` |
| `AP_DESCRIBE_OPTION_TOO_FEW_VALUES` | `"Not enough values for the option"` |
| `AP_DESCRIBE_OPTION_TOO_MANY_VALUES` | `"Too many values for the option"` |
| `AP_DESCRIBE_FLAG_TOO_MANY_REPETITION` | `"Too many repetitions for the flag"` |
| `AP_DESCRIBE_FLAG_UNEXPECTED_VALUES` | `"Flags do not take values"` |
| `AP_DESCRIBE_INVALID_ARGUMENTS` | `"Unrecognized or redundant argument"` |
| `AP_DESCRIBE_UNKNOWN` | `"Unknown error"` |

```cpp
#define AP_DESCRIBE_INVALID_ARGUMENTS "unexpected argument"
#include <argparser.hpp>
```

Define every macro you replace; the header only provides defaults for macros
that are not already defined.

## 10. Caveats

- **Everything is text.** Count limits only decide how many tokens are
  collected. Convert values yourself.
- **Unknown options can be swallowed.** An unregistered `-q` or `--foo` stays
  `simple`, so a greedy option or positional can consume it:
  `./app -q a.txt` may report `"-q"` as a positional value and still pass
  `fine()`.
- **`--` only changes styles.** An option evaluated before a positional can
  still take a post-`--` token as its value (`-o -- a` → `"a"`).
- **Short bundles have sharp edges.** `-ofile.txt` is an attached value, but
  `-vX` with `X` unregistered becomes a flag occurrence with an unexpected
  value, and `-xv` with unknown `x` is a single `simple` token.
- **`min_value_count > 0` requires the option.** The check runs even when the
  option is absent.
- **Call `pre_parse` first.** `evaluate` and `errors` have nothing meaningful to
  work with before the first `pre_parse`, and `argv_zero()` on an empty pool is
  undefined behaviour.
- **`evaluate` accumulates.** Call `reset_results()` and `pre_parse()` for a
  fresh round.
- **`fine()` ignores leftovers by default.** Use `fine(false)` or `errors()`
  when leftover arguments should fail. `errors()` names only the first one.
- **Groups add membership.** Every entry is in `default`; the `group` key never
  removes it. Assign `group` directly for an exclusive group.
- **Keys are not type-checked.** Irrelevant keys are ignored, and a long name
  on a positional still affects pre-parsing.
- **Deactivated entries** are skipped by `evaluate`, `fine` and `errors`, and
  their names are no longer resolvable, although `entry(index)` still works
  while the storage is kept.
- **References into the entry list can be invalidated.** `add_entry` and
  `delete_entry(..., true)` may reallocate or shift the vector, so do not keep a
  reference from `entry(...)` across those calls.
- **`argv_zero()` is a view.** It points into the token pool and is invalidated
  by the next `pre_parse`.
- **Not thread-safe.** A single `ap_instance` must not be used concurrently
  without external synchronization.
- **Definition errors throw; parse errors do not.** Invalid definitions raise
  `std::runtime_error`; command-line problems are stored in results.

## 11. Small recipes

### Required option with two values

```cpp
ap_entry pair("pair", ap_type::option,
              {{ap_defkey::short_name, "p"},
               {ap_defkey::min_value_count, "2"}});
// -p a b   -> success, content {"a", "b"}
// -p a     -> option_too_few_values
// (absent) -> option_too_few_values
```

### Optional option

```cpp
ap_entry out("out", ap_type::option,
             {{ap_defkey::long_name, "output"}});
// min_value_count defaults to 0, so the option is optional
if (app.entry("out").content.empty() == false)
    std::cout << app.entry("out").content.front() << '\n';
```

### Repeatable flag with a cap

```cpp
ap_entry verbose("verbose", ap_type::flag,
                 {{ap_defkey::short_name, "v,V"},
                  {ap_defkey::long_name, "verbose"},
                  {ap_defkey::max_repetition, "3"}});
// -vvv          -> success
// -vvvv         -> flag_too_many_repetition
```

### One value per occurrence

```cpp
ap_entry out("out", ap_type::option,
             {{ap_defkey::short_name, "o"},
              {ap_defkey::no_variadic, ""}});
// -o a b        -> content {"a"}; b is a leftover
// -o a -o b     -> content {"a", "b"}
```

### Exclusive groups

```cpp
ap_entry build_flag("fast", ap_type::flag,
                    {{ap_defkey::long_name, "fast"}});
build_flag.group = {"build"}; // no longer in "default"

ap_instance app({build_flag});
app.pre_parse({"app", "--fast"});
app.evaluate("build");
```

### Barrier

```cpp
ap_instance app({
    ap_entry("left", ap_type::positional, {{ap_defkey::barrier, "|"}}),
    ap_entry("right", ap_type::positional, {})});
app.pre_parse({"app", "1", "2", "|", "3", "4"});
app.evaluate();
// left.content  == {"1", "2"}
// right.content == {"3", "4"}
```

<a id="full-example"></a>

## 12. Putting it together: a subcommand parser

This program applies the rules from
[section 6.6](#66-recommended-multi-phase-parsing): it keeps one entry table,
parses in two layers, lets `--help` and `--version` win over argument errors,
and uses the `explanations` text as the label in error messages.

```cpp
#include <argparser.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace argparser;

namespace group
{
inline constexpr const char *general = "general";
inline constexpr const char *build = "build";
inline constexpr const char *verify = "verify";
} // namespace group

namespace id
{
inline constexpr const char *command = "command";
inline constexpr const char *help = "help";
inline constexpr const char *version = "version";
inline constexpr const char *verbose = "verbose";
inline constexpr const char *jobs = "jobs";
inline constexpr const char *target = "target";
inline constexpr const char *manifest = "manifest";
} // namespace id

ap_instance def_args()
{
    return ap_instance({
        ap_entry(id::command, ap_type::positional,
                 {{ap_defkey::group, group::general},
                  {ap_defkey::explanations, "command: build or verify"},
                  {ap_defkey::min_positional_count, "1"},
                  {ap_defkey::max_positional_count, "1"}}),
        ap_entry(id::help, ap_type::flag,
                 {{ap_defkey::group, group::general},
                  {ap_defkey::explanations, "-h or --help"},
                  {ap_defkey::short_name, "h"},
                  {ap_defkey::long_name, "help"},
                  {ap_defkey::max_repetition, "1"}}),
        ap_entry(id::version, ap_type::flag,
                 {{ap_defkey::group, group::general},
                  {ap_defkey::explanations, "-V or --version"},
                  {ap_defkey::short_name, "V"},
                  {ap_defkey::long_name, "version"},
                  {ap_defkey::max_repetition, "1"}}),
        ap_entry(id::verbose, ap_type::flag,
                 {{ap_defkey::group, group::build},
                  {ap_defkey::group, group::verify},
                  {ap_defkey::explanations, "-v or --verbose"},
                  {ap_defkey::short_name, "v"},
                  {ap_defkey::long_name, "verbose"},
                  {ap_defkey::max_repetition, "1"}}),
        ap_entry(id::jobs, ap_type::option,
                 {{ap_defkey::group, group::build},
                  {ap_defkey::group, group::verify},
                  {ap_defkey::explanations, "-j or --jobs"},
                  {ap_defkey::short_name, "j"},
                  {ap_defkey::long_name, "jobs"},
                  {ap_defkey::max_value_count, "1"}}),
        ap_entry(id::target, ap_type::positional,
                 {{ap_defkey::group, group::build},
                  {ap_defkey::explanations, "target to build"},
                  {ap_defkey::min_positional_count, "1"},
                  {ap_defkey::max_positional_count, "1"}}),
        ap_entry(id::manifest, ap_type::positional,
                 {{ap_defkey::group, group::verify},
                  {ap_defkey::explanations, "manifest to verify"},
                  {ap_defkey::min_positional_count, "1"},
                  {ap_defkey::max_positional_count, "1"}}),
    });
}

void report(ap_instance &cli)
{
    for (const auto &err : cli.errors())
        if (err.second == ap_result::invalid_arguments)
            std::cerr << "unexpected argument: " << err.first << '\n';
        else
            std::cerr << describe(err.second) << ": "
                      << cli.entry(err.first).explanations.front() << '\n';
}

int main(int argc, char **argv)
{
    if (argc < 1)
        return 2;

    ap_instance cli = def_args();
    cli.pre_parse(std::vector<std::string>(argv, argv + argc));

    // Layer 1: the command word and the global flags.
    cli.evaluate(group::general);
    if (cli.entry(id::help).repetition)
    {
        std::cout << "usage: tool build|verify [options]\n";
        return 0;
    }
    if (cli.entry(id::version).repetition)
    {
        std::cout << "tool 1.0\n";
        return 0;
    }
    if (!cli.fine())
    {
        report(cli);
        return 2;
    }

    // Layer 2: only the selected subcommand.
    const std::string command = cli.entry(id::command).content.at(0);
    if (command == "build")
    {
        cli.evaluate(group::build);
        if (!cli.fine(false))
        {
            report(cli);
            return 2;
        }
        const bool verbose = cli.entry(id::verbose).repetition != 0;
        const auto &jobs = cli.entry(id::jobs).content;
        std::cout << "build " << cli.entry(id::target).content.at(0)
                  << " (verbose=" << verbose
                  << ", jobs=" << (jobs.empty() ? "auto" : jobs.front())
                  << ")\n";
        return 0;
    }
    if (command == "verify")
    {
        cli.evaluate(group::verify);
        if (!cli.fine(false))
        {
            report(cli);
            return 2;
        }
        std::cout << "verify " << cli.entry(id::manifest).content.at(0)
                  << '\n';
        return 0;
    }

    std::cerr << "unknown command: " << command << '\n';
    return 2;
}
```

What to notice:

- Entry names and group names are string constants, so definitions and lookups
  cannot drift apart.
- Every entry has a group. The program never calls `evaluate()`; it evaluates
  `general` and then exactly one subcommand group.
- `fine()` is used after the general layer and `fine(false)` after the final
  layer.
- `max_repetition = "1"` turns the global flags into single-use switches; the
  early checks still let `--help --help` print help and exit 0.
- `report()` prints `invalid_arguments` differently from entry errors and uses
  `ap_entry::explanations` as the human-readable label. The example gives every
  entry at least one explanation line, so `front()` is always valid.

> **Note:** the structure above is a simplified version of the command-line
> layer in [Tsubaki-Next](https://github.com/Lawrence-Charland/Tsubaki-Next).


## 13. Script language

`<ap_script.hpp>` builds entries from a compact text script:

```cpp
#include <ap_script.hpp>

std::vector<ap_entry> entries = argparser::parse_script(script_text);
ap_instance cli(entries);
```

```
script         := block*
block          := '{' statement-list? '}'
statement-list := statement (';' statement)* ';'?
statement      := key '=' value
key            := "entry_name" | "entry_type" | <ap_defkey name>
value          := any text without ';' or '}' (may be empty)
```

```
{
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
}
```

Rules:

- The script is a sequence of `{...}` blocks. Whitespace between blocks is
  ignored, and a block describes exactly one entry.
- Statements are separated by `;`; empty statements, including whitespace-only
  ones, are skipped. Trailing `;`, blank lines and indentation are all fine.
- `key = value` splits at the **first** `=`. Whitespace around the key and the
  value is trimmed, while whitespace inside the value is kept, so
  `explanations = where to write the result` stores
  `where to write the result`.
- **There is no quote syntax.** `"`, `{` and any further `=` are ordinary
  characters. `;` ends the statement and `}` ends the block, so a value cannot
  contain either of them.
- `entry_name` and `entry_type` are required in every block. `entry_type` is
  `positional`, `option` or `flag`.
- Every other key is an `ap_defkey` name from section 4.1, so
  `max_value_count = 1` is the same as `{ap_defkey::max_value_count, "1"}`.
  Keys are case-sensitive. Keys that take no value are written with an empty
  value: `no_consume = ;` or `no_consume =` at the end of a block.
- A statement without `=` is read as a key with an empty value, so a bare
  `oops` fails as an unknown key.
- Repeated keys follow section 4.1: names and `explanations` append, counts keep
  the last value. A repeated `entry_name` keeps the last spelling.
- Entries appear in block order, and every entry is still validated by
  `ap_entry`, so all rules of section 4 apply here too.

Errors throw `std::runtime_error` with the `argparser: ` prefix. Script-level
messages name the block, numbered from `0` in script order:

```text
argparser: entry #1: "oops" is not a definition key.
argparser: entry #0: missing "entry_type".
argparser: entry #0: block opened at offset 0 is missing its closing '}'.
```

The reported cases are: a missing `{` before a block, an unclosed block, a
nested `{{`, a statement without a key (`=1`), a missing or empty `entry_name`,
a missing, empty or unknown `entry_type`, and an unknown key. Structural
problems are found first, because the blocks are located before any statement
is read: `{{entry_name=a;entry_type=flag;}}` reports the stray second block.

Errors raised by `ap_entry` (section 4) keep their own wording and name the
entry rather than the block:

```text
argparser: Error occurred while parsing definition of 'a' :	no_consume is not assignable. Please leave the value empty.
```

An empty or whitespace-only script yields an empty vector.
