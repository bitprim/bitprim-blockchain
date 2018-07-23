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

// #include <boost/functional/hash.hpp>

#include <bitcoin/bitcoin/chain/transaction.hpp>
#include <bitcoin/bitcoin/wallet/payment_address.hpp>

#include <bitprim/keoken/domain/asset.hpp>
#include <bitprim/keoken/domain/base.hpp>
#include <bitprim/keoken/message/create_asset.hpp>
#include <bitprim/keoken/message/send_tokens.hpp>


namespace bitprim {
namespace keoken {

using balance_key = std::tuple<bitprim::keoken::domain::asset_id_t, libbitcoin::wallet::payment_address>;

} // namespace keoken
} // namespace bitprim


// Standard hash.
//-----------------------------------------------------------------------------

namespace std {
template <>
struct hash<bitprim::keoken::balance_key> {
    size_t operator()(bitprim::keoken::balance_key const& key) const {
        //Note: if we choose use boost::hash_combine we have to specialize bc::wallet::payment_address in the boost namespace
        // size_t seed = 0;
        // boost::hash_combine(seed, std::get<0>(key));
        // boost::hash_combine(seed, std::get<1>(key));
        // return seed;
        size_t h1 = std::hash<bitprim::keoken::domain::asset_id_t>{}(std::get<0>(key));
        size_t h2 = std::hash<libbitcoin::wallet::payment_address>{}(std::get<1>(key));
        return h1 ^ (h2 << 1);
    }
};
} // namespace std
//-----------------------------------------------------------------------------

namespace bitprim {
namespace keoken {

class state {
    static constexpr domain::asset_id_t asset_id_initial = 1;
public:    
    using asset_element = std::tuple<domain::asset, size_t, libbitcoin::hash_digest>;
    using asset_list_t = std::vector<asset_element>;
    using balance_value_elem = std::tuple<domain::amount_t, size_t, libbitcoin::hash_digest>;
    using balance_value = std::vector<balance_value_elem>;
    using balance_t = std::unordered_map<balance_key, balance_value>;

    void create_asset(message::create_asset const& msg, 
                      libbitcoin::wallet::payment_address const& owner,
                      size_t block_height, libbitcoin::hash_digest const& txid);

    void create_balance_entry(message::send_tokens const& msg, 
                              libbitcoin::wallet::payment_address const& source,
                              libbitcoin::wallet::payment_address const& target, 
                              size_t block_height, libbitcoin::hash_digest const& txid);

    bool asset_id_exists(domain::asset_id_t id) const;

    domain::amount_t get_balance(domain::asset_id_t id, libbitcoin::wallet::payment_address const& addr) const;

private:
    domain::asset_id_t asset_id_next_ = asset_id_initial;
    asset_list_t asset_list_;
    balance_t balance_;
};

} // namespace keoken
} // namespace bitprim

#endif //BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_
