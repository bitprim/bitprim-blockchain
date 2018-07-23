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
#ifndef BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_
#define BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_

#include <tuple>
#include <unordered_map>
#include <vector>

#include <bitprim/keoken/message/base.hpp>
#include <bitprim/keoken/message/create_asset.hpp>

namespace bitprim {
namespace blockchain {
namespace keoken {

class state {
    constexpr asset_id_t asset_id_initial = 1;
public:    
    using asset_element = std::tuple<domain::asset, size_t, chain::transaction>;
    using asset_list_t = std::vector<asset_element>;

    //TODO: cambiar nombres a los siguientes tipos
    using balance_key = std::tuple<asset_id_t, payment_address>;
    using balance_value = std::tuple<amount_t, size_t, chain::transaction>;
    using balance_t = std::unordered_map<balance_key, balance_value>;

    void create_asset(message::create_asset const& msg, 
                      payment_address const& owner,
                      size_t block_height, libbitcoin::hash_digest const& txid) {
        domain::asset obj(asset_id_, obj.name(), obj.amount());

        // synchro
        asset_list_.emplace_back(obj, block_height, txid);
        balance_.emplace(balance_key{asset_id, owner}, 
                         balance_value{obj.amount(), block_height, txid});
        ++asset_id;
        // synchro end
    }

    void create_balance_entry(message::send_tokens const& msg, 
                              payment_address const& source,
                              payment_address const& target, 
                              size_t block_height, libbitcoin::hash_digest const& txid) {

        // synchro
        balance_.emplace(balance_key{msg.asset_id(), source}, 
                         balance_value{amount_t(-1) * msg.amount(), block_height, txid});

        balance_.emplace(balance_key{msg.asset_id(), target}, 
                         balance_value{msg.amount(), block_height, txid});
        // synchro end
    }

private:
    asset_id_t asset_id = asset_id_initial;
    asset_list_t asset_list_;
    balance_t balance_;
};

} // namespace keoken
} // namespace blockchain
} // namespace bitprim

#endif //BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_
