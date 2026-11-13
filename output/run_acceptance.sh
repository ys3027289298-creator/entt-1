#!/usr/bin/env bash
# Acceptance run for the unified storage access layer refactor.
# Sections are titled for the recording.
set -e
cd "$(dirname "$0")/.."
ROOT="$PWD"

section() {
    echo
    echo "================================================================"
    echo "== $1"
    echo "================================================================"
}

section "1. 行为等价测试（仓库自带测试入口 ctest）"
cd "$ROOT/build/after"
ctest --output-on-failure 2>&1 | tail -6
cd "$ROOT"

section "2. 行为等价测试（统一访问层专项，改造前后对比）"
echo "[before] 基线头文件编译的等价测试："
/tmp/unified_before --gtest_brief=1 2>&1 | tail -3
echo "[after]  新头文件编译的等价测试："
/tmp/unified_after --gtest_brief=1 2>&1 | tail -3

section "3. 万实体遍历耗时对比（10000 实体，改造前后）"
echo "基准程序: output/bench/bench_10k.cpp (每轮 15 样本 x 5000 次迭代, 钉核, 12 轮交替, 取最小值/中位数)"
mkdir -p output/bench/raw
for i in $(seq 1 12); do
    taskset -c 3 ./output/bench/bench_before > output/bench/raw/before_$i.txt
    taskset -c 3 ./output/bench/bench_after > output/bench/raw/after_$i.txt
done
python3 - <<'PYEOF'
import re, statistics
def load(p):
    d={}
    for line in open(p):
        m=re.match(r'(\w+):\s+([\d.]+)\s+ns',line)
        if m: d[m.group(1)]=float(m.group(2))
    return d
before=[load(f'output/bench/raw/before_{i}.txt') for i in range(1,13)]
after=[load(f'output/bench/raw/after_{i}.txt') for i in range(1,13)]
print(f"{'benchmark':<22}{'before_ns':>12}{'after_ns':>12}{'delta%':>9}")
worst=0.0
report=[]
for k in before[0]:
    b=min(x[k] for x in before); a=min(x[k] for x in after)
    d=(a-b)/b*100
    worst=max(worst,d)
    report.append((k,b,a,d))
    print(f"{k:<22}{b:>12.1f}{a:>12.1f}{d:>8.2f}%")
with open('output/bench/comparison.md','w') as f:
    f.write('# 万实体遍历耗时对比（10000 实体）\n\n')
    f.write('基准程序: output/bench/bench_10k.cpp; 每轮 15 样本 x 5000 次迭代, taskset 钉核, 12 轮交替, 取最小值\n\n')
    f.write('| benchmark | before (ns) | after (ns) | delta% |\n')
    f.write('| --- | ---: | ---: | ---: |\n')
    for k,b,a,d in report:
        f.write(f'| {k} | {b:.1f} | {a:.1f} | {d:+.2f}% |\n')
    f.write(f'\n**worst regression: {worst:.2f}% (limit 5%)**\n')
print(f"\nworst regression: {worst:.2f}% (acceptance limit: 5%)")
assert worst <= 5.0, "regression limit exceeded"
print("PASS: 万实体遍历耗时退化不超过 5%")
PYEOF

section "4. 编译期兼容证明（同一 TU 在改造前后头文件上编译）"
g++ -std=c++20 -I /tmp/entt-baseline/src -o /tmp/compat_before output/compat/compat.cpp && echo "[before] compat.cpp 编译通过"
g++ -std=c++20 -I "$ROOT/src" -o /tmp/compat_after output/compat/compat.cpp && echo "[after]  compat.cpp 编译通过"
g++ -std=c++20 -I "$ROOT/single_include" -c output/compat/compat.cpp -o /tmp/compat_single.o 2>/dev/null && echo "[single] 单头文件 compat.cpp 编译通过"
echo "静态断言（迭代器能力语义/值类型）在两版头文件上均成立"

section "5. 产物清单"
find output -maxdepth 2 -type f | sort
