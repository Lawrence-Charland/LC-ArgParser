# argparser

一个面向 GNU/UNIX 风格选项的小型命令行参数解析库，使用 C++17 编写。

[English](./README.md) | 简体中文

> **本项目是个人练习项目，也是首次发布。** 它因学习和日常使用而写，没有经过审计、模糊测试或生产环境检验， 因此请预期它在细节上仍显粗糙，行为也随时可能调整。

> **我不打算进行明确的版本管理。** 这里没有语义化版本的承诺，没有发布标签，也没有变更日志。 `CMakeLists.txt` 中的 `VERSION 0.1.0` 只是 CMake 为共享库的 `VERSION`/`SOVERSION` 所需的占位值，并不构成兼容性约定。 API 可能随时变化；如果你需要稳定的接口，请固定到某个提交。

## 特性

- 用「条目」描述命令行：`positional`（位置参数）、`option`（选项）、 `flag`（标志）。

- 支持 GNU/UNIX 风格语法：

  - 长名称：`--verbose`、`--output=out.txt`、`--output out.txt`

  - 短名称：`-v`、合并写法 `-vv`、混合写法 `-vo out.txt`

  - `--` 结束选项解析

- 每个条目可登记多个长名称/短名称（`{"v,V"}` 会同时登记两者）。

- 基于分组的子命令分发：`evaluate("subA")`、`fine("subA")`。

- 校验能力：位置参数的最少/最多数量、选项值的最少/最多数量、标志的最大重复次数。

- 解析结果直接存放在条目自身（`general_result`、`content`、`repetition`），无需额外的结果对象。`content` 由三种类型共用，含义见 [「`content` 里放的是什么」](#content-里放的是什么)。

- 标志按名称记录、而不只是计数，因此 `-v` 与 `--verbose` 这类别名可以区分开。

- 可替换的错误提示：`describe()` 以及可覆盖的 `AP_DESCRIBE_*` 宏。

- 头文件中带有 Doxygen 注释与用法片段。

## 环境要求

- 支持 C++17 的编译器（目前只在 GCC 上使用过）。

- CMake 3.10+（可选，也可以直接编译 `argparser.cpp` 和你自己的驱动）。

## 项目结构

```
argparser/
├── include/argparser.hpp   公共头文件：声明 + Doxygen 注释
├── argparser.cpp           实现
├── main.cpp                手工测试用的临时驱动程序（不属于 API）
├── CMakeLists.txt
└── CMakePresets.json       "linux-g++" 预设
```

`main.cpp` 是一份测试用例，而不是示例：它只是随手用来手工验证解析器的小程序，
随时可能被改写、扩充甚至删除，请不要在它之上搭建工具或期望。真正的文档是本
README 与头文件中的片段，而不是那个示例二进制。

## 构建与运行

下面的命令构建的是示例目标 `app`。在你自己的项目里，请把目标和源文件换成自己
的；库本身并不依赖 `main.cpp`。

使用自带的 CMake 预设：

```
cmake --preset linux-g++
cmake --build out/build/linux-g++ --target app
./out/build/linux-g++/app subA -i in.txt -o out.txt
```

使用普通 CMake：

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/app subA -i in.txt -o out.txt
```

不使用 CMake：

```
g++ -std=c++17 -Iinclude main.cpp argparser.cpp -o app
```

CMake 构建会生成共享库目标 `argparser` 和示例可执行文件 `app`。 `cmake --install` 只会安装库，头文件不会被安装；同时该目标没有导出自己的 include 目录。因此请在你自己项目中把 `include/` 加入头文件搜索路径，并自行 链接 `argparser.cpp`（或构建出的库）。

## 快速上手

```
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

$ ./app -vv --output=out.txt a.txt b.txt
verbose: 2
output: out.txt
input: a.txt
input: b.txt

$ ./app
input: Not enough positional arguments
```

## 解析流程

1. `pre_parse(argc, argv)` 复制 `argv` 并转发给 `pre_parse(const std::vector<std::string> &)`，由后者为每个词元标记 `ap_arg_style`（`argv_zero`、`simple`、`single_dash`、`double_dash`），并根据当前有效条目重建名称登记表。参数不是来自 `main` 时可直接使用 vector 重载：

   ```
   app.pre_parse({"app", "-v", "--output=out.txt", "a.txt"});
   ```

2. `evaluate()` 或 `evaluate(group)` 按条目顺序逐一匹配词元。匹配到的词元 会被消耗（从列表中移除），除非该条目的 `consume` 为 `false`。

3. `fine()` 与 `errors()` 报告最终结果。

## 用分组实现子命令

每个条目都有一个 `group`（默认 `"default"`）。只对当前子命令所属的分组 求值即可：

```
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

示例驱动 [main.cpp](./main.cpp) 就是这样分发的。它只是一份测试用例，能接受的
命令行随时会变；目前的行为是：

```
./app subA -i in.txt -o out.txt   # 退出码 0
./app subB -v                     # 退出码 0
./app subB -v -v                  # 退出码 1（max_repetition 为 1）
./app wrong                       # 退出码 2
```

## 支持的写法一览

| 输入 | 解释 |
| - | - |
| `-v` | 短标志 |
| `-vv` | 标志出现两次（合并的短选项） |
| `-vo out.txt` | `-v`，随后 `-o` 取值 `out.txt` |
| `-o out.txt` | 短选项 + 独立的值 |
| `-ofile.txt` | 短选项 + 紧贴的值 |
| `--verbose` | 长标志 |
| `--output=out.txt` | 使用 `=value` 的长选项 |
| `--output out.txt` | 长选项 + 独立的值 |
| `--` | 选项结束；其后的内容全部按位置参数处理 |
| `-` | 位置参数值（单独的短横线不是选项） |


## `content` 里放的是什么

`content` 是一个 `std::vector<std::string>`，承载条目解析出的内容。它的含义
取决于条目的 `type`，而不是这个字段本身：

| `type` | `content` 中保存的内容 |
| - | - |
| `positional` | 位置参数，按命令行顺序排列 |
| `option` | 为该选项收集到的值 |
| `flag` | 每次出现所使用的名称（不含前置短横线） |

`flag` 这一种值得说明，因为此时 `content` 存的是名称而不是值。`repetition` 统计
出现次数，`content` 则按命令行顺序为每次出现保留一个名称：

```
const ap_entry &verbose = app.entry("verbose");
// 以 "./app -vV --verbose --verbose" 运行时：
// verbose.repetition == 4
// verbose.content    == {"v", "V", "verbose", "verbose"}
```

因此即使 `-v`、`-V`、`--verbose` 属于同一条目，也能区分实际使用的是哪一个；
并且 `repetition` 恒等于 `content.size()`。即使是 `--verbose=x` 也会记录名称，
同时额外报告 `flag_unexpected_values`。

## API 速览

`ap_type` —— 条目所描述的参数种类：

| 取值 | 含义 |
| - | - |
| `positional` | 按位置识别，不依赖名称或前缀 |
| `option` | `-x` / `--xxx`，可以带值 |
| `flag` | `-x` / `--xxx`，不带值；计次，且每次使用的名称都保存在 `content` 中 |


`ap_defkey` —— 通过 `ap_entry` 构造函数定义条目时使用的键：

| 键 | 取值 |
| - | - |
| `group` | 分组名，默认 `"default"` |
| `explanations` | 说明文本，默认空 |
| `no_consume` | 值留空；把匹配到的参数留给下一个条目 |
| `long_name` | 一个或多个名称，用 `,` 分隔 |
| `short_name` | 一个或多个单字符，用 `,` 分隔 |
| `min_positional_count` | 整数 `>= 0`，默认 `0` |
| `max_positional_count` | 非零整数；负数或不写表示不限，默认 `-1` |
| `min_value_count` | 整数 `>= 0`，默认 `0` |
| `max_value_count` | 非零整数；负数或不写表示不限，默认 `-1` |
| `max_repetition` | 非零整数；负数或不写表示不限，默认 `-1` |


`ap_entry` —— 一个参数定义及其解析结果：

| 成员 | 作用 |
| - | - |
| `name`、`group`、`explanations` | 身份信息与帮助文本 |
| `type`、`consume` | 行为控制 |
| `positional_range`、`valcnt_range`、`max_repetition` | 校验限制 |
| `long_names`、`short_names` | 可接受的名称 |
| `general_result` | `ap_result`，默认 `unparsed` |
| `content` | 解析出的内容；具体含义取决于 `type`（见[上文](#content-里放的是什么)） |
| `repetition` | 标志出现的次数 |


`ap_instance` —— 持有条目与预处理后的参数：

| 成员 | 作用 |
| - | - |
| `ap_instance(entries)` | 用条目列表构造 |
| `add_entry(entry, index = SIZE_MAX)` | 插入条目（默认追加到末尾） |
| `delete_entry(index, clear_storage = false)` | 停用条目 |
| `update_entry(entry)` | 按名称替换已有条目 |
| `evaluate_entry(entry)` | 直接对单个条目求值 |
| `pre_parse(argc, argv)` / `pre_parse(tokens)` | 拆分并标记参数 |
| `evaluate(group)` / `evaluate()` | 对某个分组 / 全部有效条目求值 |
| `reset_results()` | 清空 `content`、`repetition`、`general_result` |
| `fine(ignore_redundant = true)` / `fine(group)` | 判断是否成功 |
| `errors()` | 失败条目以及剩余的未消耗参数 |
| `entry(name)` / `entry(index)` | 访问条目 |
| `get_index(name)` | 取得条目的位置 |
| `argv_zero()` | 最近一次 `pre_parse` 记录的 `argv[0]` |


`ap_result` —— `success`、`unparsed`、`positional_too_few_args`、 `option_too_few_values`、`option_too_many_values`、`flag_too_many_repetition`、 `flag_unexpected_values`、`invalid_arguments`。`describe(result)` 可以把其中 任意一个转换成提示文本。

`ap_arg_style` 由 `pre_parse` 使用并对外暴露，但对多数使用者而言属于内部细节。

## 错误处理

`describe()` 把解析结果转换为文本。任何提示都可以通过在包含头文件之前定义对应的宏来替换：

```
#define AP_DESCRIBE_INVALID_ARGUMENTS "unexpected argument"
#include <argparser.hpp>
```

可用宏：`AP_DESCRIBE_POSITIONAL_TOO_FEW_ARGS`、 `AP_DESCRIBE_OPTION_TOO_FEW_VALUES`、`AP_DESCRIBE_OPTION_TOO_MANY_VALUES`、 `AP_DESCRIBE_FLAG_TOO_MANY_REPETITION`、`AP_DESCRIBE_FLAG_UNEXPECTED_VALUES`、 `AP_DESCRIBE_INVALID_ARGUMENTS`、`AP_DESCRIBE_UNKNOWN`。

`success` 与 `unparsed` 不是错误，它们会由 `AP_DESCRIBE_UNKNOWN` 描述。

## 注意事项与已知行为

- **一切都是纯文本。** 数量限制只决定收集多少个词元，数值转换由使用者完成。

- **无法识别的词元会保持 `simple`**，从而可能被贪婪匹配的位置参数条目吞掉： `./app -q a.txt` 会得到 `input: -q`、`input: a.txt`，且 `fine()` 仍返回 `true`。如果希望剩余参数被报告为 `invalid_arguments`，请使用 `fine(false)`。

- **紧贴的短选项值会被贪婪拆分。** `-ofile.txt` 得到 `file.txt`，而 `-oout.txt` 得到 `ut.txt`，因为第二个 `o` 本身也是已登记的短名称。 建议使用 `-o value` 或 `--output=value`。

- **`min_value_count >= 1` 会让选项变成必填项：** 若命令行中没有该选项， 条目会报告 `option_too_few_values`。

- **先调用 `pre_parse`，再调用任何形式的 `evaluate` 方法。如果某次求值之后添加 、修改了任何条目，再次求值之前也必须调用 `pre_parse` 。** 首次 `pre_parse` 之前调用 `argv_zero()` 属于未定义行为。

- **`evaluate` 不会清空上一次的结果。** `content` 与 `repetition` 会累加； 想重新开始请调用 `reset_results()` 并重新 `pre_parse`。

- **`entry(index)` 不做边界检查** —— 越界属于未定义行为。

- **被停用的条目**（`delete_entry`）会被 `evaluate`、`fine` 与 `errors` 跳过，也无法再按名称查找（仍可通过偏移量查找），因此它上一轮留下的失败结果不再影响结果。

## 文档

更详尽的 API 参考写在 [include/argparser.hpp](./include/argparser.hpp) 的 Doxygen 注释中，其中包含构造函数的使用片段。若要生成 HTML，请运行 Doxygen 并把 `INPUT` 设为 `include`、`EXTRACT_ALL` 设为 `YES`（或用你自己的 Doxyfile 指向该头文件）。

## 许可证

MIT 许可证 —— 详见 [LICENSE](./LICENSE)。

版权所有 (c) 2026 Lawrence-Charland。

简而言之：只要保留版权声明与许可声明，你可以自由使用、复制、修改、合并、
发布、分发、再许可和/或销售本软件。本软件按「原样」提供，不附带任何形式的
担保。

MIT 许可证适用于整个项目以及每一个提交，与版本号无关。

## 作者

Lawrence Charland —— LC Projects。个人练习项目，首次发布。

