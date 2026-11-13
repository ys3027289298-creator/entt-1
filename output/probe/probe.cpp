#include <entt/entity/registry.hpp>
#include <cstdint>
#include <iostream>
#include <vector>

struct position { std::uint64_t x, y; };
struct velocity { std::uint64_t x, y; };
struct stable { static constexpr auto in_place_delete = true; std::uint64_t x; };
struct tag {};

static void ids(std::vector<entt::entity> &out, const std::vector<entt::entity> &ents) {
    for(std::size_t i = 0; i < ents.size(); ++i) { out.push_back(ents[static_cast<std::size_t>(ents[i])]); }
}

int main() {
    // --- iteration order probe ---
    {
        entt::registry reg;
        std::vector<entt::entity> ents;
        for(std::uint64_t i = 0; i < 10; ++i) {
            auto e = reg.create();
            ents.push_back(e);
            reg.emplace<position>(e, i, i);
            if(i % 2 == 0) { reg.emplace<velocity>(e, i, i); }
            if(i == 3 || i == 7) { reg.emplace<stable>(e, i); }
        }
        std::vector<std::uint64_t> seq;
        auto emit = [&](const char *name, const std::vector<std::uint64_t> &v) {
            std::cout << name << ":";
            for(auto x : v) { std::cout << " " << x; }
            std::cout << "\n";
        };
        for(auto [e, p] : reg.view<position>().each()) { seq.push_back(p.x); }
        emit("view_single", seq); seq.clear();
        for(auto [e, p, v] : reg.view<position, velocity>().each()) { seq.push_back(p.x); }
        emit("view_multi", seq); seq.clear();
        for(auto [e, p] : reg.view<position>(entt::exclude<velocity>).each()) { seq.push_back(p.x); }
        emit("view_exclude", seq); seq.clear();
        auto g = reg.group<position>(entt::get<velocity>);
        for(auto [e, p, v] : g.each()) { seq.push_back(p.x); }
        emit("group_owned", seq); seq.clear();
        auto g2 = reg.group<>(entt::get<position, velocity>);
        for(auto [e, p, v] : g2.each()) { seq.push_back(p.x); }
        emit("group_nonowned", seq); seq.clear();
        for(auto [e, p] : reg.storage<position>().each()) { seq.push_back(p.x); }
        emit("storage_each", seq); seq.clear();
        for(auto [e, p] : reg.storage<position>().reach()) { seq.push_back(p.x); }
        emit("storage_reach", seq); seq.clear();
        // in-place (stable) view with deletion
        reg.destroy(ents[0]);
    }
    // --- stable component tombstone probe ---
    {
        entt::registry reg;
        std::vector<entt::entity> ents;
        for(std::uint64_t i = 0; i < 6; ++i) {
            auto e = reg.create();
            ents.push_back(e);
            reg.emplace<stable>(e, i);
        }
        reg.remove<stable>(ents[2]);
        reg.remove<stable>(ents[4]);
        std::vector<std::uint64_t> seq;
        for(auto [e, s] : reg.view<stable>().each()) { seq.push_back(s.x); }
        std::cout << "stable_view:";
        for(auto x : seq) { std::cout << " " << x; }
        std::cout << "\n";
        std::cout << "stable_size_hint: " << reg.view<stable>().size_hint() << "\n";
        std::cout << "stable_front_back: " << static_cast<std::uint64_t>(reg.view<stable>().front()) << " " << static_cast<std::uint64_t>(reg.view<stable>().back()) << "\n";
    }
    // --- capacity growth probe ---
    {
        entt::registry reg;
        auto &st = reg.storage<position>();
        std::cout << "capacity:";
        for(std::uint64_t i = 0; i < 12; ++i) {
            auto e = reg.create();
            reg.emplace<position>(e, i, i);
            std::cout << " " << st.capacity();
        }
        std::cout << "\n";
        std::cout << "capacity_100:";
        for(std::uint64_t i = 12; i < 100; ++i) { auto e = reg.create(); reg.emplace<position>(e, i, i); }
        std::cout << " " << st.capacity();
        for(std::uint64_t i = 100; i < 1000; ++i) { auto e = reg.create(); reg.emplace<position>(e, i, i); }
        std::cout << " " << st.capacity();
        for(std::uint64_t i = 1000; i < 10000; ++i) { auto e = reg.create(); reg.emplace<position>(e, i, i); }
        std::cout << " " << st.capacity() << "\n";
        // non-owning group capacity growth
        entt::registry reg2;
        auto g = reg2.group<>(entt::get<position>);
        std::cout << "group_capacity:";
        for(std::uint64_t i = 0; i < 10; ++i) {
            auto e = reg2.create();
            reg2.emplace<position>(e, i, i);
            std::cout << " " << g.capacity();
        }
        std::cout << "\n";
    }
    return 0;
}
