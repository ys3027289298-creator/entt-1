// Compile-time source compatibility proof for the unified storage access layer.
// This translation unit exercises the public template interface of views,
// groups and single component traversal. It must compile unchanged against
// both the pre-refactor and post-refactor headers.
#include <cstdint>
#include <type_traits>
#include <vector>
#include <entt/entity/registry.hpp>
#include <entt/entity/view.hpp>
#include <entt/entity/group.hpp>
#include <entt/entity/storage.hpp>
#include <entt/entity/runtime_view.hpp>

struct position { std::uint64_t x, y; };
struct velocity { std::uint64_t x, y; };
struct stable { static constexpr auto in_place_delete = true; std::uint64_t x; };
struct tag {};

using position_storage = entt::storage_type_t<position, entt::entity>;
using velocity_storage = entt::storage_type_t<velocity, entt::entity>;

// public view/group/storage template instantiations and member types
using single_view = entt::basic_view<entt::get_t<position_storage>, entt::exclude_t<>>;
using multi_view = entt::basic_view<entt::get_t<position_storage, velocity_storage>, entt::exclude_t<tag>>;
using owned_group = entt::basic_group<entt::owned_t<position_storage>, entt::get_t<velocity_storage>, entt::exclude_t<>>;
using nonowned_group = entt::basic_group<entt::owned_t<>, entt::get_t<position_storage, velocity_storage>, entt::exclude_t<entt::storage_type_t<tag, entt::entity>>>;

static_assert(std::is_same_v<typename single_view::entity_type, entt::entity>);
static_assert(std::is_same_v<typename multi_view::entity_type, entt::entity>);
static_assert(std::is_same_v<typename owned_group::entity_type, entt::entity>);
static_assert(std::is_same_v<typename nonowned_group::entity_type, entt::entity>);
static_assert(std::is_same_v<typename single_view::size_type, std::size_t>);
static_assert(std::is_same_v<typename owned_group::size_type, std::size_t>);

// iterator capability semantics
static_assert(std::is_same_v<typename std::iterator_traits<typename single_view::iterator>::iterator_category, std::random_access_iterator_tag>);
static_assert(std::is_same_v<typename std::iterator_traits<typename multi_view::iterator>::iterator_category, std::forward_iterator_tag>);
static_assert(std::is_same_v<typename std::iterator_traits<typename owned_group::iterator>::iterator_category, std::random_access_iterator_tag>);
static_assert(std::is_same_v<typename std::iterator_traits<typename position_storage::iterator>::iterator_category, std::random_access_iterator_tag>);

// iterable value types
static_assert(std::is_same_v<typename decltype(std::declval<single_view>().each())::value_type, std::tuple<entt::entity, position &>>);
static_assert(std::is_same_v<typename decltype(std::declval<multi_view>().each())::value_type, std::tuple<entt::entity, position &, velocity &>>);
static_assert(std::is_same_v<typename decltype(std::declval<owned_group>().each())::value_type, std::tuple<entt::entity, position &, velocity &>>);
static_assert(std::is_same_v<typename position_storage::iterable::value_type, std::tuple<entt::entity, position &>>);
static_assert(std::is_same_v<typename position_storage::const_iterable::value_type, std::tuple<entt::entity, const position &>>);
static_assert(std::is_same_v<typename position_storage::reverse_iterable::value_type, std::tuple<entt::entity, position &>>);
static_assert(std::is_same_v<typename position_storage::const_reverse_iterable::value_type, std::tuple<entt::entity, const position &>>);

int main() {
    entt::registry registry;
    std::vector<entt::entity> entities;

    for(std::uint64_t i = 0u; i < 64u; ++i) {
        const auto entt = registry.create();
        entities.push_back(entt);
        registry.emplace<position>(entt, i, i);

        if((i % 2u) == 0u) { registry.emplace<velocity>(entt, i, i); }
        if((i % 4u) == 0u) { registry.emplace<stable>(entt, i); }
        if((i % 8u) == 0u) { registry.emplace<tag>(entt); }
    }

    std::uint64_t acc = 0u;

    // view: begin/end/find/front/back/contains/size_hint
    const auto view = registry.view<position, velocity>(entt::exclude<tag>);
    acc += static_cast<std::uint64_t>(view.front());
    acc += static_cast<std::uint64_t>(view.back());
    acc += view.size_hint();
    acc += view.contains(entities[2u]) ? 1u : 0u;
    acc += static_cast<std::uint64_t>(*view.find(entities[4u]));
    for(const auto entt : view) { acc += static_cast<std::uint64_t>(entt); }
    view.each([&acc](auto entt, auto &pos, auto &vel) { acc += pos.x + vel.x; });
    for(auto [entt, pos, vel] : view.each()) { acc += pos.x + vel.x; }
    for(auto [entt, pos, vel] : view.each()) { acc += pos.y + vel.y; }

    // view get/operator[]/use
    acc += view.get<position>(entities[2u]).x;
    acc += std::get<0>(view.get(entities[2u])).x;
    auto view2 = registry.view<position, velocity>();
    view2.use<velocity>();
    acc += view2.size_hint();

    // view combination operator|
    const auto combined = registry.view<position>() | registry.view<velocity>();
    acc += combined.size_hint();

    // single component view
    const auto single = registry.view<position>();
    acc += single.size();
    acc += static_cast<std::uint64_t>(*single.begin());
    acc += static_cast<std::uint64_t>(*single.rbegin());
    single.each([&acc](auto entt, auto &pos) { acc += pos.x; });
    single.each([&acc](auto &pos) { acc += pos.y; });
    for(auto [entt, pos] : single.each()) { acc += pos.x; }

    // stable (in-place) view
    const auto stable_view = registry.view<stable>();
    acc += stable_view.size_hint();
    for(auto [entt, s] : stable_view.each()) { acc += s.x; }

    // owned group
    const auto group = registry.group<position>(entt::get<velocity>);
    acc += group.size();
    acc += static_cast<std::uint64_t>(group.front());
    acc += static_cast<std::uint64_t>(group.back());
    acc += static_cast<std::uint64_t>(group[0u]);
    acc += group.contains(entities[0u]) ? 1u : 0u;
    acc += static_cast<std::uint64_t>(*group.find(entities[2u]));
    group.each([&acc](auto entt, auto &pos, auto &vel) { acc += pos.x + vel.x; });
    for(auto [entt, pos, vel] : group.each()) { acc += pos.x + vel.x; }
    group.sort<position>([](const auto &lhs, const auto &rhs) { return lhs.x < rhs.x; });
    group.sort([](const auto lhs, const auto rhs) { return lhs < rhs; });

    // non-owning group
    auto group2 = registry.group<>(entt::get<position, velocity>);
    acc += group2.size();
    acc += group2.capacity();
    group2.shrink_to_fit();
    group2.each([&acc](auto entt, auto &pos, auto &vel) { acc += pos.x + vel.x; });
    for(auto [entt, pos, vel] : group2.each()) { acc += pos.x + vel.x; }

    // storage traversal
    auto &storage = registry.storage<position>();
    acc += storage.size();
    acc += storage.capacity();
    for(auto [entt, pos] : storage.each()) { acc += pos.x; }
    for(auto [entt, pos] : storage.reach()) { acc += pos.x; }
    const auto &cstorage = storage;
    for(auto [entt, pos] : cstorage.each()) { acc += pos.x; }
    for(auto [entt, pos] : cstorage.reach()) { acc += pos.x; }
    acc += storage.get(entities[1u]).x;
    acc += std::get<0>(storage.get_as_tuple(entities[1u])).x;

    // runtime view
    entt::runtime_view rt{};
    rt.iterate(registry.storage<position>()).iterate(registry.storage<velocity>());
    for(const auto entt : rt) { acc += static_cast<std::uint64_t>(entt); }

    return static_cast<int>(acc % 256u);
}
