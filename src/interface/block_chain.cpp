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
#include <bitcoin/blockchain/interface/block_chain.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <unordered_set>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database.hpp>
#include <bitcoin/bitcoin/multi_crypto_support.hpp>
#include <bitcoin/blockchain/settings.hpp>
#include <bitcoin/blockchain/populate/populate_chain_state.hpp>
#include <boost/thread/latch.hpp>

namespace libbitcoin { namespace blockchain {

using spent_value_type = std::pair<hash_digest, uint32_t>;
//using spent_container = std::vector<spent_value_type>;
using spent_container = std::unordered_set<spent_value_type>;

}}

namespace std {

template <>
struct hash<libbitcoin::blockchain::spent_value_type> {
    size_t operator()(libbitcoin::blockchain::spent_value_type const& point) const {
        size_t seed = 0;
        boost::hash_combine(seed, point.first);
        boost::hash_combine(seed, point.second);
        return seed;
    }
};

} // namespace std

namespace libbitcoin {
namespace blockchain {

using namespace bc::config;
using namespace bc::message;
using namespace bc::database;
using namespace std::placeholders;

#define NAME "block_chain"

static const auto hour_seconds = 3600u;

block_chain::block_chain(threadpool& pool,
    const blockchain::settings& chain_settings,
    const database::settings& database_settings, bool relay_transactions)
  : stopped_(true),
    settings_(chain_settings),
    notify_limit_seconds_(chain_settings.notify_limit_hours * hour_seconds),
    chain_state_populator_(*this, chain_settings),
    database_(database_settings),
    validation_mutex_(database_settings.flush_writes && relay_transactions),
    priority_pool_(thread_ceiling(chain_settings.cores),
        priority(chain_settings.priority)),
    dispatch_(priority_pool_, NAME "_priority"),
    transaction_organizer_(validation_mutex_, dispatch_, pool, *this,
        chain_settings),
    block_organizer_(validation_mutex_, dispatch_, pool, *this, chain_settings,
        relay_transactions)
{
}

// ============================================================================
// FAST CHAIN
// ============================================================================

// Readers.
// ----------------------------------------------------------------------------

uint32_t get_clock_now() {
    auto const now = std::chrono::high_resolution_clock::now();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
}

bool block_chain::get_gaps(block_database::heights& out_gaps) const
{
    database_.blocks().gaps(out_gaps);
    return true;
}

bool block_chain::get_block_exists(const hash_digest& block_hash) const
{
    return database_.blocks().get(block_hash);
}

bool block_chain::get_block_hash(hash_digest& out_hash, size_t height) const
{
    const auto result = database_.blocks().get(height);

    if (!result)
        return false;

    out_hash = result.hash();
    return true;
}

bool block_chain::get_branch_work(uint256_t& out_work,
    const uint256_t& maximum, size_t from_height) const
{
    size_t top;
    if (!database_.blocks().top(top))
        return false;

    out_work = 0;
    for (auto height = from_height; height <= top && out_work < maximum;
        ++height)
    {
        const auto result = database_.blocks().get(height);
        if (!result)
            return false;

        out_work += chain::header::proof(result.bits());
    }

    return true;
}

bool block_chain::get_header(chain::header& out_header, size_t height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_header = result.header();
    return true;
}

bool block_chain::get_height(size_t& out_height,
    const hash_digest& block_hash) const
{
    auto result = database_.blocks().get(block_hash);
    if (!result)
        return false;

    out_height = result.height();
    return true;
}

bool block_chain::get_bits(uint32_t& out_bits, const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_bits = result.bits();
    return true;
}

bool block_chain::get_timestamp(uint32_t& out_timestamp,
    const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_timestamp = result.timestamp();
    return true;
}

bool block_chain::get_version(uint32_t& out_version,
    const size_t& height) const
{
    auto result = database_.blocks().get(height);
    if (!result)
        return false;

    out_version = result.version();
    return true;
}

bool block_chain::get_last_height(size_t& out_height) const
{
    return database_.blocks().top(out_height);
}

bool block_chain::get_output(chain::output& out_output, size_t& out_height,
    uint32_t& out_median_time_past, bool& out_coinbase,
    const chain::output_point& outpoint, size_t branch_height,
    bool require_confirmed) const
{
    // This includes a cached value for spender height (or not_spent).
    // Get the highest tx with matching hash, at or below the branch height.
    return database_.transactions().get_output(out_output, out_height,
        out_median_time_past, out_coinbase, outpoint, branch_height,
        require_confirmed);
}

bool block_chain::get_output_is_confirmed(chain::output& out_output, size_t& out_height,
                             bool& out_coinbase, bool& out_is_confirmed, const chain::output_point& outpoint,
                             size_t branch_height, bool require_confirmed) const
{
    // This includes a cached value for spender height (or not_spent).
    // Get the highest tx with matching hash, at or below the branch height.
    return database_.transactions().get_output_is_confirmed(out_output, out_height,
                                               out_coinbase, out_is_confirmed, outpoint, branch_height, require_confirmed);
}

bool block_chain::get_is_unspent_transaction(const hash_digest& hash,
    size_t branch_height, bool require_confirmed) const
{
    const auto result = database_.transactions().get(hash, branch_height,
        require_confirmed);

    return result && !result.is_spent(branch_height);
}

bool block_chain::get_transaction_position(size_t& out_height,
    size_t& out_position, const hash_digest& hash,
    bool require_confirmed) const
{
    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
        return false;

    out_height = result.height();
    out_position = result.position();
    return true;
}

////transaction_ptr block_chain::get_transaction(size_t& out_block_height,
////    const hash_digest& hash, bool require_confirmed) const
////{
////    const auto result = database_.transactions().get(hash, max_size_t,
////        require_confirmed);
////
////    if (!result)
////        return nullptr;
////
////    out_block_height = result.height();
////    return std::make_shared<transaction>(result.transaction());
////}

// Writers
// ----------------------------------------------------------------------------

bool block_chain::begin_insert() const
{
    return database_.begin_insert();
}

bool block_chain::end_insert() const
{
    return database_.end_insert();
}

bool block_chain::insert(block_const_ptr block, size_t height)
{
    return database_.insert(*block, height) == error::success;
}

void block_chain::push(transaction_const_ptr tx, dispatcher&,
    result_handler handler)
{
    last_transaction_.store(tx);

    // Transaction push is currently sequential so dispatch is not used.
    handler(database_.push(*tx, chain_state()->enabled_forks()));
}

void block_chain::reorganize(const checkpoint& fork_point,
    block_const_ptr_list_const_ptr incoming_blocks,
    block_const_ptr_list_ptr outgoing_blocks, dispatcher& dispatch,
    result_handler handler)
{
    if (incoming_blocks->empty())
    {
        handler(error::operation_failed);
        return;
    }

    // The top (back) block is used to update the chain state.
    const auto complete =
        std::bind(&block_chain::handle_reorganize,
            this, _1, incoming_blocks->back(), handler);

    database_.reorganize(fork_point, incoming_blocks, outgoing_blocks,
        dispatch, complete);
}

void block_chain::handle_reorganize(const code& ec, block_const_ptr top,
    result_handler handler)
{
    if (ec)
    {
        handler(ec);
        return;
    }

    if (!top->validation.state)
    {
        handler(error::operation_failed);
        return;
    }

    set_chain_state(top->validation.state);
    last_block_.store(top);

    handler(error::success);
}

// Properties.
// ----------------------------------------------------------------------------

// For tx validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(pool_state_mutex_);

    // Initialized on start and updated after each successful organization.
    return pool_state_;
    ///////////////////////////////////////////////////////////////////////////
}

// For block validator, call only from inside validate critical section.
chain::chain_state::ptr block_chain::chain_state(
    branch::const_ptr branch) const
{
    // Promote from cache if branch is same height as pool (most typical).
    // Generate from branch/store if the promotion is not successful.
    // If the organize is successful pool state will be updated accordingly.
    return chain_state_populator_.populate(chain_state(), branch);
}

// private.
code block_chain::set_chain_state(chain::chain_state::ptr previous)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(pool_state_mutex_);

    pool_state_ = chain_state_populator_.populate(previous);
    return pool_state_ ? error::success : error::operation_failed;
    ///////////////////////////////////////////////////////////////////////////
}

// ============================================================================
// SAFE CHAIN
// ============================================================================

// Startup and shutdown.
// ----------------------------------------------------------------------------

bool block_chain::start()
{
    stopped_ = false;

    if (!database_.open())
        return false;

    // Initialize chain state after database start but before organizers.
    pool_state_ = chain_state_populator_.populate();

    return pool_state_ && transaction_organizer_.start() &&
        block_organizer_.start();
}

bool block_chain::stop()
{
    stopped_ = true;

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    validation_mutex_.lock_high_priority();

    // This cannot call organize or stop (lock safe).
    auto result = transaction_organizer_.stop() && block_organizer_.stop();

    // The priority pool must not be stopped while organizing.
    priority_pool_.shutdown();

    validation_mutex_.unlock_high_priority();
    ///////////////////////////////////////////////////////////////////////////
    return result;
}

// Close is idempotent and thread safe.
// Optional as the blockchain will close on destruct.
bool block_chain::close()
{
    const auto result = stop();
    priority_pool_.join();
    return result && database_.close();
}

block_chain::~block_chain()
{
    close();
}

// Queries.
// ----------------------------------------------------------------------------
// Blocks are and transactions returned const because they don't change and
// this eliminates the need to copy the cached items.

void block_chain::fetch_block(size_t height, block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->validation.state &&
        cached->validation.state->height() == height)
    {
        handler(error::success, cached, height);
        return;
    }

    const auto block_result = database_.blocks().get(height);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    BITCOIN_ASSERT(block_result.height() == height);
    const auto tx_hashes = block_result.transaction_hashes();
    const auto& tx_store = database_.transactions();
    transaction::list txs;
    txs.reserve(tx_hashes.size());
    DEBUG_ONLY(size_t position = 0;)

    for (const auto& hash: tx_hashes)
    {
        const auto tx_result = tx_store.get(hash, max_size_t, true);

        if (!tx_result)
        {
            handler(error::operation_failed, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(tx_result.height() == height);
        BITCOIN_ASSERT(tx_result.position() == position++);
        txs.push_back(tx_result.transaction());
    }

    auto message = std::make_shared<const block>(block_result.header(),
        std::move(txs));
    handler(error::success, message, height);
}

void block_chain::fetch_block(const hash_digest& hash,
    block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto cached = last_block_.load();

    // Try the cached block first.
    if (cached && cached->validation.state && cached->hash() == hash)
    {
        handler(error::success, cached, cached->validation.state->height());
        return;
    }

    const auto block_result = database_.blocks().get(hash);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto height = block_result.height();
    const auto tx_hashes = block_result.transaction_hashes();
    const auto& tx_store = database_.transactions();
    transaction::list txs;
    txs.reserve(tx_hashes.size());
    DEBUG_ONLY(size_t position = 0;)

    for (const auto& hash: tx_hashes)
    {
        const auto tx_result = tx_store.get(hash, max_size_t, true);

        if (!tx_result)
        {
            handler(error::operation_failed, nullptr, 0);
            return;
        }

        BITCOIN_ASSERT(tx_result.height() == height);
        BITCOIN_ASSERT(tx_result.position() == position++);
        txs.push_back(tx_result.transaction());
    }

    const auto message = std::make_shared<const block>(block_result.header(),
        std::move(txs));
    handler(error::success, message, height);
}

void block_chain::fetch_block_header_txs_size(const hash_digest& hash,
    block_header_txs_size_fetch_handler handler) const
{

    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0, std::make_shared<hash_list>(hash_list()),0);
        return;
    }

    const auto block_result = database_.blocks().get(hash);

    if (!block_result)
    {
        handler(error::not_found, nullptr, 0, std::make_shared<hash_list>(hash_list()),0);
        return;
    }

    const auto height = block_result.height();
    const auto message = std::make_shared<const header>(block_result.header());
    const auto tx_hashes = std::make_shared<hash_list>(block_result.transaction_hashes());
    //TODO encapsulate header and tx_list
    handler(error::success, message, height, tx_hashes, block_result.serialized_size());
}

void block_chain::fetch_block_hash_timestamp(size_t height, block_hash_time_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, null_hash, 0, 0);
        return;
    }

    const auto block_result = database_.blocks().get(height);

    if (!block_result)
    {
        handler(error::not_found, null_hash, 0, 0);
        return;
    }

    handler(error::success, block_result.hash(), block_result.timestamp(), height);

}


void block_chain::fetch_block_header(size_t height,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(height);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<header>(result.header());
    handler(error::success, message, result.height());
}

void block_chain::fetch_block_header(const hash_digest& hash,
    block_header_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto message = std::make_shared<header>(result.header());
    handler(error::success, message, result.height());
}

// void block_chain::fetch_merkle_block(size_t height, transaction_hashes_fetch_handler handler) const
void block_chain::fetch_merkle_block(size_t height, merkle_block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(height);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        result.transaction_count(), result.transaction_hashes(), data_chunk{});
    handler(error::success, merkle, result.height());
}

void block_chain::fetch_merkle_block(const hash_digest& hash,
    merkle_block_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0);
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, nullptr, 0);
        return;
    }

    const auto merkle = std::make_shared<merkle_block>(result.header(),
        result.transaction_count(), result.transaction_hashes(), data_chunk{});
    handler(error::success, merkle, result.height());
}

void block_chain::fetch_compact_block(size_t height,
    compact_block_fetch_handler handler) const
{
    // TODO: implement compact blocks.
    handler(error::not_implemented, {}, 0);
}

void block_chain::fetch_compact_block(const hash_digest& hash,
    compact_block_fetch_handler handler) const
{
    // TODO: implement compact blocks.
    handler(error::not_implemented, {}, 0);
}

void block_chain::fetch_block_height(const hash_digest& hash,
    block_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    const auto result = database_.blocks().get(hash);

    if (!result)
    {
        handler(error::not_found, 0);
        return;
    }

    handler(error::success, result.height());
}

void block_chain::fetch_last_height(last_height_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    size_t last_height;

    if (!database_.blocks().top(last_height))
    {
        handler(error::not_found, 0);
        return;
    }

    handler(error::success, last_height);
}

void block_chain::fetch_transaction(const hash_digest& hash,
    bool require_confirmed, transaction_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr, 0, 0);
        return;
    }
//TODO: (bitprim) dissabled this tx cache because we don't want special treatment for the last txn, it affects the explorer rpc methods
//    // Try the cached block first if confirmation is not required.
//    if (!require_confirmed)
//    {
//        const auto cached = last_transaction_.load();
//
//        if (cached && cached->validation.state && cached->hash() == hash)
//        {
//            ////LOG_INFO(LOG_BLOCKCHAIN) << "TX CACHE HIT";
//
//            // Simulate the position and height overloading of the database.
//            handler(error::success, cached, transaction_database::unconfirmed,
//                cached->validation.state->height());
//            return;
//        }
//    }
  
    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
    {
        handler(error::not_found, nullptr, 0, 0);
        return;
    }

    const auto tx = std::make_shared<const transaction>(result.transaction());
    handler(error::success, tx, result.position(), result.height());
}


hash_digest generate_merkle_root(std::vector<chain::transaction> transactions) {
    if (transactions.empty())
        return null_hash;

    hash_list merkle, update;

    auto hasher = [&merkle](const transaction& tx)
    {
        merkle.push_back(tx.hash());
    };

    // Hash ordering matters, don't use std::transform here.
    std::for_each(transactions.begin(), transactions.end(), hasher);

    // Initial capacity is half of the original list (clear doesn't reset).
    update.reserve((merkle.size() + 1) / 2);

    while (merkle.size() > 1)
    {
        // If number of hashes is odd, duplicate last hash in the list.
        if (merkle.size() % 2 != 0)
            merkle.push_back(merkle.back());

        for (auto it = merkle.begin(); it != merkle.end(); it += 2)
            update.push_back(bitcoin_hash(build_chunk({ it[0], it[1] })));

        std::swap(merkle, update);
        update.clear();
    }

    // There is now only one item in the list.
    return merkle.front();
}

std::pair<bool, uint64_t> block_chain::total_input_value(libbitcoin::chain::transaction const& tx) const{

    uint64_t total = 0;

    for (auto const& input : tx.inputs()) {
        libbitcoin::chain::output out_output;
        size_t out_height;
        bool out_coinbase;
        uint32_t out_median; //TODO check if theres something to do with this
        if (!get_output(out_output, out_height, out_median, out_coinbase, input.previous_output(), libbitcoin::max_size_t, false)){
//            std::cout << "Output not found. Hash = " << libbitcoin::encode_hash(tx.hash())
//                      << ".\nOutput hash = " << encode_hash(input.previous_output().hash())
//                      << ".\nOutput index = " << input.previous_output().index() << "\n";
            return std::make_pair(false, 0);
        }
        const bool missing = !out_output.is_valid();
        total = ceiling_add(total, missing ? 0 : out_output.value());
    }

    return std::make_pair(true, total);
}

std::pair<bool, uint64_t> block_chain::fees(libbitcoin::chain::transaction const& tx) const {
    auto input_value = total_input_value(tx);
    if (input_value.first){
        return std::make_pair(true, floor_subtract(input_value.second, tx.total_output_value()));
    }
    return std::make_pair(false, 0);
}

bool block_chain::is_double_spent(chain::transaction const& tx, bool bip16_active) const {

    for (auto const& input : tx.inputs()) {
        auto const& outpoint = input.previous_output();
        auto& prevout = outpoint.validation;
        prevout.cache = chain::output{};

        // If the input is a coinbase there is no prevout to populate.
        if (outpoint.is_null()) return true;

        size_t output_height;
        bool output_coinbase;
        uint32_t out_median; //TODO check if theres something to do with this
        auto res_output = get_output(prevout.cache, output_height, out_median, output_coinbase, outpoint, max_size_t, true);

        if (! res_output) return true;
        if ( output_height == 0)  return true;

        const auto spend_height = prevout.cache.validation.spender_height;
        if (spend_height != chain::output::validation::not_spent) return true;

    }

    return false;
}

bool block_chain::validate_tx(chain::transaction const& tx, const size_t top) const {

    auto const now = std::chrono::high_resolution_clock::now();
    auto time = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
    if (!tx.is_final(top+1, time)){
        return false;
    }

    if (is_double_spent(tx, true)) {
        return false;
    }

    if(!tx.cached_is_standard())
        return false;

    return true;
}

void append_spend(chain::transaction const& tx, spent_container & result) {
    for (auto const& input : tx.inputs()) {
        auto const& output_point = input.previous_output();
        result.emplace(output_point.hash(), output_point.index());
    }
}

bool is_double_spend_mempool(chain::transaction const& tx, spent_container const& result) {
    for (auto const& input : tx.inputs()) {
        auto const& output_point = input.previous_output();
        auto res = result.find(std::make_pair(output_point.hash(), output_point.index()));
        return res != result.end();
    }
    return false;
}

std::vector<block_chain::tx_mempool> block_chain::fetch_mempool_all(size_t max_bytes) const {
    if (stopped()) {
        return std::vector<block_chain::tx_mempool>();
    }

    size_t height;
    if (!database_.blocks().top(height)) {
        return std::vector<block_chain::tx_mempool>();
    }

    std::vector<tx_mempool> mempool;
    spent_container spent;
    //TODO: move to constants or remove the transactions limit (35000)
    mempool.reserve(35000);
    database_.transactions_unconfirmed().for_each([&](chain::transaction const &tx) {
        if (mempool.size() > 35000 - 1) {
            return false;
        }
        auto res_validate = validate_tx(tx, height);
        auto res_ds = is_double_spend_mempool(tx, spent);
        if (res_validate && !res_ds) {
            append_spend(tx, spent);
            std::string dependencies = ""; //TODO: see what to do with the final algorithm
            size_t tx_weight = tx.to_data(true).size();
            mempool.emplace_back(tx, tx.cached_fees(), tx.cached_sigops(), dependencies, tx_weight, true);
        }
        return true;
    });

    auto const fee_per_weight = [](tx_mempool const &a, tx_mempool const &b) {
        auto const fpw_a = double(std::get<1>(a)) / std::get<4>(a);
        auto const fpw_b = double(std::get<1>(b)) / std::get<4>(b);
        return fpw_a < fpw_b;
    };

    std::sort(mempool.begin(), mempool.end(), fee_per_weight);
    // ------------------------------------------------------------------------------------
    std::vector<tx_mempool> mempool_final;

    bool finished = false;
    size_t current_size = 0;
    size_t current_sigops = 0;
    size_t iteration = 1;

    auto end = mempool.end();
    while (!finished) {
        auto txns = create_a_pack_of_txns(mempool);
        if (iteration == 1){
            //first block - continue
            current_size = std::get<1>(txns);
            current_sigops = std::get<0>(txns);
            std::move(std::get<2>(txns).begin(), std::get<2>(txns).end(), std::back_inserter(mempool_final));
            ++iteration;
        } else {
            size_t temp_size = current_size + std::get<1>(txns);
            if (temp_size <= max_bytes){
                // Correct size using the max_bytes value
                if (temp_size >= ((iteration - 1) * 950 * 1024) && temp_size <= (iteration * 950 * 1024)){
                    //Correct size - check sigops
                    size_t temp_sigops = current_sigops + std::get<0>(txns);
                    if (temp_sigops <= iteration * (20000 - 100)) {
                        //Correct sigops - continue
                        current_size += std::get<1>(txns);
                        current_sigops += std::get<0>(txns);
                        std::move(std::get<2>(txns).begin(), std::get<2>(txns).end(), std::back_inserter(mempool_final));
                        ++iteration;
                    }else {
                        // Too much sigops
                        finished = true;
                    }
                } else {
                    // Not big enough
                    finished = true;
                }
            }else{
                // The size will be bigger than the requested
                finished = true;
            }
        }
    }

    return mempool_final;
}

using tx_mempool = std::tuple<chain::transaction, uint64_t, uint64_t, std::string, size_t, bool>;
std::tuple<size_t,size_t,std::vector<tx_mempool>> block_chain::create_a_pack_of_txns(std::vector<tx_mempool>& mempool) const {
    size_t max_sigops = 20000 - 100;
    size_t max_bytes = 950 * 1024;

    size_t sigops_return = 0;
    size_t bytes_return = 0;
    std::vector<tx_mempool> mempool_return;

    size_t i = 0;

    while (i < mempool.size() && bytes_return < max_bytes && sigops_return < max_sigops) {
        auto &tx = mempool[i];
        if(std::get<5>(tx))
        {
            auto tx_size = std::get<4>(tx);
            auto tx_sigops = std::get<2>(tx);
            if (max_bytes >= bytes_return + tx_size && max_sigops >= sigops_return + tx_sigops) {
                mempool_return.push_back(tx);
                std::get<5>(tx) = false;
                bytes_return += tx_size;
                sigops_return += tx_sigops;
            }
        }
        ++i;
    }

    return std::make_tuple(sigops_return, bytes_return, mempool_return);
}

std::vector<std::tuple<std::string, std::string, size_t, std::string, uint64_t, std::string, std::string>> block_chain::fetch_mempool_addrs(std::vector<std::string> const& payment_addresses, bool use_testnet_rules) const {
/*          "    \"address\"  (string) The base58check encoded address\n"
            "    \"txid\"  (string) The related txid\n"
            "    \"index\"  (number) The related input or output index\n"
            "    \"satoshis\"  (number) The difference of satoshis\n"
            "    \"timestamp\"  (number) The time the transaction entered the mempool (seconds)\n"
            "    \"prevtxid\"  (string) The previous txid (if spending)\n"
            "    \"prevout\"  (string) The previous transaction output index (if spending)\n"
*/
    uint8_t encoding_p2kh;
    uint8_t encoding_p2sh;
    if (use_testnet_rules){
        encoding_p2kh = libbitcoin::wallet::payment_address::testnet_p2kh;
        encoding_p2sh = libbitcoin::wallet::payment_address::testnet_p2sh;
    } else {
        encoding_p2kh = libbitcoin::wallet::payment_address::mainnet_p2kh;
        encoding_p2sh = libbitcoin::wallet::payment_address::mainnet_p2sh;
    }
    std::vector<std::tuple<std::string, std::string, size_t, std::string, uint64_t, std::string, std::string>> ret;
    std::unordered_set<libbitcoin::wallet::payment_address> addrs;
    for (const auto & payment_address : payment_addresses) {
        libbitcoin::wallet::payment_address address(payment_address);
        if (address){
            addrs.insert(address);
        }
    }

    database_.transactions_unconfirmed().for_each_result([&](libbitcoin::database::transaction_unconfirmed_result const &tx_res) {
        auto tx = tx_res.transaction();
        tx.recompute_hash();
        size_t i = 0;
        for (auto const& output : tx.outputs()) {
            const auto tx_address = libbitcoin::wallet::payment_address::extract(output.script(), encoding_p2kh, encoding_p2sh);
            ++i;
            if (tx_address && addrs.find(tx_address) != addrs.end()) {
                ret.push_back(std::make_tuple(tx_address.encoded(), libbitcoin::encode_hash(tx.hash()), i, std::to_string(output.value()), tx_res.arrival_time(), "", ""));
            }
        }
        i = 0;
        for (auto const& input : tx.inputs()) {
            const auto tx_address = libbitcoin::wallet::payment_address::extract(input.script(), encoding_p2kh, encoding_p2sh);
            if (tx_address && addrs.find(tx_address) != addrs.end()) {
                boost::latch latch(2);
                fetch_transaction(input.previous_output().hash(), false,
                                  [&](const libbitcoin::code &ec,
                                      libbitcoin::transaction_const_ptr tx_ptr, size_t index,
                                      size_t height) {
                                      if (ec == libbitcoin::error::success) {
                                          ret.push_back(std::make_tuple(tx_address.encoded(),
                                                                            libbitcoin::encode_hash(tx.hash()),
                                                                            i,
                                                                            "-"+std::to_string(tx_ptr->outputs()[input.previous_output().index()].value()),
                                                                            tx_res.arrival_time(),
                                                                            libbitcoin::encode_hash(input.previous_output().hash()),
                                                                            std::to_string(input.previous_output().index())));
                                      }
                                      latch.count_down();
                                  });
                latch.count_down_and_wait();
            }
            ++i;
        }
        return true;
    });

    return ret;
}

// This is same as fetch_transaction but skips deserializing the tx payload.
void block_chain::fetch_transaction_position(const hash_digest& hash,
    bool require_confirmed, transaction_index_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, 0, 0);
        return;
    }

    const auto result = database_.transactions().get(hash, max_size_t,
        require_confirmed);

    if (!result)
    {
        handler(error::not_found, 0, 0);
        return;
    }

    handler(error::success, result.position(), result.height());
}

// This may execute over 500 queries.
void block_chain::fetch_locator_block_hashes(get_blocks_const_ptr locator,
    const hash_digest& threshold, size_t limit,
    inventory_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.

    // Find the start block height.
    // If no start block is on our chain we start with block 0.
    size_t start = 0;
    for (const auto& hash: locator->start_hashes())
    {
        const auto result = database_.blocks().get(hash);
        if (result)
        {
            start = result.height();
            break;
        }
    }

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 500.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold);

        // Otherwise limit the begin height to the threshold block height.
        // If begin exceeds end floor_subtract will handle below.
        if (result)
            begin = std::max(result.height(), begin);
    }

    auto hashes = std::make_shared<inventory>();
    hashes->inventories().reserve(floor_subtract(end, begin));

    // Build the hash list until we hit end or the blockchain top.
    for (auto height = begin; height < end; ++height)
    {
        const auto result = database_.blocks().get(height);

        // If not found then we are at our top.
        if (!result)
        {
            hashes->inventories().shrink_to_fit();
            break;
        }

        static const auto id = inventory::type_id::block;
        hashes->inventories().emplace_back(id, result.header().hash());
    }

    handler(error::success, std::move(hashes));
}

// This may execute over 2000 queries.
void block_chain::fetch_locator_block_headers(get_headers_const_ptr locator,
    const hash_digest& threshold, size_t limit,
    locator_block_headers_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    // This is based on the idea that looking up by block hash to get heights
    // will be much faster than hashing each retrieved block to test for stop.

    // Find the start block height.
    // If no start block is on our chain we start with block 0.
    size_t start = 0;
    for (const auto& hash: locator->start_hashes())
    {
        const auto result = database_.blocks().get(hash);
        if (result)
        {
            start = result.height();
            break;
        }
    }

    // The begin block requested is always one after the start block.
    auto begin = safe_add(start, size_t(1));

    // The maximum number of headers returned is 2000.
    auto end = safe_add(begin, limit);

    // Find the upper threshold block height (peer-specified).
    if (locator->stop_hash() != null_hash)
    {
        // If the stop block is not on chain we treat it as a null stop.
        const auto result = database_.blocks().get(locator->stop_hash());

        // Otherwise limit the end height to the stop block height.
        // If end precedes begin floor_subtract will handle below.
        if (result)
            end = std::min(result.height(), end);
    }

    // Find the lower threshold block height (self-specified).
    if (threshold != null_hash)
    {
        // If the threshold is not on chain we ignore it.
        const auto result = database_.blocks().get(threshold);

        // Otherwise limit the begin height to the threshold block height.
        // If begin exceeds end floor_subtract will handle below.
        if (result)
            begin = std::max(result.height(), begin);
    }

    auto message = std::make_shared<headers>();
    message->elements().reserve(floor_subtract(end, begin));

    // Build the hash list until we hit end or the blockchain top.
    for (auto height = begin; height < end; ++height)
    {
        const auto result = database_.blocks().get(height);

        // If not found then we are at our top.
        if (!result)
        {
            message->elements().shrink_to_fit();
            break;
        }

        message->elements().push_back(result.header());
    }

    handler(error::success, std::move(message));
}

// This may generally execute 29+ queries.
void block_chain::fetch_block_locator(const block::indexes& heights,
    block_locator_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, nullptr);
        return;
    }

    // Caller can cast get_headers down to get_blocks.
    auto message = std::make_shared<get_headers>();
    auto& hashes = message->start_hashes();
    hashes.reserve(heights.size());

    for (const auto height: heights)
    {
        const auto result = database_.blocks().get(height);

        if (!result)
        {
            handler(error::not_found, nullptr);
            break;
        }

        hashes.push_back(result.header().hash());
    }

    handler(error::success, message);
}

// Server Queries.
//-----------------------------------------------------------------------------

void block_chain::fetch_spend(const chain::output_point& outpoint,
    spend_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    auto point = database_.spends().get(outpoint);

    if (point.hash() == null_hash)
    {
        handler(error::not_found, {});
        return;
    }

    handler(error::success, std::move(point));
}

void block_chain::fetch_history(const short_hash& address_hash, size_t limit,
    size_t from_height, history_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::success, database_.history().get(address_hash, limit,
        from_height));
}

void block_chain::fetch_txns(const short_hash& address_hash, size_t limit,
                                size_t from_height, txns_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::success, database_.history().get_txns(address_hash, limit,
                                                    from_height));
}

void block_chain::fetch_stealth(const binary& filter, size_t from_height,
    stealth_fetch_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped, {});
        return;
    }

    handler(error::success, database_.stealth().scan(filter, from_height));
}

// Transaction Pool.
//-----------------------------------------------------------------------------

// Same as fetch_mempool but also optimized for maximum possible block fee as
// limited by total bytes and signature operations.
void block_chain::fetch_template(merkle_block_fetch_handler handler) const
{
    transaction_organizer_.fetch_template(handler);
}

// Fetch a set of currently-valid unconfirmed txs in dependency order.
// All txs satisfy the fee minimum and are valid at the next chain state.
// The set of blocks is limited in count to size. The set may have internal
// dependencies but all inputs must be satisfied at the current height.
void block_chain::fetch_mempool(size_t count_limit, uint64_t minimum_fee,
    inventory_fetch_handler handler) const
{
    transaction_organizer_.fetch_mempool(count_limit, handler);
}

// Filters.
//-----------------------------------------------------------------------------

// This may execute up to 500 queries.
// This filters against the block pool and then the block chain.
void block_chain::filter_blocks(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    // Filter through block pool first.
    block_organizer_.filter(message);
    auto& inventories = message->inventories();
    const auto& blocks = database_.blocks();

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_block_type() && blocks.get(it->hash()))
            it = inventories.erase(it);
        else
            ++it;
    }

    handler(error::success);
}

// This filters against all transactions (confirmed and unconfirmed).
void block_chain::filter_transactions(get_data_ptr message,
    result_handler handler) const
{
    if (stopped())
    {
        handler(error::service_stopped);
        return;
    }

    auto& inventories = message->inventories();
    const auto& transactions = database_.transactions();

    for (auto it = inventories.begin(); it != inventories.end();)
    {
        if (it->is_transaction_type() &&
            get_is_unspent_transaction(it->hash(), max_size_t, false))
            it = inventories.erase(it);
        else
            ++it;
    }

    handler(error::success);
}

// Subscribers.
//-----------------------------------------------------------------------------

void block_chain::subscribe_blockchain(reorganize_handler&& handler)
{
    // Pass this through to the organizer, which issues the notifications.
    block_organizer_.subscribe(std::move(handler));
}

void block_chain::subscribe_transaction(transaction_handler&& handler)
{
    // Pass this through to the tx organizer, which issues the notifications.
    transaction_organizer_.subscribe(std::move(handler));
}

void block_chain::unsubscribe()
{
    block_organizer_.unsubscribe();
    transaction_organizer_.unsubscribe();
}

// Organizers.
//-----------------------------------------------------------------------------

void block_chain::organize(block_const_ptr block, result_handler handler)
{
    // This cannot call organize or stop (lock safe).
    block_organizer_.organize(block, handler);
}

void block_chain::organize(transaction_const_ptr tx, result_handler handler)
{
    // This cannot call organize or stop (lock safe).
    transaction_organizer_.organize(tx, handler);
}

// Properties (thread safe).
// ----------------------------------------------------------------------------

bool block_chain::is_stale() const
{
    // If there is no limit set the chain is never considered stale.
    if (notify_limit_seconds_ == 0)
        return false;

    const auto top = last_block_.load();
    // BITPRIM: get the last block if there is no cache
    uint32_t last_timestamp = 0;
    if (!top)
    {
        size_t last_height;
        if (get_last_height(last_height))
        {
            chain::header last_header;
            if (get_header(last_header, last_height))
            {
                last_timestamp = last_header.timestamp();
            }
        }
    }
    const auto timestamp = top ? top->header().timestamp() : last_timestamp;
    return timestamp < floor_subtract(zulu_time(), notify_limit_seconds_);
}

const settings& block_chain::chain_settings() const
{
    return settings_;
}

// protected
bool block_chain::stopped() const
{
    return stopped_;
}

} // namespace blockchain
} // namespace libbitcoin
