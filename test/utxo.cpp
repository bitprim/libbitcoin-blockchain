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

#include <future>
#include <memory>
#include <string>
#include <bitcoin/blockchain.hpp>

using namespace bc;
using namespace bc::blockchain;
using namespace boost::system;
using namespace boost::filesystem;

#define MAINNET_BLOCK1 \
"010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000982" \
"051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649ffff00" \
"1d01e3629901010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d0104ffffffff0100f2052a0100000043410496b538e8" \
"53519c726a2c91e61ec11600ae1390813a627c66fb8be7947be63c52da7589379515d4e0a60" \
"4f8141781e62294721166bf621e73a82cbf2342c858eeac00000000"

#define MAINNET_BLOCK2 \
"010000004860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000d5f" \
"dcc541e25de1c7a5addedf24858b8bb665c9f36ef744ee42c316022c90f9bb0bc6649ffff00" \
"1d08d2bd6101010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d010bffffffff0100f2052a010000004341047211a824" \
"f55b505228e4c3d5194c1fcfaa15a456abdf37f9b9d97a4040afc073dee6c89064984f03385" \
"237d92167c13e236446b417ab79a0fcae412ae3316b77ac00000000"

#define MAINNET_BLOCK3 \
"01000000bddd99ccfda39da1b108ce1a5d70038d0a967bacb68b6b63065f626a0000000044f" \
"672226090d85db9a9f2fbfe5f0f9609b387af7be5b7fbb7a1767c831c9e995dbe6649ffff00" \
"1d05e0ed6d01010000000100000000000000000000000000000000000000000000000000000" \
"00000000000ffffffff0704ffff001d010effffffff0100f2052a0100000043410494b9d3e7" \
"6c5b1629ecf97fff95d7a4bbdac87cc26099ada28066c6ff1eb9191223cd897194a08d0c272" \
"6c5747f1db49e8cf90e75dc3e3550ae9b30086f3cd5aaac00000000"

#define TEST_SET_NAME \
    "p2p_tests"

#define TEST_NAME \
    std::string(boost::unit_test::framework::current_test_case().p_name)

#define START_BLOCKCHAIN(name, flush) \
    threadpool pool; \
    database::settings database_settings; \
    database_settings.flush_writes = flush; \
    database_settings.directory = TEST_NAME; \
    BOOST_REQUIRE(utxo_tests::create_database(database_settings)); \
    blockchain::settings blockchain_settings; \
    block_chain name(pool, blockchain_settings, database_settings); \
    BOOST_REQUIRE(name.start())

#define NEW_BLOCK(height) \
    std::make_shared<const message::block>(utxo_tests::read_block(MAINNET_BLOCK##height))


namespace utxo_tests {

static const uint64_t genesis_mainnet_work = 0x0000000100010001;

static void print_headers(const std::string& test)
{
    const auto header = "=========== " + test + " ==========";
    LOG_INFO(TEST_SET_NAME) << header;
}

bool create_database(database::settings& out_database)
{
    print_headers(out_database.directory.string());

    // Blockchain doesn't care about other indexes.
    out_database.index_start_height = max_uint32;

    // Table optimization parameters, reduced for speed and more collision.
    out_database.file_growth_rate = 42;
#ifdef BITPRIM_DB_LEGACY
    out_database.block_table_buckets = 42;
    out_database.transaction_table_buckets = 42;
#endif // BITPRIM_DB_LEGACY

#ifdef BITPRIM_DB_NEW
    out_database.db_max_size = 16106127360;
#endif

    error_code ec;
    remove_all(out_database.directory, ec);
    database::data_base database(out_database);
    return create_directories(out_database.directory, ec) 
            && database.create(chain::block::genesis_mainnet());
}

chain::block read_block(const std::string hex)
{
    data_chunk data;
    BOOST_REQUIRE(decode_base16(data, hex));
    chain::block result;
    BOOST_REQUIRE(result.from_data(data));
    return result;
}

} // namespace utxo_tests

BOOST_AUTO_TEST_SUITE(utxo_tests)

#ifdef BITPRIM_DB_NEW

BOOST_AUTO_TEST_CASE(utxo__get_utxo__not_found__false)
{
    START_BLOCKCHAIN(instance, false);

    chain::output output;
    size_t height;
    uint32_t median_time_past;
    bool coinbase;
    const chain::output_point outpoint{ null_hash, 42 };
    size_t branch_height = 0;
    BOOST_REQUIRE( ! instance.get_utxo(output, height, median_time_past, coinbase, outpoint, branch_height));
}

BOOST_AUTO_TEST_CASE(utxo__get_utxo__found__expected)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    chain::output output;
    size_t height;
    uint32_t median_time_past;
    bool coinbase;
    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
    const auto expected_value = initial_block_subsidy_satoshi();
    const auto expected_script = block2->transactions()[0].outputs()[0].script().to_string(0);
    BOOST_REQUIRE(instance.get_utxo(output, height, median_time_past, coinbase, outpoint, 12));
    BOOST_REQUIRE(coinbase);
    BOOST_REQUIRE_EQUAL(height, 2u);
    BOOST_REQUIRE_EQUAL(output.value(), expected_value);
    BOOST_REQUIRE_EQUAL(output.script().to_string(0), expected_script);
}

BOOST_AUTO_TEST_CASE(utxo__get_utxo__above_fork__false)
{
    START_BLOCKCHAIN(instance, false);

    const auto block1 = NEW_BLOCK(1);
    const auto block2 = NEW_BLOCK(2);
    BOOST_REQUIRE(instance.insert(block1, 1));
    BOOST_REQUIRE(instance.insert(block2, 2));

    chain::output output;
    size_t height;
    uint32_t median_time_past;
    bool coinbase;
    const chain::output_point outpoint{ block2->transactions()[0].hash(), 0 };
    BOOST_REQUIRE( ! instance.get_utxo(output, height, median_time_past, coinbase, outpoint, 1));
}

#endif // BITPRIM_DB_NEW

BOOST_AUTO_TEST_SUITE_END()
