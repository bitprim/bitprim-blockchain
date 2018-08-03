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
#ifndef BITPRIM_BLOCKCHAIN_KEOKEN_STATE_DELEGATED_HPP_
#define BITPRIM_BLOCKCHAIN_KEOKEN_STATE_DELEGATED_HPP_

#include <unordered_map>
#include <vector>

#include <bitcoin/bitcoin/wallet/payment_address.hpp>

// #include <bitprim/keoken/asset_entry.hpp>
// #include <bitprim/keoken/balance.hpp>
// #include <bitprim/keoken/entities/asset.hpp>
// #include <bitprim/keoken/message/create_asset.hpp>
// #include <bitprim/keoken/message/send_tokens.hpp>
// #include <bitprim/keoken/primitives.hpp>
// #include <bitprim/keoken/state_dto.hpp>

namespace bitprim {
namespace keoken {

struct state_delegated {
    using payment_address = bc::wallet::payment_address;
    using get_assets_by_address_list = std::vector<get_assets_by_address_data>;
    using get_assets_list = std::vector<get_assets_data>;
    using get_all_asset_addresses_list = std::vector<get_all_asset_addresses_data>;

    using set_initial_asset_id_func = std::function<void(asset_id_t /*asset_id_initial*/)>;
    using create_asset_func = std::function<void(std::string /*asset_name*/, amount_t /*asset_amount*/, bc::wallet::payment_address const& /*owner*/, size_t /*block_height*/, libbitcoin::hash_digest const& /*txid*/)>;
    using create_balance_entry_func = std::function<void(asset_id_t /*asset_id*/, amount_t /*asset_amount*/, bc::wallet::payment_address const& /*source*/, bc::wallet::payment_address const& /*target*/, size_t /*block_height*/, libbitcoin::hash_digest const& /*txid*/)>;
    using asset_id_exists_func = std::function<bool(asset_id_t /*id*/)>;
    using get_balance_func = std::function<amount_t(asset_id_t id, bc::wallet::payment_address const& addr)>;
    using get_assets_by_address_func = std::function<get_assets_by_address_list(bc::wallet::payment_address const& /*addr*/)>;
    using get_assets_func = std::function<get_assets_list()>;
    using get_all_asset_addresses_func = std::function<get_all_asset_addresses_list()>;

    // explicit
    // state_delegated(asset_id_t asset_id_initial);

    state_delegated() = default;


    // non-copyable class
    state_delegated(state const&) = delete;
    state_delegated operator=(state const&) = delete;

    // Commands.
    // ---------------------------------------------------------------------------------
    set_initial_asset_id_func set_initial_asset_id;
    create_asset_func create_asset;
    create_balance_entry_func create_balance_entry;

    // Queries.
    // ---------------------------------------------------------------------------------
    asset_id_exists_func asset_id_exists;
    get_balance_func get_balance;
    get_assets_by_address_func get_assets_by_address;
    get_assets_func get_assets;
    get_all_asset_addresses_func get_all_asset_addresses;
};

void bind_to_memory_state(state& st, state_delegated& st_del) {
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    st_del.set_initial_asset_id = std::bind(&state::set_initial_asset_id, &st, _1);
    st_del.create_asset         = std::bind(&state::create_asset, &st, _1, _2, _3, _4, _5);
    st_del.create_balance_entry = std::bind(&state::create_balance_entry, &st, _1, _2, _3, _4, _5, _6);

    st_del.asset_id_exists         = std::bind(&state::asset_id_exists, &st, _1);
    st_del.get_balance             = std::bind(&state::get_balance, &st, _1, _2);
    st_del.get_assets_by_address   = std::bind(&state::get_assets_by_address, &st, _1);
    st_del.get_assets              = std::bind(&state::get_assets, &st);
    st_del.get_all_asset_addresses = std::bind(&state::get_all_asset_addresses, &st);
}

} // namespace keoken
} // namespace bitprim

#endif //BITPRIM_BLOCKCHAIN_KEOKEN_STATE_DELEGATED_HPP_
