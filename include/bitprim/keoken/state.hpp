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

#include <unordered_map>
#include <vector>

#include <bitcoin/bitcoin/wallet/payment_address.hpp>

#include <bitprim/keoken/asset_entry.hpp>
#include <bitprim/keoken/balance.hpp>
#include <bitprim/keoken/state_dto.hpp>
#include <bitprim/keoken/primitives.hpp>
#include <bitprim/keoken/entities/asset.hpp>
#include <bitprim/keoken/message/create_asset.hpp>
#include <bitprim/keoken/message/send_tokens.hpp>


namespace bitprim {
namespace keoken {

//TODO(fernando): esto no va acá...
// static constexpr asset_id_t asset_id_initial = 1;

class state {
public:    
    using asset_list_t = std::vector<asset_entry>;
    using balance_value = std::vector<balance_entry>;
    using balance_t = std::unordered_map<balance_key, balance_value>;
    using payment_address = libbitcoin::wallet::payment_address;

    using get_assets_by_address_list = std::vector<get_assets_by_address_data>;
    using get_assets_list = std::vector<get_assets_data>;
    using get_all_asset_addresses_list = std::vector<get_all_asset_addresses_data>;

    explicit
    state(asset_id_t asset_id_initial);

    // non-copyable class
    state(state const&) = delete;
    state operator=(state const&) = delete;

    // Commands.
    // ---------------------------------------------------------------------------------
    void create_asset(std::string asset_name, amount_t asset_amount, 
                      payment_address owner,
                      size_t block_height, libbitcoin::hash_digest const& txid);

    void create_balance_entry(asset_id_t asset_id, amount_t asset_amount,
                              payment_address source,
                              payment_address target, 
                              size_t block_height, libbitcoin::hash_digest const& txid);

    // Queries.
    // ---------------------------------------------------------------------------------
    bool asset_id_exists(asset_id_t id) const;
    amount_t get_balance(asset_id_t id, payment_address const& addr) const;
    get_assets_by_address_list get_assets_by_address(libbitcoin::wallet::payment_address const& addr) const;
    get_assets_list get_assets() const;
    get_all_asset_addresses_list get_all_asset_addresses() const;

private:
    entities::asset get_asset_by_id(asset_id_t id) const;
    amount_t get_balance_internal(balance_value const& entries) const;

    asset_id_t asset_id_next_;
    asset_list_t asset_list_;
    balance_t balance_;

    // Synchronization
    mutable boost::shared_mutex mutex_;
};

} // namespace keoken
} // namespace bitprim

#endif //BITPRIM_BLOCKCHAIN_KEOKEN_STATE_HPP_
