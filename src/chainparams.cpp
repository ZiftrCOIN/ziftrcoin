// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "core.h"
#include "protocol.h"
#include "util.h"
#include "script.h"
#include "base58.h"

#include <assert.h>
#include <boost/assign/list_of.hpp>

using namespace boost::assign;

unsigned int pnSeed[] =
{
    0xcc5a0134
};

static const char* pszTimestamp = "Discus Fish: 24, Unknown: 22, GHash.IO: 22";

// == 0: Don't mine any
// == 1: Mine the main net genesis block
// == 2: Mine the test net genesis block
// == 3: Mine the reg test genesis block
static int whichGenesisMine = 0;

static void MineGenesisBlock(CBlock& genesis, CBigNum& bnProofOfWorkLimit, const string& pref) 
{
    uint256 proofOfWorkLimit = bnProofOfWorkLimit.getuint256();
    genesis.nNonce = 0;
    uint256 hashGenesisBlock;

    do {
        genesis.nNonce++;
        genesis.SetPoK(genesis.CalculatePoK());
        hashGenesisBlock = genesis.GetHash();
    } while (hashGenesisBlock > proofOfWorkLimit);
    
    printf("%s pok: %u\n", pref.c_str(), genesis.GetPoK());
    printf("%s nonce: %u\n", pref.c_str(), genesis.nNonce);
    printf("%s time: %llu\n", pref.c_str(), (long long)genesis.nTime);
    printf("%s hash: %s\n", pref.c_str(), genesis.GetHash().ToString().c_str());
    printf("%s merkleRoot: %s\n", pref.c_str(), genesis.hashMerkleRoot.ToString().c_str());
    // CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
    // ssBlock << genesis;
    // printf("%s block: \n%s\n", pref.c_str(), HexStr(ssBlock.begin(), ssBlock.end()).c_str());
}

//
// Main network
//
class CMainParams : public CChainParams {
public:
    CMainParams() {
        base58Prefixes[PUBKEY_ADDRESS] = list_of(80);  // P2PKH addresses start with 'Z'
        base58Prefixes[SCRIPT_ADDRESS] = list_of(5);   // P2SH  addresses start with '3'
        base58Prefixes[SECRET_KEY] =     list_of(208); // Priv keys prefixed with 80 + 128
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x88)(0xAD)(0xE4);

        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0x9e;
        pchMessageStart[1] = 0xee;
        pchMessageStart[2] = 0x83;
        pchMessageStart[3] = 0x2b;
        nDefaultPort = 10333;
        nRPCPort = 10332;
        vAlertPubKey = ParseHex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");//"0380d4125f5357aac2b98c201ee76c3e26a5ef084a5fcd26e65c9cfff7ad1a026c");

        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 20);
        // nNumIncrBlocks =  5  * 365 * 24 * 60;
        // nNumConstBlocks = 5  * 365 * 24 * 60;
        // nNumDecrBlocks =  10 * 365 * 24 * 60;

        CTransaction txNew;

        // Set the input of the coinbase
        txNew.vin.resize(1);
        txNew.vin[0].scriptSig = CScript() << CScriptNum(0) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));

        // Set the output of the coinbase
        txNew.vout.resize(5);
        for (int i = 0; i < 5; i++) {
            txNew.vout[i].scriptPubKey.clear();
            if (i > 0) 
                txNew.vout[i].scriptPubKey << CScriptNum(25 * i) << OP_CHECKLOCKTIMEVERIFY;
            txNew.vout[i].scriptPubKey << ParseHex("037e6d28a34b6fc0f305162a245ac55b81c3dfff7726f65dd80493b04fcebc76e7") << OP_CHECKSIG;
            txNew.vout[i].nValue = (i > 0 ? 25000000 * COIN : 350000000 * COIN);
        }

        txNew.vout.resize(txNew.vout.size() + 1);

        txNew.vout[txNew.vout.size()-1].scriptPubKey.clear();
        txNew.vout[txNew.vout.size()-1].scriptPubKey 
            << CScriptNum(3)
            << OP_CHECKLOCKTIMEVERIFY
            << OP_HASH160
            << ParseHex("93720988f7ee02f085e6e7a4ef2fe388089bff80")
            << OP_EQUAL;
        txNew.vout[txNew.vout.size()-1].nValue = (124 * COIN);

        genesis.vtx.push_back(txNew);

        
        genesis.nVersion          = 1;
        genesis.SetPoKFlag(true);
        genesis.SetPoK(0);
        genesis.nNonce            = 2811502;
        genesis.nTime             = 1423801719;
        genesis.nBits             = bnProofOfWorkLimit.GetCompact();
        genesis.hashPrevBlock     = 0;
        genesis.hashMerkleRoot    = genesis.BuildMerkleTree();
        genesis.SetPoK(genesis.CalculatePoK());

        hashGenesisBlock          = genesis.GetHash();

        // CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
        // ssBlock << *((CBlockHeader*)&genesis);
        // std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
        // printf("block: %s\n", strHex.c_str());
        // printf("hashGenesisBlock: %s\n", hashGenesisBlock.ToString().c_str());

        if (whichGenesisMine == 1) {
            MineGenesisBlock(genesis, bnProofOfWorkLimit, strDataDir);
        } 

        assert(hashGenesisBlock == uint256("0x00000384571a71cd10e2dfe2bc635da2a5863e8553eaa27d4cdcc169ab651ba0"));//"0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashMerkleRoot == uint256("0x8c7cb543e998040a3fe670d644ebe99ee3f4f9b2f82dcd998b27eb2be1eff5a0"));//"0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));     

        vSeeds.push_back(CDNSSeedData("ziftrcoin.com", "seed1.ziftrcoin.com"));
        vSeeds.push_back(CDNSSeedData("ziftrcoin.com", "seed2.ziftrcoin.com"));

        // Convert the pnSeeds array into usable address objects.
        for (unsigned int i = 0; i < ARRAYLEN(pnSeed); i++)
        {
            // It'll only connect to one or two seed nodes because once it connects,
            // it'll get a pile of addresses with newer timestamps.
            // Seed nodes are given a random 'last seen time' of between one and two
            // weeks ago.
            const int64_t nOneWeek = 7*24*60*60;
            struct in_addr ip;
            memcpy(&ip, &pnSeed[i], sizeof(ip));
            CAddress addr(CService(ip, GetDefaultPort()));
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }
    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const {
        return vFixedSeeds;
    }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


//
// Testnet 
//
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        base58Prefixes[PUBKEY_ADDRESS] = list_of(111);
        base58Prefixes[SCRIPT_ADDRESS] = list_of(196);
        base58Prefixes[SECRET_KEY]     = list_of(239);
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);

        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0x8b;
        pchMessageStart[1] = 0x11;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x06;
        nDefaultPort = 11333;
        nRPCPort = 11332;
        strDataDir = "testnet";
        vAlertPubKey = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");//"02911661822e0b2cd814ed3ff4d23c97cfe7c89ad1eb6ce2ad3181195cfa9e0886");

        genesis.nTime             = 1423802073;
        genesis.nNonce            = 340284;
        genesis.SetPoK(genesis.CalculatePoK());
        
        hashGenesisBlock = genesis.GetHash();

        if (whichGenesisMine == 2) {
            MineGenesisBlock(genesis, bnProofOfWorkLimit, strDataDir);
        }

        assert(hashGenesisBlock == uint256("0x00000bdbbc56422def24bd25c384b4fc127a9b514bde8c5a6757c0447e8e43b0"));
        // Merkle root is the same as parent

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("ziftrcoin.com", "testnet-seed1.ziftrcoin.com"));

        
    }
    virtual Network NetworkID() const { return CChainParams::TESTNET; }
};
static CTestNetParams testNetParams;


//
// Regression test
//
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        pchMessageStart[0] = 0xda;
        pchMessageStart[1] = 0xfb;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xad;
        nDefaultPort = 12333;
        strDataDir = "regtest";

        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 1);

        genesis.nBits             = bnProofOfWorkLimit.GetCompact();
        genesis.nTime             = 1423802140;
        genesis.nNonce            = 1;
        genesis.SetPoK(genesis.CalculatePoK());

        hashGenesisBlock = genesis.GetHash();

        if (whichGenesisMine == 3) {
            MineGenesisBlock(genesis, bnProofOfWorkLimit, strDataDir);
        }
        
        assert(hashGenesisBlock == uint256("0x34591d38a5ad59dbca044da0d12036c22a476f20b2bf0856a5c91179abcc2ad6"));
        // Merkle root is the same as parent

        vSeeds.clear();  // Regtest mode doesn't have any DNS seeds.
    }

    virtual bool RequireRPCPassword() const { return false; }
    virtual Network NetworkID() const { return CChainParams::REGTEST; }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}

void SelectParams(CChainParams::Network network) {
    switch (network) {
        case CChainParams::MAIN:
            pCurrentParams = &mainParams;
            break;
        case CChainParams::TESTNET:
            pCurrentParams = &testNetParams;
            break;
        case CChainParams::REGTEST:
            pCurrentParams = &regTestParams;
            break;
        default:
            assert(false && "Unimplemented network");
            return;
    }
}

bool SelectParamsFromCommandLine() {
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fTestNet = GetBoolArg("-testnet", false);

    if (fTestNet && fRegTest) {
        return false;
    }

    if (fRegTest) {
        SelectParams(CChainParams::REGTEST);
    } else if (fTestNet) {
        SelectParams(CChainParams::TESTNET);
    } else {
        SelectParams(CChainParams::MAIN);
    }
    return true;
}
