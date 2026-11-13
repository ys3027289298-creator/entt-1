# 统一存储访问层改造报告

## 目标

视图（view）、分组（group）、单组件遍历（storage/single-component view）三条迭代路径原本各写一套
（`extended_view_iterator`、`extended_group_iterator`、`extended_storage_iterator` 三个独立实现，
行为细节不一致）。改造目标：统一存储访问层，三条路径走同一接口，删除重复路径；
迭代顺序、能力语义、容量增长策略与改造前等价；万实体遍历耗时退化 ≤ 5%；对外模板接口源码兼容。

## 改造内容

### 新增：统一存储访问层 `src/entt/entity/access.hpp`

- `internal::entity_accessor<Pool>`：按实体标识解析元素（`get_as_tuple`），用于视图与非拥有分组。
- `internal::positional_accessor<Pool>`：按紧凑位置解析元素（`rbegin()[index]`），用于拥有分组。
- `internal::payload_accessor<Container, Page>`：按页式容器位置解析元素，用于存储自身遍历。
- `internal::extended_iterator<It, Accessor...>`：统一的"实体迭代器 + 元素访问器"拉链迭代器，
  取代原来三个独立的 extended 迭代器实现（重复路径已删除）。
- `internal::view_iterator`（原 view.hpp 内）与 `all_of`/`none_of` 一并迁入访问层，
  供多组件视图与 in-place 单组件视图共享。

### 修改

- `view.hpp`：`extended_view_iterator` 改为继承 `extended_iterator` 的薄封装（构造时从视图迭代器
  的存储指针数组构建访问器）；`view.hpp` 内的 `view_iterator`/`all_of`/`none_of` 迁移至访问层。
- `group.hpp`：`extended_group_iterator` 改为继承 `extended_iterator` 的薄封装
  （owned 用位置访问器，get 用实体访问器）。
- `storage.hpp`：`extended_storage_iterator` 改为 `extended_iterator` 的别名；
  存储的 `each()/reach()` 直接以 `payload_accessor` 构造统一迭代器。
- `single_include/entt/entt.hpp`：用仓库自带脚本 `scripts/amalgamate.py` 重新生成
  （原文件停留在 4.0 版本，本次同步为与 src 一致的 4.1 并包含访问层）。
- `test/CMakeLists.txt` + `test/entt/entity/unified_access.cpp`：新增行为等价专项测试。

### 等价性论证

- **迭代顺序**：视图/分组/存储的实体序由稀疏集 packed 数组与分组重排决定，本次未触碰
  `sparse_set.hpp` 与分组处理器（`group_handler`）的任何逻辑；元素解析表达式
  （`get_as_tuple`、`rbegin()[index]`、页内偏移 `pos/Page + fast_mod`）与原实现逐行等价。
- **能力语义**：迭代器类别（random access / forward / input）、`value_type`/`reference`、
  反向迭代器可用性均通过编译期静态断言验证（见下）。
- **容量增长策略**：未修改任何分配/扩容代码路径；专项测试断言页粒度（1024）增长轨迹与
  非拥有分组的几何增长轨迹与基线一致。

## 验收数据

### 1. 行为等价测试（仓库自带测试入口）

- `ctest` 全量 16 个测试目标（含 entity 861 个用例）改造后全部通过：`output/ctest_after.log`。
- 新增专项等价测试 `test/entt/entity/unified_access.cpp`（迭代顺序黄金值、墓碑语义、
  容量增长轨迹、能力语义静态断言、视图/分组一致性）：
  同一测试文件在**基线头文件**与**新头文件**上分别编译，均 7/7 通过
  （`output/equivalence/before.txt`、`output/equivalence/after.txt`）。
- ASan+UBSan 构建下 entity 861 个用例全部通过（无未定义行为）。

### 2. 万实体遍历耗时对比

见 `output/bench/comparison.md`（10000 实体，15 样本 × 5000 次迭代/轮，taskset 钉核，
12 轮交替取最小值）：

| benchmark | before (ns) | after (ns) | delta% |
| --- | ---: | ---: | ---: |
| single_view_each | 4218.1 | 4153.1 | -1.54% |
| single_view_range | 4091.4 | 4202.9 | +2.72% |
| multi_view_each | 6467.4 | 6460.9 | -0.10% |
| multi_view_range | 10583.6 | 10738.8 | +1.47% |
| owned_group_each | 5765.3 | 4685.6 | -18.73% |
| owned_group_range | 5801.1 | 5742.3 | -1.01% |
| nonowned_group_range | 5652.4 | 5715.0 | +1.11% |
| storage_each | 3201.1 | 3190.7 | -0.32% |
| storage_reach | 4164.7 | 3077.3 | -26.11% |
| stable_view_each | 2101.2 | 1960.0 | -6.72% |

**最差退化 +2.72%（验收上限 5%）**，多数路径反而更快（统一访问层免去了一次性
`++` 多个迭代器的开销）。原始数据在 `output/bench/raw/`。

### 3. 编译期兼容证明

`output/compat/compat.cpp` 覆盖公开模板接口（`basic_view`/`basic_group`/`basic_storage`/
`registry.view/group/storage`、`each()` 两种形态、`use<>`、`operator|`、`front/back/find/
contains/size_hint/capacity/shrink_to_fit/sort/sort_as`、运行时视图、四种存储迭代器
value_type 静态断言等）：**同一 TU 在改造前头文件与改造后头文件上均编译通过、运行结果一致**；
单头文件 `single_include/entt/entt.hpp` 亦编译通过。

### 4. 录屏

`output/recording.cast`（asciinema 格式，分段标题：1. 行为等价测试-ctest；
2. 行为等价测试-专项对比；3. 万实体耗时对比；4. 编译期兼容证明；5. 产物清单）。
播放：`asciinema play output/recording.cast`。

## 产物清单（output/）

- `REPORT.md`：本报告。
- `run_acceptance.sh`：一键验收脚本（分段标题，含全部验收步骤）。
- `acceptance.log`：验收运行日志。
- `recording.cast`：验收录屏（分段标题）。
- `bench/bench_10k.cpp`、`bench_before`、`bench_after`：万实体基准程序与二进制。
- `bench/comparison.md`、`bench/raw/`：耗时对比表与原始数据。
- `equivalence/before.txt`、`equivalence/after.txt`：等价测试改造前后结果。
- `compat/compat.cpp`：编译期兼容证明 TU。
- `ctest_before.log`、`ctest_after.log`：仓库测试入口结果。
- `probe/probe.cpp`：黄金行为数据采集探针。
