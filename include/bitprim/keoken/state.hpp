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
        return h1 ^ (h2 << 1u);
    }
};
} // namespace std
//-----------------------------------------------------------------------------

namespace bitprim {
namespace keoken {

struct asset_entry {
    asset_entry(domain::asset asset, size_t block_height, libbitcoin::hash_digest const& txid)
        : asset(std::move(asset))
        , block_height(block_height)
        , txid(txid)
    {}

    // asset_entry() = default;
    // asset_entry(asset_entry const& x) = default;
    // asset_entry(asset_entry&& x) = default;
    // asset_entry& operator=(asset_entry const& x) = default;
    // asset_entry& operator=(asset_entry&& x) = default;

    domain::asset asset;
    size_t block_height;
    libbitcoin::hash_digest txid;
};

    // auto const cmp = [](asset_entry const& a, domain::asset_id_t id) {
    //     return a.asset.id() < id;
    // };


struct balance_entry {
    balance_entry(domain::amount_t amount, size_t block_height, libbitcoin::hash_digest const& txid)
        : amount(amount), block_height(block_height), txid(txid)
    {}

    // balance_entry() = default;
    // balance_entry(balance_entry const& x) = default;
    // balance_entry(balance_entry&& x) = default;
    // balance_entry& operator=(balance_entry const& x) = default;
    // balance_entry& operator=(balance_entry&& x) = default;

    domain::amount_t amount;
    size_t block_height;
    libbitcoin::hash_digest txid;
};

//TODO(fernando): put DTOs in other source file
struct get_assets_by_address_data {
    get_assets_by_address_data(domain::asset_id_t asset_id, std::string asset_name, libbitcoin::wallet::payment_address asset_creator, domain::amount_t amount)
        : asset_id(asset_id)
        , asset_name(std::move(asset_name))
        , asset_creator(std::move(asset_creator))
        , amount(amount)
    {}

    domain::asset_id_t asset_id;
    std::string asset_name;
    libbitcoin::wallet::payment_address asset_creator;
    domain::amount_t amount;
};

using get_assets_data = get_assets_by_address_data;

struct get_all_asset_addresses_data : get_assets_by_address_data {

    get_all_asset_addresses_data(domain::asset_id_t asset_id, std::string asset_name, libbitcoin::wallet::payment_address asset_creator, domain::amount_t amount, libbitcoin::wallet::payment_address amount_owner)
        : get_assets_by_address_data(asset_id, std::move(asset_name), std::move(asset_creator), amount)
        , amount_owner(std::move(amount_owner))
    {}

    libbitcoin::wallet::payment_address amount_owner;   //TODO(fernando): naming: quien es dueno del saldo
};

class state {
    //TODO(fernando): esto no va acá...
    static constexpr domain::asset_id_t asset_id_initial = 1;
public:    
    using asset_list_t = std::vector<asset_entry>;
    using balance_value = std::vector<balance_entry>;
    using balance_t = std::unordered_map<balance_key, balance_value>;
    using payment_address = libbitcoin::wallet::payment_address;

    using get_assets_by_address_list = std::vector<get_assets_by_address_data>;
    using get_assets_list = std::vector<get_assets_data>;
    using get_all_asset_addresses_list = std::vector<get_all_asset_addresses_data>;


    //TODO(fernando): message::create_asset should not be used here
    void create_asset(message::create_asset const& msg, 
                      payment_address const& owner,
                      size_t block_height, libbitcoin::hash_digest const& txid);

    //TODO(fernando): message::send_tokens should not be used here
    void create_balance_entry(message::send_tokens const& msg, 
                              payment_address const& source,
                              payment_address const& target, 
                              size_t block_height, libbitcoin::hash_digest const& txid);

    bool asset_id_exists(domain::asset_id_t id) const;

    domain::amount_t get_balance(domain::asset_id_t id, payment_address const& addr) const;


    get_assets_by_address_list get_assets_by_address(libbitcoin::wallet::payment_address const& addr) const;
    get_assets_list get_assets() const;
    get_all_asset_addresses_list get_all_asset_addresses() const;

private:
    domain::asset get_asset_by_id(domain::asset_id_t id) const;
    domain::amount_t get_balance(balance_value const& entries) const;

    domain::asset_id_t asset_id_next_ = asset_id_initial;
    asset_list_t asset_list_;
    balance_t balance_;
};

} // namespace keoken
} // namespace bitprim

#endif //BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_
