// Copyright (c) 2012-2013 The Bitcoin Core developers
// Copyright (c) 2015 The ziftrCOIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"

#include "base58.h"

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

Array createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL)
{
    Array result;
    result.push_back(nRequired);
    Array addresses;
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

Value CallRPC(string args)
{
    vector<string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    Array params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        Value result = (*method)(params, false);
        return result;
    }
    catch (Object& objError)
    {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}


BOOST_AUTO_TEST_SUITE(rpc_tests)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
    Value r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction not_hex"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction null null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] []"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction {} {}"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {}"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction DEADBEEF"), runtime_error);
    string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    BOOST_CHECK_NO_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locktime").get_int(), 0);
    BOOST_CHECK_THROW(r = CallRPC(string("decoderawtransaction ")+rawtx+" extra"), runtime_error);

    BOOST_CHECK_THROW(CallRPC("signrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction ff00"), runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null NONE|ANYONECANPAY"));
    BOOST_CHECK_NO_THROW(CallRPC(string("signrawtransaction ")+rawtx+" [] [] NONE|ANYONECANPAY"));
    BOOST_CHECK_THROW(CallRPC(string("signrawtransaction ")+rawtx+" null null badenum"), runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction null"), runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction DEADBEEF"), runtime_error);
    BOOST_CHECK_THROW(CallRPC(string("sendrawtransaction ")+rawtx+" extra"), runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
    CChainParams::Network network = Params().NetworkID();
    SelectParams(CChainParams::TESTNET);

    Value r;
    // input is a 1-of-2 multisig (so is output):
    string prevout =
      "[{\"txid\":\"0101010101010101010101010101010101010101010101010101010101010101\","
      "\"vout\":1,\"scriptPubKey\":\"a914f8c2065bcb60bf273313cfab35cc696d16ad347087\","
      "\"redeemScript\":\"512102a138abe0b0b0ffa932b9062837d5afd3cdd882256fb2ea1673ffca952e908a57210359d165637382227373829a6b0aca486e50d36ab65dfa005b51f0b7f21467fd1c52ae\"}]";
    r = CallRPC(string("createrawtransaction ")+prevout+" "+
      "{\"2NFvXzwUPLrTDPwdSDHVP6sozJRhZijRawD\":11}");
    string notsigned = r.get_str();
    string privkey1 = "\"cU1b9xaE1j3gsyT84M81N5ziK3n8EfVZ6CxB72pj9wRYftSe9nnA\"";
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"[]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == false);
    r = CallRPC(string("signrawtransaction ")+notsigned+" "+prevout+" "+"["+privkey1+"]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == true);

    SelectParams(network);
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(write_string(ValueFromAmount(0LL), false) == "0.00000");
    BOOST_CHECK(write_string(ValueFromAmount(1LL), false) == "0.00001");
    BOOST_CHECK(write_string(ValueFromAmount(22195LL), false) == "0.22195");
    BOOST_CHECK(write_string(ValueFromAmount(50000LL), false) == "0.50000");
    BOOST_CHECK(write_string(ValueFromAmount(98989LL), false) == "0.98989");
    BOOST_CHECK(write_string(ValueFromAmount(100000LL), false) == "1.00000");
    BOOST_CHECK(write_string(ValueFromAmount(999999999999990LL), false) == "9999999999.99990");
    BOOST_CHECK(write_string(ValueFromAmount(999999999999999LL), false) == "9999999999.99999");
}

static Value ValueFromString(const std::string &str)
{
    Value value;
    BOOST_CHECK(read_string(str, value));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK(AmountFromValue(ValueFromString("0.00001")) == 1LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.22195")) == 22195LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.5")) == 50000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.50000")) == 50000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("0.98989")) == 98989LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("1.00000")) == 100000LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("9999999999.9999")) == 999999999999990LL);
    BOOST_CHECK(AmountFromValue(ValueFromString("9999999999.99999")) == 999999999999999LL);
}

BOOST_AUTO_TEST_SUITE_END()
