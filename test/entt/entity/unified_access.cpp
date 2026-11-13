#include <cstdint>
#include <type_traits>
#include <vector>
#include <gtest/gtest.h>
#include <entt/entity/registry.hpp>
#include <entt/entity/storage.hpp>
#include <entt/entity/view.hpp>

namespace unified_access {

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

struct tag {};

template<typename Iterable>
std::vector<std::uint64_t> collect(Iterable &&iterable) {
    std::vector<std::uint64_t> seq{};

    for(const auto &pack : iterable) {
        seq.push_back(static_cast<std::uint64_t>(std::get<0>(pack)));
    }

    return seq;
}

} // namespace unified_access

TEST(UnifiedAccess, IterationOrderView) {
    entt::registry registry;
    std::vector<entt::entity> entities{};

    for(std::uint64_t i = 0u; i < 10u; ++i) {
        const auto entt = registry.create();
        entities.push_back(entt);
        registry.emplace<unified_access::position>(entt, i, i);

        if((i % 2u) == 0u) {
            registry.emplace<unified_access::velocity>(entt, i, i);
        }
    }

    // single component traversal: reverse packed order of the storage
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::position>().each()), (std::vector<std::uint64_t>{9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u}));
    // multi component view: leading storage order, filtered
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::position, unified_access::velocity>().each()), (std::vector<std::uint64_t>{8u, 6u, 4u, 2u, 0u}));
    // exclude filter
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::position>(entt::exclude<unified_access::velocity>).each()), (std::vector<std::uint64_t>{9u, 7u, 5u, 3u, 1u}));

    // empty component view
    registry.emplace<unified_access::tag>(entities[3u]);
    registry.emplace<unified_access::tag>(entities[5u]);
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::tag>().each()), (std::vector<std::uint64_t>{5u, 3u}));
}

TEST(UnifiedAccess, IterationOrderGroup) {
    entt::registry registry;

    for(std::uint64_t i = 0u; i < 10u; ++i) {
        const auto entt = registry.create();
        registry.emplace<unified_access::position>(entt, i, i);

        if((i % 2u) == 0u) {
            registry.emplace<unified_access::velocity>(entt, i, i);
        }
    }

    const auto group = registry.group<unified_access::position>(entt::get<unified_access::velocity>);
    ASSERT_EQ(unified_access::collect(group.each()), (std::vector<std::uint64_t>{8u, 6u, 4u, 2u, 0u}));

    const auto group2 = registry.group<>(entt::get<unified_access::position, unified_access::velocity>);
    ASSERT_EQ(unified_access::collect(group2.each()), (std::vector<std::uint64_t>{0u, 2u, 4u, 6u, 8u}));

    // owned group reorders the owned storage: the single component view
    // iterates the same packed array and therefore reflects the new order
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::position>().each()), (std::vector<std::uint64_t>{9u, 1u, 7u, 3u, 5u, 8u, 6u, 4u, 2u, 0u}));
}

TEST(UnifiedAccess, IterationOrderStorage) {
    entt::registry registry;

    for(std::uint64_t i = 0u; i < 10u; ++i) {
        const auto entt = registry.create();
        registry.emplace<unified_access::position>(entt, i, i);

        if((i % 2u) == 0u) {
            registry.emplace<unified_access::velocity>(entt, i, i);
        }
    }

    // force the owned group to reorder its owned storage
    const auto group = registry.group<unified_access::position>(entt::get<unified_access::velocity>);
    ASSERT_EQ(group.size(), 5u);

    // storage traversal goes through the same access layer as views
    ASSERT_EQ(unified_access::collect(registry.storage<unified_access::position>().each()), (std::vector<std::uint64_t>{9u, 1u, 7u, 3u, 5u, 8u, 6u, 4u, 2u, 0u}));
    // reverse storage traversal is the exact reverse of forward traversal
    ASSERT_EQ(unified_access::collect(registry.storage<unified_access::position>().reach()), (std::vector<std::uint64_t>{0u, 2u, 4u, 6u, 8u, 5u, 3u, 7u, 1u, 9u}));
    // view and storage agree on the single component traversal order
    ASSERT_EQ(unified_access::collect(registry.view<unified_access::position>().each()), unified_access::collect(registry.storage<unified_access::position>().each()));
}

TEST(UnifiedAccess, TombstoneSemantics) {
    entt::registry registry;
    std::vector<entt::entity> entities{};

    for(std::uint64_t i = 0u; i < 6u; ++i) {
        const auto entt = registry.create();
        entities.push_back(entt);
        registry.emplace<unified_access::stable>(entt, i);
    }

    registry.remove<unified_access::stable>(entities[2u]);
    registry.remove<unified_access::stable>(entities[4u]);

    const auto view = registry.view<unified_access::stable>();
    ASSERT_EQ(unified_access::collect(view.each()), (std::vector<std::uint64_t>{5u, 3u, 1u, 0u}));
    ASSERT_EQ(view.size_hint(), 6u);
    ASSERT_EQ(view.front(), entities[5u]);
    ASSERT_EQ(view.back(), entities[0u]);
    ASSERT_TRUE(view.contains(entities[3u]));
    ASSERT_FALSE(view.contains(entities[2u]));
}

TEST(UnifiedAccess, CapacityGrowth) {
    entt::registry registry;
    auto &storage = registry.storage<unified_access::position>();

    // page-granular growth: one page (1024 elements) at a time
    for(std::uint64_t i = 0u; i < 1024u; ++i) {
        const auto entt = registry.create();
        registry.emplace<unified_access::position>(entt, i, i);
        ASSERT_EQ(storage.capacity(), 1024u);
    }

    const auto entt = registry.create();
    registry.emplace<unified_access::position>(entt, 0u, 0u);
    ASSERT_EQ(storage.capacity(), 2048u);

    // non-owning group: geometric growth of the internal sparse set
    entt::registry other{};
    const auto group = other.group<>(entt::get<unified_access::position>);
    const std::vector<std::uint64_t> expected{1u, 2u, 4u, 4u, 8u, 8u, 8u, 8u, 16u, 16u};

    for(std::uint64_t i = 0u; i < expected.size(); ++i) {
        const auto entity = other.create();
        other.emplace<unified_access::position>(entity, i, i);
        ASSERT_EQ(group.capacity(), expected[static_cast<std::size_t>(i)]);
    }
}

TEST(UnifiedAccess, CapabilitySemantics) {
    using position_storage = entt::storage_type_t<unified_access::position, entt::entity>;
    using velocity_storage = entt::storage_type_t<unified_access::velocity, entt::entity>;
    using stable_storage = entt::storage_type_t<unified_access::stable, entt::entity>;

    // single component view capabilities
    using single_view = entt::basic_view<entt::get_t<position_storage>, entt::exclude_t<>>;
    static_assert(std::is_same_v<typename std::iterator_traits<typename single_view::iterator>::iterator_category, std::random_access_iterator_tag>);
    static_assert(std::is_same_v<typename std::iterator_traits<typename single_view::reverse_iterator>::iterator_category, std::random_access_iterator_tag>);

    // in-place (stable) single component view capabilities
    using stable_view = entt::basic_view<entt::get_t<stable_storage>, entt::exclude_t<>>;
    static_assert(std::is_same_v<typename std::iterator_traits<typename stable_view::iterator>::iterator_category, std::forward_iterator_tag>);
    static_assert(std::is_same_v<typename stable_view::reverse_iterator, void>);

    // multi component view capabilities
    using multi_view = entt::basic_view<entt::get_t<position_storage, velocity_storage>, entt::exclude_t<>>;
    static_assert(std::is_same_v<typename std::iterator_traits<typename multi_view::iterator>::iterator_category, std::forward_iterator_tag>);

    // group capabilities
    using owned_group = entt::basic_group<entt::owned_t<position_storage>, entt::get_t<velocity_storage>, entt::exclude_t<>>;
    static_assert(std::is_same_v<typename std::iterator_traits<typename owned_group::iterator>::iterator_category, std::random_access_iterator_tag>);

    // storage capabilities
    static_assert(std::is_same_v<typename std::iterator_traits<typename position_storage::iterator>::iterator_category, std::random_access_iterator_tag>);

    // iterable value types
    static_assert(std::is_same_v<typename decltype(std::declval<single_view>().each())::value_type, std::tuple<entt::entity, unified_access::position &>>);
    static_assert(std::is_same_v<typename decltype(std::declval<multi_view>().each())::value_type, std::tuple<entt::entity, unified_access::position &, unified_access::velocity &>>);
    static_assert(std::is_same_v<typename decltype(std::declval<owned_group>().each())::value_type, std::tuple<entt::entity, unified_access::position &, unified_access::velocity &>>);
    static_assert(std::is_same_v<typename position_storage::iterable::value_type, std::tuple<entt::entity, unified_access::position &>>);
    static_assert(std::is_same_v<typename position_storage::const_iterable::value_type, std::tuple<entt::entity, const unified_access::position &>>);
    static_assert(std::is_same_v<typename position_storage::reverse_iterable::value_type, std::tuple<entt::entity, unified_access::position &>>);
    static_assert(std::is_same_v<typename position_storage::const_reverse_iterable::value_type, std::tuple<entt::entity, const unified_access::position &>>);

    ASSERT_TRUE(true);
}

TEST(UnifiedAccess, ViewGroupConsistency) {
    entt::registry registry;

    for(std::uint64_t i = 0u; i < 32u; ++i) {
        const auto entt = registry.create();
        registry.emplace<unified_access::position>(entt, i, i);

        if((i % 3u) == 0u) {
            registry.emplace<unified_access::velocity>(entt, i, i);
        }
    }

    const auto group = registry.group<unified_access::position>(entt::get<unified_access::velocity>);

    // the owned group and the equivalent multi component view iterate the
    // same entities in the same order
    ASSERT_EQ(unified_access::collect(group.each()), unified_access::collect(registry.view<unified_access::position, unified_access::velocity>().each()));
    ASSERT_EQ(group.size(), (registry.view<unified_access::position, unified_access::velocity>().size_hint()));

    // group sort keeps the owned storage packed and ordered
    group.sort<unified_access::position>([](const auto &lhs, const auto &rhs) { return lhs.x < rhs.x; });
    std::vector<std::uint64_t> sorted{};

    for(const auto [entt, pos, vel] : group.each()) {
        sorted.push_back(pos.x);
    }

    ASSERT_EQ(sorted, (std::vector<std::uint64_t>{0u, 3u, 6u, 9u, 12u, 15u, 18u, 21u, 24u, 27u, 30u}));
}
