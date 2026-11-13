#ifndef ENTT_ENTITY_ACCESS_HPP
#define ENTT_ENTITY_ACCESS_HPP

#include "../config/config.h"
#include "../core/bit.hpp"
#include "../core/iterator.hpp"
#include "../stl/array.hpp"
#include "../stl/concepts.hpp"
#include "../stl/cstddef.hpp"
#include "../stl/iterator.hpp"
#include "../stl/memory.hpp"
#include "../stl/tuple.hpp"
#include "../stl/type_traits.hpp"
#include "../stl/utility.hpp"
#include "entity.hpp"
#include "fwd.hpp"

namespace entt {

/*! @cond ENTT_INTERNAL */
namespace internal {

/**
 * @brief Unified storage access layer.
 *
 * All the entity-based iteration facilities (views, groups and single
 * element traversal) resolve their elements through the accessors defined
 * here. An accessor is a lightweight callable object that maps the position
 * of a base entity iterator to the elements of a single storage.
 */

[[nodiscard]] bool all_of(auto first, const auto last, const auto entt) noexcept {
    for(; (first != last) && (*first)->contains(entt); ++first) {}
    return first == last;
}

[[nodiscard]] bool none_of(auto first, const auto last, const auto entt) noexcept {
    for(; (first != last) && !(*first)->contains(entt); ++first) {}
    return first == last;
}

/*! @brief Resolves the packed position of a base entity iterator. */
template<typename It>
[[nodiscard]] constexpr auto access_index(const It &it) noexcept {
    return it.index();
}

/*! @brief Resolves the packed position of a reversed base entity iterator. */
template<typename It>
[[nodiscard]] constexpr auto access_index(const stl::reverse_iterator<It> &it) noexcept {
    return access_index(it.base()) + 1;
}

/**
 * @brief By-entity accessor: resolves elements through entity identifiers.
 * @tparam Type Storage type from which to fetch the elements.
 */
template<typename Type>
class entity_accessor {
public:
    /*! @brief Storage type from which to fetch the elements. */
    using pool_type = Type;

    constexpr entity_accessor() noexcept
        : pool{} {}

    constexpr entity_accessor(Type *ref) noexcept
        : pool{ref} {}

    /**
     * @brief Returns the elements assigned to the entity of a base iterator.
     * @param it A base entity iterator.
     * @return The elements assigned to the entity of the base iterator.
     */
    template<typename It>
    [[nodiscard]] constexpr decltype(auto) operator()(const It &it) const noexcept {
        return pool->get_as_tuple(*it);
    }

private:
    Type *pool;
};

/**
 * @brief By-position accessor: resolves elements through packed positions.
 * @tparam Type Storage type from which to fetch the elements.
 */
template<typename Type>
class positional_accessor {
public:
    /*! @brief Storage type from which to fetch the elements. */
    using pool_type = Type;

    constexpr positional_accessor() noexcept
        : pool{} {}

    constexpr positional_accessor(Type *ref) noexcept
        : pool{ref} {}

    /**
     * @brief Returns the element at the position of a base iterator.
     * @param it A base entity iterator.
     * @return The element at the position of the base iterator.
     */
    template<typename It>
    [[nodiscard]] constexpr decltype(auto) operator()(const It &it) const noexcept {
        if constexpr(stl::is_void_v<typename Type::value_type>) {
            return stl::make_tuple();
        } else {
            return stl::forward_as_tuple(pool->rbegin()[access_index(it)]);
        }
    }

private:
    Type *pool;
};

/**
 * @brief Payload accessor: resolves elements directly from a paged container.
 * @tparam Container Type of the paged container.
 * @tparam Page Page size of the container.
 */
template<typename Container, auto Page>
class payload_accessor {
    template<typename, auto>
    friend class payload_accessor;

    using container_type = stl::remove_const_t<Container>;
    using alloc_traits = stl::allocator_traits<typename container_type::allocator_type>;

    using pointer = stl::conditional_t<
        stl::is_const_v<Container>,
        typename alloc_traits::template rebind_traits<typename stl::pointer_traits<typename container_type::value_type>::element_type>::const_pointer,
        typename alloc_traits::template rebind_traits<typename stl::pointer_traits<typename container_type::value_type>::element_type>::pointer>;

public:
    /*! @brief Element reference type. */
    using reference = decltype(*stl::declval<pointer>());

    constexpr payload_accessor() noexcept
        : payload{} {}

    constexpr payload_accessor(Container *ref) noexcept
        : payload{ref} {}

    template<typename Other>
    requires stl::is_const_v<Container> && stl::same_as<container_type, Other>
    constexpr payload_accessor(const payload_accessor<Other, Page> &other) noexcept
        : payload{other.payload} {}

    /**
     * @brief Returns the element at the position of a base iterator.
     * @param it A base entity iterator.
     * @return The element at the position of the base iterator.
     */
    template<typename It>
    [[nodiscard]] constexpr decltype(auto) operator()(const It &it) const noexcept {
        const auto pos = static_cast<typename container_type::size_type>(access_index(it));
        return stl::forward_as_tuple(static_cast<reference>((*payload)[pos / Page][fast_mod(static_cast<stl::size_t>(pos), Page)]));
    }

private:
    Container *payload;
};

/**
 * @brief Unified extended iterator.
 *
 * Zips a base entity iterator with a set of storage accessors. Views, groups
 * and single element traversal all share this implementation to iterate
 * entities and their elements.
 *
 * @tparam It Base entity iterator type.
 * @tparam Accessor Storage accessor types.
 */
template<typename It, typename... Accessor>
class extended_iterator {
    template<typename, typename...>
    friend class extended_iterator;

public:
    /*! @brief Base entity iterator type. */
    using iterator_type = It;
    /*! @brief Value type of the iterator. */
    using value_type = decltype(stl::tuple_cat(stl::make_tuple(*stl::declval<It>()), stl::declval<const Accessor &>()(stl::declval<const It &>())...));
    /*! @brief Pointer type of the iterator. */
    using pointer = input_iterator_pointer<value_type>;
    /*! @brief Reference type of the iterator. */
    using reference = value_type;
    /*! @brief Difference type of the iterator. */
    using difference_type = stl::ptrdiff_t;
    /*! @brief Iterator category. */
    using iterator_category = stl::input_iterator_tag;
    /*! @brief Iterator concept. */
    using iterator_concept = stl::forward_iterator_tag;

    /*! @brief Default constructor. */
    constexpr extended_iterator()
        : it{},
          accessor{} {}

    /**
     * @brief Constructs an extended iterator from a base iterator and a set
     * of storage accessors.
     * @param from Base entity iterator.
     * @param ref Storage accessors.
     */
    constexpr extended_iterator(iterator_type from, Accessor... ref)
        : it{from},
          accessor{ref...} {}

    /**
     * @brief Constructs an extended iterator from a convertible counterpart.
     * @tparam Args Types of accessors of the other iterator.
     * @param other The iterator to copy from.
     */
    template<typename... Args>
    requires (sizeof...(Args) == sizeof...(Accessor)) && (stl::constructible_from<Accessor, Args> && ...)
    constexpr extended_iterator(const extended_iterator<It, Args...> &other)
        : it{other.it},
          accessor{other.accessor} {}

    /*! @brief Advances the iterator. */
    constexpr extended_iterator &operator++() noexcept {
        return ++it, *this;
    }

    /*! @copydoc operator++ */
    constexpr extended_iterator operator++(int) noexcept {
        const extended_iterator orig = *this;
        return ++(*this), orig;
    }

    /**
     * @brief Dereferences the iterator.
     * @return A tuple that contains the current entity and its elements.
     */
    [[nodiscard]] constexpr reference operator*() const noexcept {
        return [this]<auto... Index>(stl::index_sequence<Index...>) {
            return stl::tuple_cat(stl::make_tuple(*it), stl::get<Index>(accessor)(it)...);
        }(stl::index_sequence_for<Accessor...>{});
    }

    /*! @copydoc operator* */
    [[nodiscard]] constexpr pointer operator->() const noexcept {
        return operator*();
    }

    /**
     * @brief Returns the base entity iterator.
     * @return The base entity iterator.
     */
    [[nodiscard]] constexpr iterator_type base() const noexcept {
        return it;
    }

    /**
     * @brief Compares two extended iterators.
     * @tparam Args Types of accessors of the other iterator.
     * @param other The iterator to compare with.
     * @return True if the two iterators are equal, false otherwise.
     */
    template<typename... Args>
    [[nodiscard]] constexpr bool operator==(const extended_iterator<Args...> &other) const noexcept {
        return it == other.it;
    }

private:
    It it;
    stl::tuple<Accessor...> accessor;
};

/**
 * @brief Filtering entity iterator shared by all the views.
 * @tparam Type Common type among all storage types.
 * @tparam Checked True to enable the tombstone check, false otherwise.
 * @tparam Get Number of storage iterated by the view.
 * @tparam Exclude Number of storage used to filter the view.
 */
template<typename Type, bool Checked, stl::size_t Get, stl::size_t Exclude>
class view_iterator final {
    template<typename, typename...>
    friend struct extended_view_iterator;

    using iterator_type = Type::const_iterator;
    using iterator_traits = stl::iterator_traits<iterator_type>;

    [[nodiscard]] bool valid(const iterator_traits::value_type entt) const noexcept {
        return (!Checked || (entt != tombstone))
               && ((Get == 1u) || (internal::all_of(pools.begin(), pools.begin() + index, entt) && internal::all_of(pools.begin() + index + 1, pools.end(), entt)))
               && ((Exclude == 0u) || internal::none_of(filter.begin(), filter.end(), entt));
    }

    void seek_next() {
        for(constexpr iterator_type sentinel{}; it != sentinel && !valid(*it); ++it) {}
    }

public:
    using value_type = iterator_traits::value_type;
    using pointer = iterator_traits::pointer;
    using reference = iterator_traits::reference;
    using difference_type = iterator_traits::difference_type;
    using iterator_category = stl::forward_iterator_tag;

    constexpr view_iterator() noexcept
        : it{},
          pools{},
          filter{},
          index{} {}

    view_iterator(iterator_type first, stl::array<const Type *, Get> value, stl::array<const Type *, Exclude> excl, const stl::size_t idx) noexcept
        : it{first},
          pools{value},
          filter{excl},
          index{static_cast<difference_type>(idx)} {
        ENTT_ASSERT((Get != 1u) || (Exclude != 0u) || pools[0u]->policy() == deletion_policy::in_place, "Non in-place storage view iterator");
        seek_next();
    }

    view_iterator &operator++() noexcept {
        ++it;
        seek_next();
        return *this;
    }

    view_iterator operator++(int) noexcept {
        const view_iterator orig = *this;
        return ++(*this), orig;
    }

    [[nodiscard]] pointer operator->() const noexcept {
        return &*it;
    }

    [[nodiscard]] reference operator*() const noexcept {
        return *operator->();
    }

    template<typename Other, auto... Args>
    [[nodiscard]] constexpr bool operator==(const view_iterator<Other, Args...> &other) const noexcept {
        return it == other.it;
    }

private:
    iterator_type it;
    stl::array<const Type *, Get> pools;
    stl::array<const Type *, Exclude> filter;
    difference_type index;
};

} // namespace internal
/*! @endcond */

} // namespace entt

#endif
