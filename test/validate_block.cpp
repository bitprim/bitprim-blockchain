/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
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
#include <boost/test/unit_test.hpp>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::chain;
using namespace bc::blockchain;
using namespace bc::machine;

BOOST_AUTO_TEST_SUITE(validate_block_tests)

BOOST_AUTO_TEST_CASE(validate_block__native__block_438513_tx__valid) {
    //// DEBUG [blockchain] Input validation failed (stack false)
    //// forks        : 62
    //// outpoint     : 8e51d775e0896e03149d585c0655b3001da0c55068b0885139ac6ec34cf76ba0:0
    //// script       : a914faa558780a5767f9e3be14992a578fc1cbcf483087
    //// inpoint      : 6b7f50afb8448c39f4714a73d2b181d3e3233e84670bdfda8f141db668226c54:0
    //// transaction  : 0100000001a06bf74cc36eac395188b06850c5a01d00b355065c589d14036e89e075d7518e000000009d483045022100ba555ac17a084e2a1b621c2171fa563bc4fb75cd5c0968153f44ba7203cb876f022036626f4579de16e3ad160df01f649ffb8dbf47b504ee56dc3ad7260af24ca0db0101004c50632102768e47607c52e581595711e27faffa7cb646b4f481fe269bd49691b2fbc12106ad6704355e2658b1756821028a5af8284a12848d69a25a0ac5cea20be905848eb645fd03d3b065df88a9117cacfeffffff0158920100000000001976a9149d86f66406d316d44d58cbf90d71179dd8162dd388ac355e2658

    static const auto index = 0u;
    static const auto forks = 62u;
    static const auto encoded_script = "a914faa558780a5767f9e3be14992a578fc1cbcf483087";
    static const auto encoded_tx = "0100000001a06bf74cc36eac395188b06850c5a01d00b355065c589d14036e89e075d7518e000000009d483045022100ba555ac17a084e2a1b621c2171fa563bc4fb75cd5c0968153f44ba7203cb876f022036626f4579de16e3ad160df01f649ffb8dbf47b504ee56dc3ad7260af24ca0db0101004c50632102768e47607c52e581595711e27faffa7cb646b4f481fe269bd49691b2fbc12106ad6704355e2658b1756821028a5af8284a12848d69a25a0ac5cea20be905848eb645fd03d3b065df88a9117cacfeffffff0158920100000000001976a9149d86f66406d316d44d58cbf90d71179dd8162dd388ac355e2658";

    data_chunk decoded_tx;
    BOOST_REQUIRE(decode_base16(decoded_tx, encoded_tx));

    data_chunk decoded_script;
    BOOST_REQUIRE(decode_base16(decoded_script, encoded_script));

    transaction tx;
    BOOST_REQUIRE(tx.from_data(decoded_tx));

    const auto& input = tx.inputs()[index];
    auto& prevout = input.previous_output().validation.cache;

    prevout.set_value(0);
    prevout.set_script(script::factory_from_data(decoded_script, false));
    BOOST_REQUIRE(prevout.script().is_valid());

    const auto result = validate_input::verify_script(tx, index, forks);

    BOOST_REQUIRE_EQUAL(result.value(), error::success);

}
#ifdef BITPRIM_CURRENCY_BCH
BOOST_AUTO_TEST_CASE(validate_block__native__block_520679_tx__valid)
{
    //// DEBUG [blockchain] Input validation failed (stack false)
    //// forks        : 62 (?)
    //// outpoint     : dae852c88a00e95141cfe924ac6667a91af87431988d23eff268ea3509d6d83c:1
    //// script       : 76a9149c1093566aa0812e4ea55b5dc3d19a4223fa84d388ac
    //// inpoint      : ace8aaf0ea1c9996171fb2f73dff1f972d9c4b5b916534c32e0ae7f4a011116f:0
    //// transaction  : 01000000013cd8d60935ea68f2ef238d983174f81aa96766ac24e9cf4151e9008ac852e8da010000006a47304402206ccfd8739b2f98350d91ff7fec529f8bc085459b36cf26a22d95606737d4381002204429c60535745ef0b71c14bf0a9df565e8c87b934ee0b2766971cf5b15d085c04121020f123b05aadc865fd60d1513144f48f5d8de3403d3c3f00ce233d53329f10ccaffffffff0156998501000000001976a914bf4679910a2ba81b7f3f2ee03fc77847dc673b2288ac00000000

    static const auto index = 0u;
    static const auto encoded_script = "76a9149c1093566aa0812e4ea55b5dc3d19a4223fa84d388ac";
    static const auto encoded_tx = "01000000013cd8d60935ea68f2ef238d983174f81aa96766ac24e9cf4151e9008ac852e8da010000006a47304402206ccfd8739b2f98350d91ff7fec529f8bc085459b36cf26a22d95606737d4381002204429c60535745ef0b71c14bf0a9df565e8c87b934ee0b2766971cf5b15d085c04121020f123b05aadc865fd60d1513144f48f5d8de3403d3c3f00ce233d53329f10ccaffffffff0156998501000000001976a914bf4679910a2ba81b7f3f2ee03fc77847dc673b2288ac00000000";

    //This value after conversion its equal to the above code.
    uint32_t native_forks = rule_fork::bip16_rule;
    native_forks |= rule_fork::bip65_rule;
    native_forks |= rule_fork::bip66_rule;
    native_forks |= rule_fork::bip112_rule;
    native_forks |= rule_fork::cash_verify_flags_script_enable_sighash_forkid;
    native_forks |= rule_fork::cash_low_s_rule;

    data_chunk decoded_tx;
    BOOST_REQUIRE(decode_base16(decoded_tx, encoded_tx));

    data_chunk decoded_script;
    BOOST_REQUIRE(decode_base16(decoded_script, encoded_script));

    transaction tx;
    BOOST_REQUIRE(tx.from_data(decoded_tx));

    const auto& input = tx.inputs()[index];
    auto& prevout = input.previous_output().validation.cache;

    prevout.set_value(25533210);
    prevout.set_script(script::factory_from_data(decoded_script, false));
    BOOST_REQUIRE(prevout.script().is_valid());

    const auto result = validate_input::verify_script(tx, index, native_forks);
    BOOST_REQUIRE_EQUAL(result.value(), error::success);
}


BOOST_AUTO_TEST_CASE(validate_block__2018NOV__block_520679_tx__valid)
{
    //// DEBUG [blockchain] Input validation failed (stack false)
    // forks        : 1073973119
    // outpoint     : 208fc2edc6fbf4c6cf7fb3ac0c7a1cb23f88fc3ddcced6423ad02d429acb2d07:0
    // script       : 76a9149a45c630ad1ddde200adbf048a929329220dd9a388ac
    // value        : 801932
    // inpoint      : 0375c73a1216188d4dfaed7508740ec1fd601b226de9845325f939348eead1fd:0
    // transaction  : 0100000001072dcb9a422dd03a42d6cedc3dfc883fb21c7a0cacb37fcfc6f4fbc6edc28f20000000006b48304502210099212bdccb2f12d26a1e6d859601bcd76ae3c8861261c6143923937200fa62a40220114e8003a90ffcb6ab3e05641b3daf64006d7bc4f959870f04efb01cad9aa4f3412102822d3e9a0bd0be3f4fab74c2ac9c85f4a0316b331bf92b3c3ef4484975c85e24ffffffff013c000c00000000001976a91463b302f02c2635a4054aa9b43995abbaa28c6f1088ac00000000


    static const auto index = 0u;
    static const auto encoded_script = "76a9149a45c630ad1ddde200adbf048a929329220dd9a388ac";
    static const auto encoded_tx = "0100000001072dcb9a422dd03a42d6cedc3dfc883fb21c7a0cacb37fcfc6f4fbc6edc28f20000000006b48304502210099212bdccb2f12d26a1e6d859601bcd76ae3c8861261c6143923937200fa62a40220114e8003a90ffcb6ab3e05641b3daf64006d7bc4f959870f04efb01cad9aa4f3412102822d3e9a0bd0be3f4fab74c2ac9c85f4a0316b331bf92b3c3ef4484975c85e24ffffffff013c000c00000000001976a91463b302f02c2635a4054aa9b43995abbaa28c6f1088ac00000000";
    static const auto value = 801932;


    //This value after conversion its equal to the above code.
    // static const uint32_t branches = 296831u;
    uint32_t native_forks = rule_fork::bip16_rule;
    native_forks |= rule_fork::bip65_rule;
    native_forks |= rule_fork::bip66_rule;
    native_forks |= rule_fork::bip112_rule;
    native_forks |= rule_fork::cash_verify_flags_script_enable_sighash_forkid;
    native_forks |= rule_fork::cash_low_s_rule;
    native_forks |= rule_fork::cash_checkdatasig;
    native_forks |= rule_fork::cash_schnorr;
    native_forks |= rule_fork::cash_segwit_recovery;


    data_chunk decoded_tx;
    BOOST_REQUIRE(decode_base16(decoded_tx, encoded_tx));

    data_chunk decoded_script;
    BOOST_REQUIRE(decode_base16(decoded_script, encoded_script));

    transaction tx;
    BOOST_REQUIRE(tx.from_data(decoded_tx));

    const auto& input = tx.inputs()[index];
    auto& prevout = input.previous_output().validation.cache;

    prevout.set_value(value);
    prevout.set_script(script::factory_from_data(decoded_script, false));
    BOOST_REQUIRE(prevout.script().is_valid());

    const auto result = validate_input::verify_script(tx, index, native_forks);
    BOOST_REQUIRE_EQUAL(result.value(), error::success);
}
#endif

BOOST_AUTO_TEST_SUITE_END()