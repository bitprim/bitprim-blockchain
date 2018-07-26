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

#include "doctest.h"

#include <bitprim/keoken/state.hpp>

using namespace bitprim::keoken;
using libbitcoin::hash_digest;
using libbitcoin::hash_literal;
using libbitcoin::wallet::payment_address;

TEST_CASE("[state_asset_id_exists_empty] ") {

    state state_(0);
    CHECK( ! state_.asset_id_exists(0));
    CHECK( ! state_.asset_id_exists(1));
}

TEST_CASE("[state_asset_id_exists_not_empty] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address addr("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, addr, height, txid);
    
    CHECK(state_.asset_id_exists(0));
}

TEST_CASE("[state_get_assets_empty] ") {

    state state_(1);
    auto const& ret = state_.get_assets();
    CHECK(ret.size() == 0);
}


TEST_CASE("[state_get_assets_not_empty] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address addr("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, addr, height, txid);

    auto const& ret = state_.get_assets();
    CHECK(ret.size() == 1);

    auto const new_asset = ret[0];

    CHECK(new_asset.asset_id == 0);
    CHECK(new_asset.asset_name == name);
    CHECK(new_asset.amount == amount);
    CHECK(new_asset.asset_creator == addr);
}

TEST_CASE("[state_create_asset] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address addr("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, addr, height, txid);
    
    auto const& ret = state_.get_assets();

    auto const new_asset = ret[0];

    CHECK(new_asset.asset_id == 0);
    CHECK(new_asset.asset_name == name);
    CHECK(new_asset.amount == amount);
    CHECK(new_asset.asset_creator == addr);

    auto const& balance = state_.get_balance(0,addr);

     CHECK(balance == 1559);
}

TEST_CASE("[state_create_balance_entry] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address source("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    payment_address destination("1CK6KHY6MHgYvmRQ4PAafKYDrg1ejbH1cE");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, source, height, txid);
    
    domain::amount_t amount_to_transfer = 5;
    state_.create_balance_entry(0, amount_to_transfer, source, destination, height, txid );

    auto const& balance = state_.get_balance(0,source);

     CHECK(balance == 1554);

     auto const& balance_destination = state_.get_balance(0,destination);

     CHECK(balance_destination == 5);
}


TEST_CASE("[state_get_assets_by_address] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address source("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    payment_address destination("1CK6KHY6MHgYvmRQ4PAafKYDrg1ejbH1cE");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, source, height, txid);
    
    domain::amount_t amount_to_transfer = 5;
    state_.create_balance_entry(0, amount_to_transfer, source, destination, height, txid );

    auto const& list_source = state_.get_assets_by_address(source);
    auto const& list_destination= state_.get_assets_by_address(destination);

    CHECK(list_source.size() == 1);
    CHECK(list_destination.size() == 1);

    auto const asset_1 = list_source[0];

    CHECK(asset_1.asset_id == 0);
    CHECK(asset_1.asset_name == name);
    CHECK(asset_1.amount == amount - amount_to_transfer);
    CHECK(asset_1.asset_creator == source);

    auto const asset_2 = list_destination[0];

    CHECK(asset_2.asset_id == 0);
    CHECK(asset_2.asset_name == name);
    CHECK(asset_2.amount == amount_to_transfer);
    CHECK(asset_2.asset_creator == source);
}


TEST_CASE("[state_get_all_asset_addresses] ") {

    state state_(0);

    std::string name = "Test";
    domain::amount_t amount = 1559;
    size_t height = 456;
    payment_address source("moNQd8TVGogcLsmPzNN2QdFwDfcAZFfUCr");
    payment_address destination("1CK6KHY6MHgYvmRQ4PAafKYDrg1ejbH1cE");
    const hash_digest txid = hash_literal("8b4b9487199ed6668cf6135f29f832c215ab8d32a32c323923594e7475dece25");

    state_.create_asset(name, amount, source, height, txid);
    
    domain::amount_t amount_to_transfer = 5;
    state_.create_balance_entry(0, amount_to_transfer, source, destination, height, txid );

    auto const& list_source = state_.get_all_asset_addresses();
    
    CHECK(list_source.size() == 2);

    auto const asset_1 = list_source[0];

    CHECK(asset_1.asset_id == 0);
    CHECK(asset_1.asset_name == name);
    CHECK(asset_1.amount == amount - amount_to_transfer);
    CHECK(asset_1.asset_creator == source);
    CHECK(asset_1.amount_owner == source);

    auto const asset_2 = list_source[1];

    CHECK(asset_2.asset_id == 0);
    CHECK(asset_2.asset_name == name);
    CHECK(asset_2.amount == amount_to_transfer);
    CHECK(asset_2.asset_creator == source);
}


