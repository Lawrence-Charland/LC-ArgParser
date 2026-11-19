# argparser 使用指南

[English](./USAGE.md) | 简体中文

`argparser` 是一个面向 GNU/UNIX 风格命令行的 C++17 小型库。公共头文件只包含
声明，具体行为以本文档为准。

- `#include <argparser.hpp>`，并链接 `argparser` 库（或把 `src/argparser.cpp`
  与你的程序一起编译）。
- 所有内容都位于 `argparser` 命名空间。
- 所有值都是纯字符串；库只做数量限制，不会替你转换数字、路径或枚举。

## 目录

- [1. 心智模型](#1-心智模型)
- [2. 构建与链接](#2-构建与链接)
- [3. 条目类型](#3-条目类型)
- [4. 定义条目](#4-定义条目)
  - [4.1 定义键](#41-定义键)
  - [4.2 定义键没有类型校验](#42-定义键没有类型校验)
- [5. 预处理](#5-预处理)
  - [5.1 词元样式](#51-词元样式)
- [6. 求值](#6-求值)
  - [6.1 顺序、分组与消耗](#61-顺序分组与消耗)
  - [6.2 选项](#62-选项)
  - [6.3 标志](#63-标志)
  - [6.4 位置参数与屏障](#64-位置参数与屏障)
  - [6.5 条目之间的相互影响](#65-条目之间的相互影响)
  - [6.6 推荐的多阶段解析流程](#66-推荐的多阶段解析流程)
- [7. 结果](#7-结果)
  - [7.1 `ap_result`](#71-ap_result)
  - [7.2 成功检查与剩余参数](#72-成功检查与剩余参数)
  - [7.3 开始新一轮](#73-开始新一轮)
- [8. 条目管理](#8-条目管理)
- [9. 自定义错误消息](#9-自定义错误消息)
- [10. 注意事项](#10-注意事项)
- [11. 常见片段](#11-常见片段)
  - [需要两个值的必填选项](#需要两个值的必填选项)
  - [可选选项](#可选选项)
  - [有次数上限的可重复标志](#有次数上限的可重复标志)
  - [每次出现只取一个值](#每次出现只取一个值)
  - [独占分组](#独占分组)
  - [屏障](#屏障)
- [12. 综合示例：多子命令解析器](#full-example)

## 1. 心智模型

一个 `ap_instance` 持有两样东西：

- 一组 `ap_entry` 对象，用来描述可接受的参数；
- 一池预处理后的命令行词元。

解析分为三个阶段：

1. `pre_parse(...)` 拆分命令行，并给每个词元标记 `ap_arg_style`。
2. `evaluate()` 或 `evaluate(group)` 按条目顺序把词元与有效条目匹配，并把结果
   写回条目。
3. `fine()` / `errors()` 报告结果。

没有独立的结果对象，结果就是每个 `ap_entry` 的字段：`general_result`、
`content` 和 `repetition`。

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

    app.pre_parse(argc, argv); // 也可用：app.pre_parse({"app", "-v", "file.txt"});
    app.evaluate();

    if (app.fine() == false)
    {
        for (auto &err : app.errors())
            std::cerr << err.first << ": " << describe(err.second) << '\n';
        return 1;
    }
}
```

多子命令程序的推荐分层方式见
[6.6 节](#66-推荐的多阶段解析流程)，完整程序见[第 12 节](#full-example)。

## 2. 构建与链接

使用 CMake：

```cmake
add_subdirectory(path/to/argparser)
target_link_libraries(your_target PRIVATE argparser)
```

不使用 CMake：

```sh
g++ -std=c++17 -Ipath/to/argparser/include \
    main.cpp path/to/argparser/src/argparser.cpp -o app
```

安装后的版本提供 `<argparser.hpp>`，链接目标名为 `argparser`。需要 C++17 编译器。

## 3. 条目类型

| `ap_type` | 匹配方式 | `content` 保存什么 |
| --- | --- | --- |
| `positional` | 仅按位置 | 收集到的位置参数 |
| `option` | `-x` / `--xxx` | 收集到的值 |
| `flag` | `-x` / `--xxx` | 每次出现所使用的名称（不含连字符） |

`repetition` 只对标志维护，并且此时恒等于 `content.size()`；选项和位置参数不使用
它。

## 4. 定义条目

```cpp
ap_entry(name, type, { {key, value}, ... });
```

- `name` 是条目在实例内的身份。同一实例中的两个有效条目不能重名。
- 定义按键值对的顺序依次应用。
- 所有值都是 `std::string`，数量也不例外：
  `{ap_defkey::max_value_count, "2"}`，而不是 `2`。
- 非法定义会让构造函数抛出 `std::runtime_error`，消息中包含条目名：
  `argparser: Error occurred while parsing definition of '<name>' : <原因>`。

构造后仍可直接修改公共字段：

| 字段 | 含义 |
| --- | --- |
| `name` | 条目名称 |
| `group` | 分组集合，初始为 `{"default"}` |
| `explanations` | 帮助文本 |
| `type` | `ap_type` |
| `consume` | 匹配到的词元是否从池中移除（默认 `true`） |
| `no_variadic` | 选项每次出现是否最多取一个值（默认 `false`） |
| `positional_range` | 位置参数数量 `{min, max}`，默认 `{0, -1}` |
| `valcnt_range` | 选项值数量 `{min, max}`，默认 `{0, -1}` |
| `max_repetition` | 标志重复上限，默认 `-1` |
| `long_names`、`short_names` | 接受的名称，不含前缀连字符 |
| `barriers` | 结束位置参数收集的词元 |
| `general_result` | `evaluate` 写入的结果 |
| `content`、`repetition` | `evaluate` 写入的载荷 |

### 4.1 定义键

| 键 | 值 | 作用 | 重复定义时 |
| --- | --- | --- | --- |
| `group` | 分组名 | 再加入一个分组 | 追加分组 |
| `explanations` | 任意文本 | 帮助文本 | 追加 |
| `no_consume` | 必须为 `""` | 令 `consume = false` | 无副作用 |
| `long_name` | `name` 或 `a,b` | 登记长名称，不含 `--` | 追加 |
| `short_name` | `c` 或 `a,b` | 登记单字符短名称，不含 `-`；`-` 会被拒绝 | 追加 |
| `min_positional_count` | 整数 `>= 0` | 位置参数最少数量 | 后者覆盖 |
| `max_positional_count` | 非零整数 | 位置参数最多数量；负数表示不限 | 后者覆盖 |
| `barrier` | 字面词元 | 结束位置参数收集；不按 `,` 拆分；空值被拒绝 | 追加 |
| `no_variadic` | 必须为 `""` | 选项每次出现最多取一个值 | 无副作用 |
| `min_value_count` | 整数 `>= 0` | 选项值总数量下限；`> 0` 表示该选项必填 | 后者覆盖 |
| `max_value_count` | 非零整数 | 选项值总数量上限；负数表示不限 | 后者覆盖 |
| `max_repetition` | 非零整数 | 标志出现次数上限；负数表示不限 | 后者覆盖 |

几个最容易弄错的地方：

- **选项值数量是总数。** `min_value_count` 和 `max_value_count` 统计的是该选项
  **所有出现次数**累计的值，而不是每次出现各自的数量。例如
  `max_value_count = 1` 时，`-o a -o b` 也是错误，尽管每次出现只有一个值。
- **`min_value_count > 0` 会让选项变成必填。** 选项未出现时其值总数为 `0`，
  因此该条目一旦被求值就会报 `option_too_few_values`。
- **每个条目一开始都属于 `default`。** `group` 键只是增加分组，从不移除
  `default`。若要让条目只属于另一个分组，请在构造后直接赋值：
  `ap_entry e(...); e.group = {"build"};`
- **`explanations` 总是追加。** 多次定义会累积；若需替换，请自行清空该 vector。
- **名称不要带连字符。** `{ap_defkey::long_name, "--output"}` 无法匹配
  `--output`，应写成 `"output"`。短名称必须各自只有一个字符。
- **屏障是字面量，且不按 `,` 拆分。** 多个屏障请重复写 `barrier` 键。
- **数量使用 `std::stoi` 解析。** 非数字会抛异常；负的最小值和为零的最大值都会
  被拒绝（`max_*` 一律不能为 0）。
- **名称校验。** 空的长名称、长度不等于 1 的短名称，以及短名称 `-`，都会在构造
  时被拒绝。

### 4.2 定义键没有类型校验

构造函数接受任意 `ap_type` 搭配任意键。与类型无关的键会被求值过程忽略：
选项上的 `max_repetition`、标志上的 `max_value_count`、选项上的 `barrier` 等都不
起作用。名称是例外：长名称会从所有有效条目中收集，因此位置参数上的 `long_name`
仍会让预处理器把该词当作长选项词元（而位置参数又永远匹配不到它）；短名称只会
从选项和标志中收集。请不要在位置参数上设置名称。

## 5. 预处理

```cpp
void pre_parse(int argc, char **argv);
void pre_parse(const std::vector<std::string> &argv);
```

两个重载做同样的事，第一个只是包装第二个。`argv[0]` 被记录为程序名
（`ap_arg_style::argv_zero`）。

- vector 不能为空，`argc` 必须至少为 `1`。
- 每次调用都会替换上一次的词元池，并根据有效条目重建名称登记表。
- 新增、更新或删除条目时不会立即重建名称登记表；预处理器要到下一次
  `pre_parse` 才会识别新名称。被删除的条目会立即停止参与求值，因为它的名称已从
  查找表中移除。
- 词元池本身（`ap_arg_token`：原始拼写、词元文本、样式）属于内部细节，
  `ap_instance` 不对外暴露。

### 5.1 词元样式

| 样式 | 含义 |
| --- | --- |
| `argv_zero` | 程序名；不会被任何条目匹配 |
| `simple` | 位置参数值、选项值，或无法识别的词 |
| `single_dash` | 短选项/标志、合并的短选项组，或紧贴值 |
| `double_dash` | 长选项/标志，可带 `=value` |

| 输入 | 产生的词元（`argv[0]` 之后） |
| --- | --- |
| `-v` | `single_dash` `v` |
| `-vv` | 两个 `single_dash` 词元：`v`、`v` |
| `-vV` | 两个 `single_dash` 词元：`v`、`V` |
| `-vo out.txt` | `single_dash` `v`、`single_dash` `o`，然后 `simple` `out.txt` |
| `-ofile.txt` | `single_dash` `o=file.txt`（其余部分是紧贴值） |
| `--verbose` | `double_dash` `verbose` |
| `--output=out.txt` | `double_dash` `output=out.txt` |
| `--output out.txt` | `double_dash` `output`，然后 `simple` `out.txt` |
| `--` | 被丢弃；其后每个词都变为 `simple` |
| `-` | `simple` |
| `--unknown` | 若没有有效条目登记该长名称，则为 `simple` `--unknown` |
| `-q` | 若 `q` 不是已登记的短名称，则为 `simple` `-q` |

预处理规则中值得注意的点：

- 只有 `=` 之前的部分是某个有效条目的已登记长名称时，长词才会成为
  `double_dash` 词元；否则整个词保持 `simple`，可能被选项值或位置参数吞掉。
- 只有首字符是已登记短名称时，短词才会被解析；若首字符未知，整个词保持
  `simple`（`-xv` 是一个 `simple` 词元）。
- 在合并写法中，每个已登记字符都会成为独立词元。遇到取值选项的字符时，词的
  剩余部分作为它的值，合并解析到此结束（`-vo out.txt` → `v`、`o`；
  `-ofile.txt` → `o=file.txt`）。
- 在至少一个已知标志之后出现未知字符时，该字符会作为意外值附加到前一个标志上：
  若 `v` 是标志，`-vX` 会变成 `v=X`，于是该标志报告
  `flag_unexpected_values`。
- `--` 会被移除。它并不能阻止选项把下一个 `simple` 词元当作值：若 `-o` 是取值
  选项，`-o -- a` 会得到 `content == {"a"}`。该词元最终归谁，取决于条目顺序。
- 单独的 `-` 是 `simple`，可以作为位置参数或选项值。

## 6. 求值

### 6.1 顺序、分组与消耗

- `evaluate()` 按条目顺序遍历所有有效条目。
- `evaluate(group)` 只遍历 `group` 集合包含该分组的有效条目，仍按条目顺序。
- 除非 `consume` 为 `false`，条目匹配到的词元会从池中移除；因此后续条目只能看到
  仍然留在池中的词元。
- 条目顺序就是求值顺序。放在前面的贪婪选项或位置参数可能拿走后面条目想要的
  词元。
- `evaluate` 不会清空上一次的结果。`content` 和 `repetition` 会累积，
  `general_result` 只会被新的发现覆盖。
- 每个条目始终包含 `"default"`。因此 `evaluate("default")` 也会运行那些用
  `group` 键加入其他分组的条目。若想严格按分组分发，请直接赋值 `group`，或避免
  调用 `evaluate()` / `evaluate("default")`。

```cpp
ap_entry command("command", ap_type::positional,
                 {{ap_defkey::group, "cli"},
                  {ap_defkey::max_positional_count, "1"}});
ap_entry fast("fast", ap_type::flag,
              {{ap_defkey::long_name, "fast"},
               {ap_defkey::group, "build"}});
// command.group == {"default", "cli"}
// fast.group    == {"default", "build"}

app.evaluate("cli");   // 求值 command
app.evaluate("build"); // 求值 fast
```

### 6.2 选项

选项匹配 `single_dash` 或 `double_dash` 词元，条件是名称属于它的短名称或长名称。
即使没有值，匹配到的选项键也会被消耗（除非 `consume` 为 `false`）。

值有两种收集方式：

- **紧贴：** `-ovalue`、`--opt=value` 直接产生一个值。
- **独立：** 裸 `-o` / `--opt` 之后，连续的 `simple` 词元都会被收集，直到遇到
  非 `simple` 词元或达到数量上限。

短选项会把名称之后的所有内容都当作紧贴值，因此 `-o=value` 记录的是
`"=value"` 而不是 `"value"`。若不希望 `=` 进入值中，请使用 `-o value` 或
`--opt=value`。

数量检查使用所有出现次数的总数：

- 若又匹配到一个键时总数已达到 `max_value_count`，条目报告
  `option_too_many_values`。该键（以及紧贴其上的内容）仍会被消耗，但不会再记录
  新的值。
- 若在收集独立值时达到上限，收集只是停止；剩余词留在池中，只有在检查剩余参数
  时才会体现为 `invalid_arguments`。
- 结束时若总数低于 `min_value_count`，报告 `option_too_few_values`。由于即使选项
  从未出现也会执行该检查，`min_value_count > 0` 意味着“必填”。

`no_variadic` 只改变单次出现的收集方式：每次出现最多取一个独立值。总数仍会跨
多次出现累计，因此上面的最小/最大规则不变。

```cpp
ap_entry o("o", ap_type::option,
           {{ap_defkey::short_name, "o"},
            {ap_defkey::long_name, "opt"}});
// -o a b c        -> content {"a", "b", "c"}
// -o a -o b       -> content {"a", "b"}
// -oa --opt=x     -> content {"a", "x"}
```

配合 `{ap_defkey::no_variadic, ""}`：

```text
-o a b c     -> content {"a"}；b、c 留在池中
-o a -o b    -> content {"a", "b"}
min=2, -o a b       -> option_too_few_values（总数为 1）
min=2, -o a -o b    -> 成功（总数为 2）
max=1, -o a -o b    -> option_too_many_values（总数已达 1）
```

### 6.3 标志

标志的匹配方式与选项相同，但记录的是出现次数而不是值：

- `repetition` 统计出现次数。
- `content` 按命令行顺序保存匹配到的名称（不含连字符）。
- `-v`、`-V`、`--verbose` 可以属于同一个条目，并在 `content` 中保持可区分。
- `--verbose=x` 会记录名称，并额外报告 `flag_unexpected_values`。
- 计数结束后，`repetition > max_repetition` 报告
  `flag_too_many_repetition`（负数表示不限）。

```cpp
// 输入 -vv --verbose --verbose 时：
// repetition == 4
// content    == {"v", "v", "verbose", "verbose"}
```

### 6.4 位置参数与屏障

位置参数从池的开头按顺序收集 `simple` 词元，直到：

- 达到数量上限，或
- 遇到已登记的屏障。

规则：

- 只收集 `simple` 词元。被前面的选项消耗掉的值不可用。
- 没有“位置参数过多”错误；多余的词留在池中。
- 若收集数量低于 `min_positional_count`，条目报告
  `positional_too_few_args`。
- 屏障与词元文本逐字比较，且不会存入 `content`。
- 一个条目最多消耗一个屏障。若数量上限先结束收集，条目会继续向后扫描并消耗
  找到的下一个屏障，但不把它加入 `content`。
- 除非 `consume` 为 `false`，屏障会随条目一起被消耗。
- `--` 不能作为屏障（预处理时已被移除）。由于 `--` 之后的一切都是 `simple`，
  那里看起来像选项的词也可以作为屏障。
- `barrier` 的值不按 `,` 拆分；多个屏障请重复该键。

```cpp
ap_instance app({
    ap_entry("files", ap_type::positional, {{ap_defkey::barrier, "|"}}),
    ap_entry("rest", ap_type::positional, {})});

app.pre_parse({"app", "a.txt", "b.txt", "|", "c.txt"});
app.evaluate();
// files.content == {"a.txt", "b.txt"}
// rest.content  == {"c.txt"}
```

### 6.5 条目之间的相互影响

求值是贪婪且依赖顺序的：

```text
选项 o 在前，位置参数 p 在后：  -o a b  -> o {"a"}，p {"b"}
位置参数 p 在前，选项 o 在后：  -o a b  -> p {"a", "b"}，o {}
```

`no_consume` 会让条目填好自己的 `content` 却把词元留在池中，供后续条目再次匹配。
它很强大，也很容易误用：除非后续条目把它消耗掉，否则同一个词可能既作为值被
记录，又作为剩余参数被报告。

### 6.6 推荐的多阶段解析流程

带子命令的程序，最好把解析拆成明确的层次：

1. 把所有条目集中在一个 `def_args()` 辅助函数中。把命令词和全局标志放进
   `general` 分组，每个子命令的条目放进各自的分组；只在特殊模式下解析的开关可以
   再放一个内部组。
2. 只调用一次 `pre_parse`。
3. `evaluate("general")`，然后在做任何校验之前用 `repetition` 检查 `--help`、
   `--version` 这类提前退出的标志。即使命令行其余部分有错，它们也应当能用。
4. general 层求值后调用 `fine()`。这里忽略剩余参数，因为它们多半属于子命令。
5. 从位置参数的 `content` 读取命令词。
6. `evaluate(<子命令分组>)`，然后用 `fine(false)` 在最后一层拒绝剩余参数。
7. 把标志当作布尔值（`repetition != 0`），把选项/位置参数当作值列表（`content`）。

两条规则可以让层次保持可预期：

- **显式求值分组，不要用 `evaluate()`。** 每个条目同时也属于 `default`；
  `evaluate()` 会一次性运行所有子命令的条目。
- **给每个条目写一行 `explanations`。** 同一段文本可以作为错误消息里的条目名称，
  让 `errors()` 的输出更易读。

[第 12 节](#full-example)的完整程序遵循这一流程。

## 7. 结果

### 7.1 `ap_result`

| 值 | 含义 | 产生位置 |
| --- | --- | --- |
| `unparsed` | 尚未求值 | `general_result` 的初始值 |
| `success` | 没有发现问题 | 求值结束时若结果仍为 `unparsed` 则设置 |
| `positional_too_few_args` | 位置参数总数低于下限 | 位置参数检查 |
| `option_too_few_values` | 选项值总数低于下限 | 选项检查（选项缺失时也会触发） |
| `option_too_many_values` | 达到上限后又匹配到键 | 选项检查 |
| `flag_too_many_repetition` | 标志总数超过 `max_repetition` | 标志检查 |
| `flag_unexpected_values` | 标志出现时带了 `=value` | 标志检查 |
| `invalid_arguments` | 有词元未被消耗 | 由 `errors()` 合成，不会存入条目 |

每个条目只有一个 `general_result`。若同时存在多个问题，后执行的检查会覆盖先前的
结果。

`describe(result)` 可把任意结果转换为消息。`success` 和 `unparsed` 不是错误，
会用 `AP_DESCRIBE_UNKNOWN` 描述。

### 7.2 成功检查与剩余参数

- `fine()`（默认 `ignore_redundant = true`）：所有有效条目都是 `success` 或
  `unparsed` 时返回 `true`。忽略剩余词元。
- `fine(false)`：额外要求 `argv[0]` 之后没有剩余词元。
- `fine(group)`：只检查 `group` 中的条目，且忽略剩余词元。
- `errors()`：返回所有结果既不是 `success` 也不是 `unparsed` 的有效条目，再为
  **第一个**剩余词元追加一条 `invalid_arguments`（键为其原始拼写）。它只报告
  第一个剩余词元，而不是全部。若词元来自合并的短选项，原始拼写是整个合并写法。
  返回的字符串是副本，因此在后续求值后仍然有效。

`errors()` 不按分组过滤。调用 `evaluate("build")` 之后，其他分组中保留了上一次
求值错误的条目仍会被列出。

### 7.3 开始新一轮

- `reset_results()` 清空所有条目（含未激活条目）的 `content`，把 `repetition`
  置 0、`general_result` 置回 `unparsed`。这些也是新建条目的初始值。它不触碰
  词元池。
- `pre_parse(...)` 替换词元池并重建名称登记表。
- 常规的新一轮是：`reset_results()` → `pre_parse(...)` → `evaluate(...)`。

## 8. 条目管理

```cpp
void add_entry(const ap_entry &entry, std::size_t index = SIZE_MAX);
void delete_entry(std::size_t index, bool clear_storage = false);
void update_entry(const ap_entry &entry);

const ap_entry &entry(const std::string &name);
const ap_entry &entry(std::size_t index);
std::size_t get_index(const std::string &name);
```

- `add_entry` 默认追加。传入 `index` 会插入到该位置，后续条目后移；名称不能属于
  任何有效条目。`index` 不能超过当前大小（没有边界检查，超出属于未定义行为）。
- `delete_entry` 会停用条目：名称从查找表移除，此后 `evaluate`、`fine`、
  `errors` 都会跳过它。`clear_storage = false` 时条目及其结果仍可按索引访问；
  `true` 时条目被删除，后续索引随之前移。索引越界抛
  `std::runtime_error`。
- `update_entry` 按名称替换条目，连结果一起替换。名称未激活时抛异常。
- `entry(name)` 和 `get_index(name)` 在名称未激活时抛 `std::out_of_range`。
- `entry(index)` 没有边界检查；索引越界属于未定义行为。
- 新增、更新、删除的名称要到下一次 `pre_parse` 才会被预处理器识别。
- `ap_instance(entries)` 构造函数等价于按顺序逐个添加条目；名称冲突时抛异常。
- `evaluate_entry(entry)` 直接对当前词元池求值单个条目，填充该条目的结果，并在
  `consume` 为 `true` 时消耗其匹配到的词元。大多数程序只需要 `evaluate()` /
  `evaluate(group)`。

```cpp
// entries 是已有的 std::vector<ap_entry>
ap_instance app(entries);
app.add_entry(ap_entry("color", ap_type::option,
                       {{ap_defkey::long_name, "color"}}));
app.update_entry(ap_entry("color", ap_type::option,
                          {{ap_defkey::long_name, "colour"}}));
app.delete_entry(app.get_index("color")); // 停用，保留存储
```

## 9. 自定义错误消息

`describe()` 使用的宏可以在包含头文件之前覆盖：

| 宏 | 默认值 |
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

替换哪些宏就定义哪些；头文件只为尚未定义的宏提供默认值。

## 10. 注意事项

- **一切都是文本。** 数量限制只决定收集多少词元，值的类型转换需要自己做。
- **未知选项可能被吞掉。** 未登记的 `-q` 或 `--foo` 会保持 `simple`，贪婪的选项
  或位置参数可以把它消费掉：`./app -q a.txt` 可能把 `-q` 报告为位置参数值，而
  `fine()` 仍然返回 `true`。
- **`--` 只改变样式。** 若选项条目先于位置参数求值，它仍可把 `--` 之后的词元
  当作值（`-o -- a` → `"a"`）。
- **短选项合并写法有几个陷阱。** `-ofile.txt` 是紧贴值；但 `v` 为标志、`X` 未登记时
  `-vX` 会变成带意外值的标志；`x` 未登记时 `-xv` 是一个 `simple` 词元。
- **`min_value_count > 0` 表示选项必填。** 即使选项未出现也会执行检查。
- **先调用 `pre_parse`。** 在第一次 `pre_parse` 之前，`evaluate` 和 `errors`
  没有有意义的词元可处理，而在空词元池上调用 `argv_zero()` 属于未定义行为。
- **`evaluate` 会累积。** 开始新一轮请调用 `reset_results()` 和 `pre_parse()`。
- **`fine()` 默认忽略剩余参数。** 需要剩余参数导致失败时用 `fine(false)` 或
  `errors()`；`errors()` 只给出第一个剩余参数。
- **分组只增加成员关系。** 每个条目都在 `default` 中，`group` 键不会移除它。
  若需要独占分组，请直接赋值 `group`。
- **定义键没有类型校验。** 无关的键会被忽略；位置参数上的长名称仍会影响预处理。
- **已停用的条目**会被 `evaluate`、`fine`、`errors` 跳过，名称也无法再解析；
  但只要保留存储，`entry(index)` 仍可访问。
- **指向条目列表的引用可能失效。** `add_entry` 与 `delete_entry(..., true)` 可能
  导致 vector 重新分配或元素移动，因此不要在这些调用前后保留 `entry(...)` 返回的
  引用。
- **`argv_zero()` 返回的是视图。** 它指向词元池，下一次 `pre_parse` 后失效。
- **非线程安全。** 同一个 `ap_instance` 不能在无外部同步的情况下并发使用。
- **定义错误抛异常，解析错误不抛。** 非法定义抛 `std::runtime_error`；命令行
  问题写入结果字段。

## 11. 常见片段

### 需要两个值的必填选项

```cpp
ap_entry pair("pair", ap_type::option,
              {{ap_defkey::short_name, "p"},
               {ap_defkey::min_value_count, "2"}});
// -p a b   -> 成功，content {"a", "b"}
// -p a     -> option_too_few_values
// （缺失） -> option_too_few_values
```

### 可选选项

```cpp
ap_entry out("out", ap_type::option,
             {{ap_defkey::long_name, "output"}});
// min_value_count 默认为 0，因此该选项可选
if (app.entry("out").content.empty() == false)
    std::cout << app.entry("out").content.front() << '\n';
```

### 有次数上限的可重复标志

```cpp
ap_entry verbose("verbose", ap_type::flag,
                 {{ap_defkey::short_name, "v,V"},
                  {ap_defkey::long_name, "verbose"},
                  {ap_defkey::max_repetition, "3"}});
// -vvv          -> 成功
// -vvvv         -> flag_too_many_repetition
```

### 每次出现只取一个值

```cpp
ap_entry out("out", ap_type::option,
             {{ap_defkey::short_name, "o"},
              {ap_defkey::no_variadic, ""}});
// -o a b        -> content {"a"}；b 是剩余参数
// -o a -o b     -> content {"a", "b"}
```

### 独占分组

```cpp
ap_entry build_flag("fast", ap_type::flag,
                    {{ap_defkey::long_name, "fast"}});
build_flag.group = {"build"}; // 不再属于 "default"

ap_instance app({build_flag});
app.pre_parse({"app", "--fast"});
app.evaluate("build");
```

### 屏障

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

## 12. 综合示例：多子命令解析器

下面的完整程序应用了
[6.6 节](#66-推荐的多阶段解析流程)的规则：所有条目集中在一张表里，分两层解析，
让 `--help`、`--version` 优先于参数错误，并用 `explanations` 文本作为错误消息
里的标签。

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

    // 第 1 层：命令词与全局标志。
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

    // 第 2 层：只求值选中的子命令。
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

值得注意的地方：

- 条目名和分组名都是字符串常量，定义与查找不会各写各的、逐渐走样。
- 每个条目都有分组。程序从不调用 `evaluate()`，而是先求值 `general`，再只求值
  一个子命令分组。
- general 层之后用 `fine()`，最后一层之后用 `fine(false)`。
- `max_repetition = "1"` 把全局标志变成只能出现一次的开关；由于提前检查，
  `--help --help` 仍然会打印帮助并退出 0。
- `report()` 对 `invalid_arguments` 与条目错误分开输出，并把
  `ap_entry::explanations` 当作人类可读的标签。示例中每个条目至少有一行说明，
  因此 `front()` 总是有效。

> **说明：** 上面的解析结构简化自
> [Tsubaki-Next](https://github.com/Lawrence-Charland/Tsubaki-Next) 的命令行层。

