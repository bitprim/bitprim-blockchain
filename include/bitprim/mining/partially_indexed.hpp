/**
 * Copyright (c) 2016-2018 Bitprim Inc.
 *
 * This file is part of Bitprim.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef BITPRIM_BLOCKCHAIN_MINING_PARTIALLY_INDEXED_HPP_
#define BITPRIM_BLOCKCHAIN_MINING_PARTIALLY_INDEXED_HPP_

// #include <algorithm>
// #include <chrono>
// #include <tuple>
// #include <unordered_map>
// #include <unordered_set>
// #include <type_traits>
#include <list>
#include <vector>

#define BITPRIM_MEMPOOL_USE_LIST


#include <bitprim/mining/common.hpp>
#include <bitprim/mining/node.hpp>
// #include <bitprim/mining/result_code.hpp>

#include <bitcoin/bitcoin.hpp>


namespace libbitcoin {
namespace mining {

using main_index_t = size_t;


template <typename I, typename T>
    // requires(Regular<T>)
class partially_indexed_node {
public:
    using value_type = T;

    partially_indexed_node(I index, T const& x) 
        : index_(index)
        , x_(x)
    {}

    partially_indexed_node(I index, T&& x) 
        : index_(index)
        , x_(std::move(x))
    {}

    // template <typename... Args>
    // partially_indexed_node(std::piecewise_construct_t, I index, Args&&... args)
    //     : index_(index)
    //     , x_(std::forward<Args>(args)...)
    // {}

    template <typename... Args>
    partially_indexed_node(I index, Args&&... args)
        : index_(index)
        , x_(std::forward<Args>(args)...)
    {}

    I const& index() const {
        return index_;
    }

    T const& element() const {
        return x_;
    }

    T& element() {
        return x_;
    }

    void set_index(I index) {
        index_ = index;
    }

    void set_element(T const& x) {
        x_ = x;
    }

    void set_element(T&& x) {
        x_ = std::move(x);
    }

private:
    I index_;
    T x_;
};

template <typename T, typename Cmp, typename State>
    // requires(Regular<T>)
class partially_indexed {
public:
    using indexes_container_t = std::list<main_index_t>;
    using candidate_index_t = indexes_container_t::iterator; //TODO(fernando): const iterator?
    using value_type = partially_indexed_node<candidate_index_t, T>;
    using main_container_t = std::vector<value_type>;

    partially_indexed(Cmp cmp, State& state) 
        : null_index_(std::end(candidate_elements_))
        , sorted_(true)
        , cmp_(cmp)
        , state_(state)
    {
        //TODO(fernando): reserve structures, delegate to user
    }

    void reserve(size_t all, size_t candidates) {
        all_elements_.reserve(all);
        candidate_elements_.reserve(candidates);
    }

    bool insert(T const& x) {
        auto res = insert_internal(x);
        check_invariant();
        return res;
    }

    bool insert(T&& x) {
        auto res =  insert_internal(std::move(x));
        check_invariant();
        return res;
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        auto res =  insert_internal(std::forward<Args>(args)...);
        check_invariant();
        return res;
    }

    size_t all_size() const {
        return all_elements_.size();
    }

    size_t candidates_size() const {
        return candidate_elements_.size();
    }

private:
    struct candidate_cmp_t;
    struct sorter_t;

    candidate_cmp_t candidate_cmp() const {
        return candidate_cmp_t{*this};
    }

    sorter_t sorter() {
        return sorter_t{*this};
    }

    struct nested_t {
        explicit
        nested_t(partially_indexed& x)
            : outer_(x)
        {}
      
        partially_indexed& outer() {
            return outer_;
        }

        partially_indexed const& outer() const {
            return outer_;
        }

    private:
        partially_indexed& outer_;
    };


    struct nested_const_t {
        explicit
        nested_const_t(partially_indexed const& x)
            : outer_(x)
        {}
      
        partially_indexed const& outer() const {
            return outer_;
        }

    private:
        partially_indexed const& outer_;
    };

    struct candidate_cmp_t : nested_const_t {
        using nested_const_t::outer;
        using nested_const_t::nested_const_t;

        bool operator()(main_index_t a, main_index_t b) const {
            auto const& elem_a = outer().all_elements_[a].element();
            auto const& elem_b = outer().all_elements_[b].element();
            return outer().cmp_(elem_a, elem_b);
        }
    };

    struct remover_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t i) {
            auto& node = outer().all_elements_[i];
            outer().candidate_elements_.erase(node.index());
            node.set_index(outer().null_index_);
        }
    };

    struct inserter_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t i) {
            auto& node = outer().all_elements_[i];
            
            // candidate_elements_.push_back(i);
            // auto new_cand_index = std::prev(std::end(candidate_elements_));
            // node.set_index(new_cand_index);

            // auto start = std::chrono::high_resolution_clock::now();
            auto it = std::upper_bound(std::begin(outer().candidate_elements_), std::end(outer().candidate_elements_), i, outer().candidate_cmp());
            // auto end = std::chrono::high_resolution_clock::now();
            // auto time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            // binary_search_time += time_ns;

            if (it == std::end(outer().candidate_elements_)) {
                outer().candidate_elements_.push_back(i);
                auto cand_index = std::prev(std::end(outer().candidate_elements_));
                node.set_index(cand_index);
            } else {
                auto cand_index = outer().candidate_elements_.insert(it, i);
                node.set_index(cand_index);
            }
        }
    };

    struct getter_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        std::pair<bool, T&> operator()(main_index_t i) {
            auto& node = outer().all_elements_[i];
            return {node.index() != outer().null_index_, node.element()};
        }
    };

    struct getter_const_t : nested_const_t {
        using nested_const_t::outer;
        using nested_const_t::nested_const_t;

        std::pair<bool, T const&> operator()(main_index_t i) const {
            auto& node = outer().all_elements_[i];
            return {node.index() != outer().null_index_, node.element()};
        }
    };

    struct bounds_ok_t : nested_const_t {
        using nested_const_t::outer;
        using nested_const_t::nested_const_t;

        bool operator()(main_index_t i) const {
            return i < outer().all_elements_.size();
        }
    };


    struct sorter_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t index, value_type const& x, candidate_index_t from, candidate_index_t to) {
            auto new_pos = std::upper_bound(from, to, index, outer().candidate_cmp());
            outer().candidate_elements_.splice(new_pos, outer().candidate_elements_, x.index());
        }
    };

    struct re_sort_left_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t index) {
            auto const& node = outer().all_elements_[index];
            auto from = std::begin(outer().candidate_elements_);
            auto to = node.index();
            outer().sorter()(index, node, from, to);

            // auto new_pos = std::upper_bound(from, to, index, outer().candidate_cmp());
            // // reindex_increment(new_pos, it);
            // // std::rotate(new_pos, it, it + 1);
            // // parent.set_candidate_index(std::distance(std::begin(candidate_transactions_), new_pos));

            // outer().candidate_elements_.splice(new_pos, outer().candidate_elements_, node.index());

            
        }
    };

    struct re_sort_right_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t index) {
            auto const& node = outer().all_elements_[index];
            auto from = std::next(node.index());
            auto to = std::end(outer().candidate_elements_);
            outer().sorter()(index, node, from, to);

            // auto new_pos = std::upper_bound(from, to, index, outer().candidate_cmp());
            // // reindex_decrement(from, new_pos);
            // // it = std::rotate(it, it + 1, new_pos);
            // // parent.set_candidate_index(std::distance(std::begin(candidate_transactions_), it));

            // outer().candidate_elements_.splice(new_pos, outer().candidate_elements_, node.index());
        }
    };

    struct re_sort_to_end_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t from_index, main_index_t find_index) {
            auto const& find_node = outer().all_elements_[find_index];
            auto const& from_node = outer().all_elements_[from_index];

            auto from = std::next(from_node.index());
            auto to = std::end(outer().candidate_elements_);
            outer().sorter()(find_index, find_node, from, to);
        }
    };

    struct re_sort_from_begin_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t to_index, main_index_t find_index) {
            auto const& find_node = outer().all_elements_[find_index];
            auto const& to_node = outer().all_elements_[to_index];

            auto from = std::begin(outer().candidate_elements_);
            auto to = to_node.index();
            outer().sorter()(find_index, find_node, from, to);
        }
    };

    struct re_sort_t : nested_t {
        using nested_t::outer;
        using nested_t::nested_t;

        void operator()(main_index_t from_index, main_index_t to_index, main_index_t find_index) {
            auto const& find_node = outer().all_elements_[find_index];
            auto const& from_node = outer().all_elements_[from_index];
            auto const& to_node = outer().all_elements_[to_index];

            auto from = std::next(from_node.index());
            auto to = to_node.index();
            outer().sorter()(find_index, find_node, from, to);
        }
    };

    template <typename I>
    struct reverser_t : nested_t {

        // reverser_t(main_container_t& all_elements, I f, I l)
        //     : all_elements_(all_elements)
        //     , f(f)
        //     , l(l)
        // {}

        using nested_t::outer;

        reverser_t(partially_indexed& x, I f, I l)
            : nested_t(x)
            , f(f)
            , l(l)
        {}

        bool has_next() const {
            return f != l;
        }

        std::pair<main_index_t, T&> next() {
            --l;
            return {*l, outer().all_elements_[*l].element()};
        }

    private:
        // main_container_t& all_elements_;
        I const f;
        I l;
    };

    // template <typename I>
    // reverser_t<I> reverser(main_container_t& all_elements, I f, I l) {
    //     return reverser_t<I>(all_elements, f, l);
    // }


    template <typename I>
    reverser_t<I> reverser(partially_indexed& x, I f, I l) {
        return reverser_t<I>(x, f, l);
    }

    reverser_t<candidate_index_t> reverser() {
        return reverser(*this, std::begin(candidate_elements_), std::end(candidate_elements_));
    }


    value_type& insert_main_element(candidate_index_t index, T const& x) {
        all_elements_.emplace_back(index, x);
        return all_elements_.back();
    }

    value_type& insert_main_element(candidate_index_t index, T&& x) {
        all_elements_.emplace_back(index, std::move(x));
        return all_elements_.back();
    }

    template <typename... Args>
    value_type& insert_main_element(candidate_index_t index, Args&&... args) {
        all_elements_.emplace_back(index, std::forward<Args>(args)...);
        return all_elements_.back();
    }


// ----------------------------------------------------------------------------------------
//  Invariant Checks
// ----------------------------------------------------------------------------------------

    void check_invariant_partial() const {
        
        BOOST_ASSERT(candidate_elements_.size() <= all_elements_.size());

        {
            for (auto i : candidate_elements_) {
                if (i >= all_elements_.size()) {
                    BOOST_ASSERT(false);
                }
            }
        }

        // {
        //     for (auto const& node : all_elements_) {
        //         if (node.index() != null_index_ && node.index() >= candidate_elements_.size()) {
        //             BOOST_ASSERT(false);
        //         }
        //     }
        // }

        {
            auto ci_sorted = candidate_elements_;
            ci_sorted.sort();
            auto last = std::unique(std::begin(ci_sorted), std::end(ci_sorted));
            BOOST_ASSERT(std::distance(std::begin(ci_sorted), last) == ci_sorted.size());
        }
        
        {
            std::vector<main_index_t> all_sorted;
            for (auto const& node : all_elements_) {
                if (node.index() != null_index_) {
                    all_sorted.push_back(*(node.index()));
                }
            }
            std::sort(std::begin(all_sorted), std::end(all_sorted));
            auto last = std::unique(std::begin(all_sorted), std::end(all_sorted));
            BOOST_ASSERT(std::distance(std::begin(all_sorted), last) == all_sorted.size());
        }

        {
            auto it = std::begin(candidate_elements_);
            auto end = std::end(candidate_elements_);
            while (it != end) {
                auto const& node = all_elements_[*it];
                BOOST_ASSERT(it == node.index());
                ++it;
            }
        }

        {
            size_t i = 0;
            size_t non_indexed = 0;
            for (auto const& node : all_elements_) {
                if (node.index() != null_index_) {
                    BOOST_ASSERT(*(node.index()) == i);
                } else {
                    ++non_indexed;
                }
                ++i;
            }

            BOOST_ASSERT(candidate_elements_.size() + non_indexed == all_elements_.size());
        }
    }

    void check_invariant() const {
        check_invariant_partial();

        {
            getter_const_t g{*this};
            bounds_ok_t b{*this};
            // size_t ci = 0;
            for (auto i : candidate_elements_) {
                auto const& node = all_elements_[i];
                // check_children_accum(i);
                BOOST_ASSERT(state_.check_node(i, g, b));
            }
        }

        {
            getter_const_t g{*this};
            bounds_ok_t b{*this};
            size_t i = 0;
            for (auto const& node : all_elements_) {
                // check_children_accum(i);
                BOOST_ASSERT(state_.check_node(i, g, b));
                ++i;
            }
        }        


        {
            if (sorted_) {
                auto res = std::is_sorted(std::begin(candidate_elements_), std::end(candidate_elements_), candidate_cmp());

                // if (! res) {
                //     auto res2 = std::is_sorted(std::begin(candidate_elements_), std::end(candidate_elements_), candidate_cmp());
                //     std::cout << res2;
                // }

                BOOST_ASSERT(res);

            }
        }        
    }
// ----------------------------------------------------------------------------------------
//  Invariant Checks (End)
// ----------------------------------------------------------------------------------------



    // std::cout << all_elements_[*cand_index].element().fee() << "\n";
    // for (auto mi : candidate_elements_) {
    //     auto ci = all_elements_[mi].index();
    //     std::cout << all_elements_[mi].element().fee() << "\n";
    //     if (mi == *ci) {
    //         std::cout << "OK\n";
    //     } else {
    //         std::cout << "Error\n";
    //     }
    // }


    template <typename... Args>
    bool insert_internal(Args&&... args) {
        auto const main_index = all_elements_.size();
        
        // candidate_elements_.push_back(main_index);
        // auto cand_index = std::prev(std::end(candidate_elements_));
        // auto& inserted = insert_main_element(cand_index, std::forward<Args>(args)...);

        auto& inserted = insert_main_element(null_index_, std::forward<Args>(args)...);

        if (state_.has_room_for(inserted.element()) ) {

            candidate_elements_.push_back(main_index);
            auto cand_index = std::prev(std::end(candidate_elements_));
            inserted.set_index(cand_index);

            state_.accumulate(inserted.element());
            sorted_ = false;
        } else {
            
            if ( ! sorted_) {
                //Si los elementos no-ordenados son pocos, ver de usar un algoritmo más eficiente.
                // candidate_elements_.sort(candidate_cmp_t{*this});
                candidate_elements_.sort(candidate_cmp());
                sorted_ = true;
                auto rev = reverser();
                state_.remove_insert_several(inserted.element(), main_index, rev, remover_t{*this}, getter_t{*this}, inserter_t{*this}, re_sort_left_t{*this}, re_sort_right_t{*this}, re_sort_to_end_t{*this}, re_sort_t{*this}, re_sort_from_begin_t{*this});


                // std::cout << all_elements_[*cand_index].element().fee() << "\n";

                //Quiere decir que en el estado previo, no había ningun elemento no-indexado (todos estaban indexados).
                //Por lo tanto tengo que  (en el "mejor" de los casos)
                // remover algunos y sólo insertar el que acabo de insertar.
            } else {
                // state_.remove_insert_several(inserted.element(), rev, [](main_index_t i){
                //     std::cout << i << std::endl;
                // });
                // remove elements
                // could return false
                // in that case, remove the recently added element from candidate_elements_
            }
        }


        // insert_in_candidates(x, main_index);
        return true;
    }
    // template <typename U>
    //     //requires (SameType<T, U>)
    // bool insert_internal(U&& x) {
    //     auto const main_index = all_elements_.size();
    //     candidate_elements_.push_back(main_index);

    //     auto& inserted = insert_main_element(std::forward<U>(x));

    //     if ( ! state_(x) ) {
    //         // remove elements
    //         // could return false
    //         // in that case, remove the recently added element from candidate_elements_
    //     }

    //     // insert_in_candidates(x, main_index);
    //     return true;
    // }

    // void add_in_candidates(T const& x, main_index_t main_index) {
    // }
    
private:
    indexes_container_t candidate_elements_;
    main_container_t all_elements_;
    candidate_index_t const null_index_;    //TODO(fernando): const iterator??
    bool sorted_;
    Cmp cmp_;
    State& state_;
};

}  // namespace mining
}  // namespace libbitcoin

#endif  //BITPRIM_BLOCKCHAIN_MINING_PARTIALLY_INDEXED_HPP_
