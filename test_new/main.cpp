/**
 * Copyright (c) 2018 Bitprim developers (see AUTHORS)
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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

//#include <bitprim/keoken/interpreter.hpp>

#include "doctest.h"

#include <bitprim/keoken/state.hpp>

using namespace bitprim::keoken;

TEST_CASE("[state_empty] ") {

    state state_(1);
    //interpreter interpreter_;

    /*  void create_asset(message::create_asset const& msg, 
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
    get_all_asset_addresses_list get_all_asset_addresses() const;*/

    //auto const& ret = state_.get_assets();

    CHECK(true);
    //CHECK( ! state_.asset_id_exists(1));
}


