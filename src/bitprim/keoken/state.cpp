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

#include <bitprim/keoken/state.hpp>

#include <algorithm>
#include <numeric>

using libbitcoin::data_chunk;
using libbitcoin::hash_digest;
using libbitcoin::wallet::payment_address;

namespace bitprim {
namespace keoken {

state::state(asset_id_t asset_id_initial)
    : asset_id_next_(asset_id_initial)
{}

void state::create_asset(std::string asset_name, amount_t asset_amount, 
                    payment_address owner,
                    size_t block_height, hash_digest const& txid) {

    entities::asset obj(asset_id_next_, std::move(asset_name), asset_amount, owner);

    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    //TODO(fernando): emplace inside a lock? It is a good practice? is construct outside and push preferible?
    asset_list_.emplace_back(std::move(obj), block_height, txid);

    balance_.emplace(balance_key{asset_id_next_, std::move(owner)}, 
                     balance_value{balance_entry{asset_amount, block_height, txid}});
 
    ++asset_id_next_;
}

void state::create_balance_entry(asset_id_t asset_id, amount_t asset_amount,
                            payment_address source,
                            payment_address target, 
                            size_t block_height, hash_digest const& txid) {

    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    //TODO(fernando): emplace inside a lock? It is a good practice? is construct outside and push preferible?
    balance_[balance_key{asset_id, std::move(source)}].emplace_back(amount_t(-1) * asset_amount, block_height, txid);
    balance_[balance_key{asset_id, std::move(target)}].emplace_back(asset_amount, block_height, txid);
}

bool state::asset_id_exists(asset_id_t id) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return id < asset_id_next_;      // id > 0 ????
}

// private
amount_t state::get_balance_internal(balance_value const& entries) const {
    // precondition: mutex_.lock_shared() called
    return std::accumulate(entries.begin(), entries.end(), amount_t(0), [](amount_t bal, balance_entry const& entry) {
        return bal + entry.amount;
    });
}

amount_t state::get_balance(asset_id_t id, libbitcoin::wallet::payment_address const& addr) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    
    auto it = balance_.find(balance_key{id, addr});
    if (it == balance_.end()) {
        return amount_t(0);
    }

    return get_balance_internal(it->second);
}

state::get_assets_by_address_list state::get_assets_by_address(libbitcoin::wallet::payment_address const& addr) const {
    get_assets_by_address_list res;

    {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    for (auto const& entry : asset_list_) {
        // using balance_key = std::tuple<bitprim::keoken::asset_id_t, libbitcoin::wallet::payment_address>;
        balance_key key {entry.asset.id(), addr};
        auto it = balance_.find(key);
        if (it != balance_.end()) {
            res.emplace_back(entry.asset.id(),
                             entry.asset.name(),
                             entry.asset.owner(),
                             get_balance_internal(it->second)
                            );
        }
    }
    }

    return res;
}

state::get_assets_list state::get_assets() const {
    get_assets_list res;

    {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);

    for (auto const& entry : asset_list_) {
        res.emplace_back(entry.asset.id(),
                            entry.asset.name(),
                            entry.asset.owner(),
                            entry.asset.amount()
                        );
    }
    }

    return res;
}

// private
entities::asset state::get_asset_by_id(asset_id_t id) const {
    // precondition: id must exists in asset_list_
    // precondition: mutex_.lock_shared() called

    auto const cmp = [](asset_entry const& a, asset_id_t id) {
        return a.asset.id() < id;
    };

    auto it = std::lower_bound(asset_list_.begin(), asset_list_.end(), id, cmp);
    return it->asset;

    // if (it != asset_list_.end() && !cmp(id, *it)) {
}

state::get_all_asset_addresses_list state::get_all_asset_addresses() const {
    get_all_asset_addresses_list res;

    {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);

    for (auto const& bal : balance_) {
        auto const& bal_key = bal.first;
        auto const& bal_value = bal.second;
        auto const& asset_id = std::get<0>(bal_key);
        auto const& amount_owner = std::get<1>(bal_key);

        auto asset = get_asset_by_id(asset_id);

        res.emplace_back(asset_id,
                         asset.name(),
                         asset.owner(),
                         get_balance_internal(bal_value),
                         amount_owner
                        );
    }
    }

    return res;
}

} // namespace keoken
} // namespace bitprim
