# 万实体遍历耗时对比（10000 实体）

基准程序: output/bench/bench_10k.cpp; 每轮 15 样本 x 5000 次迭代, taskset 钉核, 12 轮交替, 取最小值

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

**worst regression: 2.72% (limit 5%)**
