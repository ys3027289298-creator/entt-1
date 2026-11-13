#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>
#include <entt/entity/registry.hpp>

namespace {

struct position {
    std::uint64_t x, y;
};

struct velocity {
    std::uint64_t x, y;
};

struct stable {
    static constexpr auto in_place_delete = true;
    std::uint64_t x;
};

constexpr std::size_t entity_count = 10000;
constexpr std::size_t iterations = 5000;

template<typename Func>
double measure(Func &&func) {
    std::vector<double> samples{};

    for(std::size_t i = 0u; i < 15u; ++i) {
        const auto start = std::chrono::steady_clock::now();

        for(std::size_t j = 0u; j < iterations; ++j) {
            func();
        }

        const auto end = std::chrono::steady_clock::now();
        samples.push_back(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / static_cast<double>(iterations));
    }

    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2u];
}

} // namespace

int main() {
    entt::registry registry;

    for(std::uint64_t i = 0u; i < entity_count; ++i) {
        const auto entt = registry.create();
        registry.emplace<position>(entt, i, i);

        if((i % 2u) == 0u) {
            registry.emplace<velocity>(entt, i, i);
        }

        if((i % 4u) == 0u) {
            registry.emplace<stable>(entt, i);
        }
    }

    const auto group = registry.group<position>(entt::get<velocity>);
    const auto group2 = registry.group<>(entt::get<position, velocity>);

    std::uint64_t acc = 0u;

    std::cout << "entities=" << entity_count << " iterations=" << iterations << "\n";
    std::cout << "single_view_each: " << measure([&] {
        registry.view<position>().each([&acc](auto, auto &pos) { acc += pos.x; });
    }) << " ns\n";
    std::cout << "single_view_range: " << measure([&] {
        for(auto [entt, pos] : registry.view<position>().each()) { acc += pos.x; }
    }) << " ns\n";
    std::cout << "multi_view_each: " << measure([&] {
        registry.view<position, velocity>().each([&acc](auto, auto &pos, auto &vel) { acc += pos.x + vel.x; });
    }) << " ns\n";
    std::cout << "multi_view_range: " << measure([&] {
        for(auto [entt, pos, vel] : registry.view<position, velocity>().each()) { acc += pos.x + vel.x; }
    }) << " ns\n";
    std::cout << "owned_group_each: " << measure([&] {
        group.each([&acc](auto, auto &pos, auto &vel) { acc += pos.x + vel.x; });
    }) << " ns\n";
    std::cout << "owned_group_range: " << measure([&] {
        for(auto [entt, pos, vel] : group.each()) { acc += pos.x + vel.x; }
    }) << " ns\n";
    std::cout << "nonowned_group_range: " << measure([&] {
        for(auto [entt, pos, vel] : group2.each()) { acc += pos.x + vel.x; }
    }) << " ns\n";
    std::cout << "storage_each: " << measure([&] {
        for(auto [entt, pos] : registry.storage<position>().each()) { acc += pos.x; }
    }) << " ns\n";
    std::cout << "storage_reach: " << measure([&] {
        for(auto [entt, pos] : registry.storage<position>().reach()) { acc += pos.x; }
    }) << " ns\n";
    std::cout << "stable_view_each: " << measure([&] {
        registry.view<stable>().each([&acc](auto, auto &s) { acc += s.x; });
    }) << " ns\n";

    std::cout << "checksum: " << acc << "\n";
    return 0;
}
