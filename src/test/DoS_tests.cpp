// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2015 The ziftrCOIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for denial-of-service detection/prevention code
//

//
// Useful for generating the test cases
//
// int64_t nTimeStart = GetTime();
// std::cout << "(" << nTimeStart << ", " << bnProofOfWorkLimit.GetCompact() << ")\n";

// for (int i = 0; i < 10; i++)
// {
//     nTimeStart += 45*4;
//     bnProofOfWorkLimit = 45 * bnProofOfWorkLimit / 60;
//     bnProofOfWorkLimit.SetCompact(bnProofOfWorkLimit.GetCompact());
//     std::cout << "(" << nTimeStart << ", " << bnProofOfWorkLimit.GetCompact() << ")\n";
// }

// for (int i = 0; i < 4; i++)
// {
//     nTimeStart += 90*4;
//     bnProofOfWorkLimit = 360 * bnProofOfWorkLimit;
//     bnProofOfWorkLimit = bnProofOfWorkLimit / 240;
//     bnProofOfWorkLimit.SetCompact(bnProofOfWorkLimit.GetCompact());
//     std::cout << "(" << nTimeStart << ", " << bnProofOfWorkLimit.GetCompact() << ")\n";
// }

// throw std::exception();



#include "bignum.h"
#include "keystore.h"
#include "main.h"
#include "net.h"
#include "script.h"
#include "serialize.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/foreach.hpp>
#include <boost/test/unit_test.hpp>

// Tests this internal-to-main.cpp method:
extern bool AddOrphanTx(const CTransaction& tx, NodeId peer);
extern void EraseOrphansFor(NodeId peer);
extern unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
extern std::map<uint256, CTransaction> mapOrphanTransactions;
extern std::map<uint256, std::set<uint256> > mapOrphanTransactionsByPrev;

CService ip(uint32_t i)
{
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

BOOST_AUTO_TEST_SUITE(DoS_tests)

BOOST_AUTO_TEST_CASE(DoS_banning)
{
    CNode::ClearBanned();
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.nVersion = 1;
    Misbehaving(dummyNode1.GetId(), 100); // Should get banned
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(CNode::IsBanned(addr1));
    BOOST_CHECK(!CNode::IsBanned(ip(0xa0b0c001|0x0000ff00))); // Different IP, not banned

    CAddress addr2(ip(0xa0b0c002));
    CNode dummyNode2(INVALID_SOCKET, addr2, "", true);
    dummyNode2.nVersion = 1;
    Misbehaving(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, false);
    BOOST_CHECK(!CNode::IsBanned(addr2)); // 2 not banned yet...
    BOOST_CHECK(CNode::IsBanned(addr1));  // ... but 1 still should be
    Misbehaving(dummyNode2.GetId(), 50);
    SendMessages(&dummyNode2, false);
    BOOST_CHECK(CNode::IsBanned(addr2));
}

BOOST_AUTO_TEST_CASE(DoS_banscore)
{
    CNode::ClearBanned();
    mapArgs["-banscore"] = "111"; // because 11 is my favorite number
    CAddress addr1(ip(0xa0b0c001));
    CNode dummyNode1(INVALID_SOCKET, addr1, "", true);
    dummyNode1.nVersion = 1;
    Misbehaving(dummyNode1.GetId(), 100);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(!CNode::IsBanned(addr1));
    Misbehaving(dummyNode1.GetId(), 10);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(!CNode::IsBanned(addr1));
    Misbehaving(dummyNode1.GetId(), 1);
    SendMessages(&dummyNode1, false);
    BOOST_CHECK(CNode::IsBanned(addr1));
    mapArgs.erase("-banscore");
}

BOOST_AUTO_TEST_CASE(DoS_bantime)
{
    CNode::ClearBanned();
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime); // Overrides future calls to GetTime()

    CAddress addr(ip(0xa0b0c001));
    CNode dummyNode(INVALID_SOCKET, addr, "", true);
    dummyNode.nVersion = 1;

    Misbehaving(dummyNode.GetId(), 100);
    SendMessages(&dummyNode, false);
    BOOST_CHECK(CNode::IsBanned(addr));

    SetMockTime(nStartTime+60*60);
    BOOST_CHECK(CNode::IsBanned(addr));

    SetMockTime(nStartTime+60*60*24+1);
    BOOST_CHECK(!CNode::IsBanned(addr));
}

static bool CheckNBits(unsigned int nbits1, int64_t time1, unsigned int nbits2, int64_t time2)\
{
    if (time1 > time2)
        return CheckNBits(nbits2, time2, nbits1, time1);
    int64_t deltaTime = time2-time1;

    CBigNum required;
    required.SetCompact(ComputeMinWork(nbits1, deltaTime));
    CBigNum have;
    have.SetCompact(nbits2);

    if (time1 == 1425108718 && nbits1 == 490078207 && time2 == 1425109078 && nbits2 == 491847679)
    {
        std::cout << "have: " << have.GetCompact() << std::endl;
        std::cout << "req : " << required.GetCompact() << std::endl;
    }

    return (have <= required);
}

BOOST_AUTO_TEST_CASE(DoS_checknbits)
{
    using namespace boost::assign; // for 'map_list_of()'

    // Timestamps,nBits from the block chain.
    // These are the block-chain checkpoint blocks
    typedef std::map<int64_t, unsigned int> BlockData;
    BlockData chainData =
        map_list_of (1425111048, 494927871)
                    (1425111228, 492830719)
                    (1425111408, 491257855)
                    (1425111588, 490078207)
                    (1425111768, 489193471)
                    (1425111948, 488529919)
                    (1425112128, 488032255)
                    (1425112308, 487659007)
                    (1425112488, 487379071)
                    (1425112668, 487169119)
                    (1425112848, 487011655)
                    (1425113208, 487247850)
                    (1425113568, 487602143)
                    (1425113928, 488133582)
                    (1425114288, 488930741);

    // Make sure CheckNBits considers every combination of block-chain-lock-in-points
    // "sane":
    BOOST_FOREACH(const BlockData::value_type& i, chainData)
    {
        BOOST_FOREACH(const BlockData::value_type& j, chainData)
        {
            BOOST_CHECK(CheckNBits(i.second, i.first, j.second, j.first));
        }
    }

    // Test a couple of insane combinations:
    BlockData::value_type firstcheck = *(chainData.begin());
    BlockData::value_type lastcheck = *(chainData.rbegin());

    // First checkpoint difficulty at or a while after the last checkpoint time should fail when
    // compared to last checkpoint0
    BOOST_CHECK(!CheckNBits(firstcheck.second, lastcheck.first+5*60, lastcheck.second, lastcheck.first));
    BOOST_CHECK(!CheckNBits(firstcheck.second, lastcheck.first+10*60, lastcheck.second, lastcheck.first));

    // ... but OK if enough time passed for difficulty to adjust downward:
    BOOST_CHECK(CheckNBits(firstcheck.second, lastcheck.first+20*60, lastcheck.second, lastcheck.first));
}

CTransaction RandomOrphan()
{
    std::map<uint256, CTransaction>::iterator it;
    it = mapOrphanTransactions.lower_bound(GetRandHash());
    if (it == mapOrphanTransactions.end())
        it = mapOrphanTransactions.begin();
    return it->second;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans)
{
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++)
    {
        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = GetRandHash();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());

        AddOrphanTx(tx, i);
    }

    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = txPrev.GetHash();
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        SignSignature(keystore, txPrev, tx, 0);

        AddOrphanTx(tx, i);
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++)
    {
        CTransaction txPrev = RandomOrphan();

        CTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
        tx.vin.resize(500);
        for (unsigned int j = 0; j < tx.vin.size(); j++)
        {
            tx.vin[j].prevout.n = j;
            tx.vin[j].prevout.hash = txPrev.GetHash();
        }
        SignSignature(keystore, txPrev, tx, 0);
        // Re-use same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++)
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;

        BOOST_CHECK(!AddOrphanTx(tx, i));
    }

    // Test EraseOrphansFor:
    for (NodeId i = 0; i < 3; i++)
    {
        size_t sizeBefore = mapOrphanTransactions.size();
        EraseOrphansFor(i);
        BOOST_CHECK(mapOrphanTransactions.size() < sizeBefore);
    }

    // Test LimitOrphanTxSize() function:
    LimitOrphanTxSize(40);
    BOOST_CHECK(mapOrphanTransactions.size() <= 40);
    LimitOrphanTxSize(10);
    BOOST_CHECK(mapOrphanTransactions.size() <= 10);
    LimitOrphanTxSize(0);
    BOOST_CHECK(mapOrphanTransactions.empty());
    BOOST_CHECK(mapOrphanTransactionsByPrev.empty());
}

BOOST_AUTO_TEST_CASE(DoS_checkSig)
{
    // Test signature caching code (see key.cpp Verify() methods)

    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;

    // 100 orphan transactions:
    static const int NPREV=100;
    CTransaction orphans[NPREV];
    for (int i = 0; i < NPREV; i++)
    {
        CTransaction& tx = orphans[i];
        tx.vin.resize(1);
        tx.vin[0].prevout.n = 0;
        tx.vin[0].prevout.hash = GetRandHash();
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1*CENT;
        tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());

        AddOrphanTx(tx, 0);
    }

    // Create a transaction that depends on orphans:
    CTransaction tx;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1*CENT;
    tx.vout[0].scriptPubKey.SetDestination(key.GetPubKey().GetID());
    tx.vin.resize(NPREV);
    for (unsigned int j = 0; j < tx.vin.size(); j++)
    {
        tx.vin[j].prevout.n = 0;
        tx.vin[j].prevout.hash = orphans[j].GetHash();
    }
    // Creating signatures primes the cache:
    boost::posix_time::ptime mst1 = boost::posix_time::microsec_clock::local_time();
    for (unsigned int j = 0; j < tx.vin.size(); j++)
        BOOST_CHECK(SignSignature(keystore, orphans[j], tx, j));
    boost::posix_time::ptime mst2 = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::time_duration msdiff = mst2 - mst1;
    long nOneValidate = msdiff.total_milliseconds();
    if (fDebug) printf("DoS_Checksig sign: %ld\n", nOneValidate);

    // ... now validating repeatedly should be quick:
    // 2.8GHz machine, -g build: Sign takes ~760ms,
    // uncached Verify takes ~250ms, cached Verify takes ~50ms
    // (for 100 single-signature inputs)
    mst1 = boost::posix_time::microsec_clock::local_time();
    for (unsigned int i = 0; i < 5; i++)
        for (unsigned int j = 0; j < tx.vin.size(); j++)
            BOOST_CHECK(VerifySignature(CCoins(orphans[j], MEMPOOL_HEIGHT), tx, j, flags, SIGHASH_ALL));
    mst2 = boost::posix_time::microsec_clock::local_time();
    msdiff = mst2 - mst1;
    long nManyValidate = msdiff.total_milliseconds();
    if (fDebug) printf("DoS_Checksig five: %ld\n", nManyValidate);

    BOOST_CHECK_MESSAGE(nManyValidate < nOneValidate, "Signature cache timing failed");

    // Empty a signature, validation should fail:
    CScript save = tx.vin[0].scriptSig;
    tx.vin[0].scriptSig = CScript();
    BOOST_CHECK(!VerifySignature(CCoins(orphans[0], MEMPOOL_HEIGHT), tx, 0, flags, SIGHASH_ALL));
    tx.vin[0].scriptSig = save;

    // Swap signatures, validation should fail:
    std::swap(tx.vin[0].scriptSig, tx.vin[1].scriptSig);
    BOOST_CHECK(!VerifySignature(CCoins(orphans[0], MEMPOOL_HEIGHT), tx, 0, flags, SIGHASH_ALL));
    BOOST_CHECK(!VerifySignature(CCoins(orphans[1], MEMPOOL_HEIGHT), tx, 1, flags, SIGHASH_ALL));
    std::swap(tx.vin[0].scriptSig, tx.vin[1].scriptSig);

    // Exercise -maxsigcachesize code:
    mapArgs["-maxsigcachesize"] = "10";
    // Generate a new, different signature for vin[0] to trigger cache clear:
    CScript oldSig = tx.vin[0].scriptSig;
    BOOST_CHECK(SignSignature(keystore, orphans[0], tx, 0));
    BOOST_CHECK(tx.vin[0].scriptSig != oldSig);
    for (unsigned int j = 0; j < tx.vin.size(); j++)
        BOOST_CHECK(VerifySignature(CCoins(orphans[j], MEMPOOL_HEIGHT), tx, j, flags, SIGHASH_ALL));
    mapArgs.erase("-maxsigcachesize");

    LimitOrphanTxSize(0);
}

BOOST_AUTO_TEST_SUITE_END()
