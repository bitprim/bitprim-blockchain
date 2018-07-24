/**
 * Copyright (c) 2017-2018 Bitprim Inc.
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
using libbitcoin::wallet::payment_address;

namespace bitprim {
namespace keoken {

using domain::amount_t;

void state::create_asset(message::create_asset const& msg, 
                    payment_address const& owner,
                    size_t block_height, libbitcoin::hash_digest const& txid) {
    domain::asset obj(asset_id_next_, msg.name(), msg.amount(), owner);

    // synchro
    asset_list_.emplace_back(obj, block_height, txid);
 
    balance_.emplace(balance_key{asset_id_next_, owner}, 
                     balance_value{balance_entry{obj.amount(), block_height, txid}});
    ++asset_id_next_;
    // synchro end
}

void state::create_balance_entry(message::send_tokens const& msg, 
                            payment_address const& source,
                            payment_address const& target, 
                            size_t block_height, libbitcoin::hash_digest const& txid) {
    // synchro
    balance_[balance_key{msg.asset_id(), source}].emplace_back(domain::amount_t(-1) * msg.amount(), block_height, txid);
    balance_[balance_key{msg.asset_id(), target}].emplace_back(msg.amount(), block_height, txid);
    // synchro end
}

bool state::asset_id_exists(domain::asset_id_t id) const {
    //sincro
    return id < asset_id_next_;      // id > 0 ????
    //sincro end
}

// private
domain::amount_t state::get_balance(balance_value const& entries) const {
    // precondition: sincro???

    //sincro ???
    return std::accumulate(entries.begin(), entries.end(), domain::amount_t(0), [](domain::amount_t bal, balance_entry const& entry) {
        return bal + entry.amount;
    });
    //sincro end
}

domain::amount_t state::get_balance(domain::asset_id_t id, libbitcoin::wallet::payment_address const& addr) const {
    //sincro
    
    auto it = balance_.find(balance_key{id, addr});
    if (it == balance_.end()) {
        return domain::amount_t(0);
    }

    return get_balance(it->second);
    //sincro end
}

state::get_assets_by_address_list state::get_assets_by_address(libbitcoin::wallet::payment_address const& addr) const {
    get_assets_by_address_list res;

    for (auto const& entry : asset_list_) {
        // using balance_key = std::tuple<bitprim::keoken::domain::asset_id_t, libbitcoin::wallet::payment_address>;
        balance_key key {entry.asset.id(), addr};
        auto it = balance_.find(key);
        if (it != balance_.end()) {
            res.emplace_back(entry.asset.id(),
                             entry.asset.name(),
                             entry.asset.owner(),
                             get_balance(it->second)
                            );
        }
    }

    return res;
}

state::get_assets_list state::get_assets() const {
    get_assets_list res;

    for (auto const& entry : asset_list_) {
        res.emplace_back(entry.asset.id(),
                            entry.asset.name(),
                            entry.asset.owner(),
                            entry.asset.amount()
                        );
    }

    return res;
}

domain::asset state::get_asset_by_id(domain::asset_id_t id) const {
    // precondition: id must exists in asset_list_

    auto const cmp = [](asset_entry const& a, asset_entry const& b) {
        return a.asset.id() < b.asset.id();
    };

    auto it = std::lower_bound(asset_list_.begin(), asset_list_.end(), id, cmp);
    return it->asset;

    // if (it != asset_list_.end() && !cmp(id, *it)) {
}

state::get_all_asset_addresses_list state::get_all_asset_addresses() const {
    get_all_asset_addresses_list res;

    for (auto const& bal : balance_) {
        // dic_key = bal.first
        // dic_value = bal.second //vector

        auto asset = get_asset_by_id(std::get<0>(bal.first));

        res.emplace_back(std::get<1>(bal.first),
                         std::get<0>(bal.first),
                         asset.name(),
                         asset.owner(),
                         get_balance(bal.second)
                        );
    }

    return res;


    // 3. listar todas las wallets con tokens en keoken (saldo mayor a cero)
    /*/
        input: NADA
        out  : lista de 
                    - address **
                    - asset id
                    - asset name
                    - addr owner
                    - amount
    */
}




} // namespace keoken
} // namespace bitprim
