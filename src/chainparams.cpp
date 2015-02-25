// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The ziftrCOIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "core.h"
#include "protocol.h"
#include "util.h"
#include "script.h"
#include "base58.h"
#include "hash.h" 
// TODO delete me


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

static 
struct {
    int nNumYearsDelay;
    std::string p2shVal;
} COINBASE_ADDRESSES[] = {
    {0, "da3fea3a64eea0574ed66529a7a6ff6c7874a7a3"},
    {0, "4ab6f5fd56eb731e3bcdd8dac9425df9e8b9faab"},
    {0, "21f34867d4980a6e6af7b036ad079598dca81c29"},
    {0, "534a5ace951fb2a070030fbd2a06eab96b224e28"},
    {0, "24f416d527b3d4a39d7cb1e1d83c310eb8f576ba"},
    {0, "ce519691bd7bcb592ee8b0d93aa5a96acd425e74"},
    {0, "6e04fecf244f0fb34e6ca74f81ae0ae51836910e"},
    {0, "3e7563d71532c1300e44135a5debe473214174cd"},
    {0, "22d310cbb24491f5d0fcbec3f44195737acc8d62"},
    {0, "5a0abe050b1d59a0043e37ec1dfd232ad0645790"},
    {0, "3b95ca8ecf71eed291678b26aae8808afc895d4d"},
    {0, "a32b476d35670461504b1cb40377fadf6671632a"},
    {0, "fb5dd45884298c2f22ad9af59206c88d3f5055c8"},
    {0, "ac1c4e518133f2235b44a4299f007f843dffec16"},
    {0, "007da34bb38387c91429ab2800895740081f77cb"},
    {0, "919b7e3aa625d1986b6b4c83614de578a2686171"},
    {0, "a8c7fe8cb2dc0e42a8e4f367bf5ce7c76af1e7f0"},
    {0, "e979ca5952dd2d393f48ff034d3e6659539da25f"},
    {0, "5d095df9efd0ccd88a023bcba1f29ed05ec3cf87"},
    {0, "fa26a7db318bf77a6ef367fbba8d9445de404e0a"},
    {0, "87deca17e1a32d9f94cb334ab78b20106f7de0f1"},
    {0, "12bdf80fdd217062403408b1bdddb18847f2a0b6"},
    {0, "91709dbd34e912c6b1fb2c36db8b08bbef862ff0"},
    {0, "a165334f0a8db36609de29783123e57655244b3b"},
    {0, "dbc150fab7c2cc9afb03b84a6be1246fea2951f8"},
    {0, "ae5df9a8f256ee52886b1cabf421e96d76f87e62"},
    {0, "7c0f10a29b33353968d18649e33698d7905d259e"},
    {0, "4bd6c51f4852e04f39478dcda32575ca65319ee3"},
    {0, "6d47d036108d961d7f406bb6b04cd8bb5704a20b"},
    {0, "05c73b92da9a7cecdb3faeaf211ab9191a39407a"},
    {0, "12197b4b504d14ecbd65eea52f16de5db9d56c51"},
    {0, "74245399e24a94fd480f10de4d96ce6e15a04276"},
    {0, "3c95f77c8c0369cd8ffeff9e387b2dda84f7a73b"},
    {0, "c4bc9cf596a338e41850cc040e9f461eed746fe0"},
    {0, "fe90295b9619ad1cdcb1b0b70700abfde92943c4"},
    {0, "e40ef4bec3c7c22e26ab4e293cd9c1e76468cb44"},
    {0, "a7e5bc5ca0d3a782caa4e8a1c5eccd67309d0f16"},
    {0, "3eb549a64c9d894989f25081cbeb7cf809dcbd30"},
    {0, "ceeab52b623544fc2f27ef180c88068a8d503796"},
    {0, "7be344ed8b68e0a13b9f1164ca0c9db50057f4c7"},
    {0, "14acff4287059b8c547b6bbb49458c6d1d577e8c"},
    {0, "efbd574253b64703270449a0302a06e40c118abd"},
    {0, "79bb4091dd6879df0ba492f268bcba086b316dd2"},
    {0, "96e1f73282f87a4065f577c0e7c00df5e9e4ef65"},
    {0, "c4e5922f0abd253d8b14a573c5284f19ebcaabaf"},
    {0, "5baa75da77f4f1140b296e51bda96b597d7fc6be"},
    {0, "d32d9281dfbbef3ba052a5409c0b84eda64077b0"},
    {0, "08802fd3e05e55a13d4008f9061bcb2aa4f3677a"},
    {0, "265a7428508ba3a861220737038d240539b72995"},
    {0, "faa259cd2cada0187deb904c95063f0f077e7d25"},
    {0, "ff4a33698f6f549d4d340655865c79c52d4a2cc2"},
    {0, "870fa70d6e7c60df69c402bde450d6aa40275026"},
    {0, "42e2bdeba00356442562600299dfe13f47359e6b"},
    {0, "09a62ecd7d65193dc5ffe17480d51ba25b557a3c"},
    {0, "48050f29b42d6167ac8146734d96c4e0a388a8ab"},
    {0, "ef179cf6a62f6cfc8a5d428bf03534f65b121920"},
    {0, "56dc5decec2ea2cb10862babab61e79058f523ef"},
    {0, "4e3db41e93fc2b43e6231d2128e4753b4cab528d"},
    {0, "729ab83d2275946dd32861c8cc9547b7ee4e42cd"},
    {0, "e33c26a52e43c9ab73315daf220b6cb0572d29bf"},
    {0, "a2ff3f65bec27aee54d3d30027114ad7be020747"},
    {0, "4e632c8ba99a4f84cd50073e91ddb0f9f5aaeea6"},
    {0, "c552654ce54d4d9579299b34d93accb76e3d1991"},
    {0, "2efaed0e80b9b566bc2d3b255a7d05375cc1c670"},
    {0, "624ae1942913c16fb3e1009a69d6445a4356331b"},
    {0, "7ebda584319a4598f4b1bf5ada8866b5ab4a3822"},
    {0, "45dca6ee404f2067d195df535d4d4958aa38d024"},
    {0, "34a3fa5738d0bd8afc65aa1009fd8d18fd46e874"},
    {0, "bcfaddf54ceb565809e1b617b6e6d9f49f5e2f3b"},
    {0, "269957be2d7164d808760efbccb6cca806e3601b"},
    {1, "df8934cac9f50aca1383069a85e7dc1b97cf4bfa"},
    {1, "ee7a965b8710906da31db9b03900416d68c99af4"},
    {1, "9788c8d6ba76ddb8ae4d354b7b5fe390a923b98c"},
    {1, "0a568d39c7b3c7f439e563ca6415d72ad7782124"},
    {1, "85898b7dae8f7be10f04d38d990e8ad2155f867a"},
    {2, "c92297b5759d3108579ce9ca0e488000b08c0ed6"},
    {2, "247fce461aa05e754c6028ce482bdfad37abdcd4"},
    {2, "c19c7ad91e65e41a3aae384dbc01fa44d4d87ab6"},
    {2, "a4f0c847934b1dbcee451327e82df7d58f98284d"},
    {2, "502e8799a4e7d8079c395fec454ebde6d827131c"},
    {3, "4b617725bccc1d5af81e7a6c3e62583518cb36cc"},
    {3, "fe40d3c4082030164f239832336e46c43d391f06"},
    {3, "e078aab6c661dc0fc9f09cb4e0def11244711cd4"},
    {3, "af8b21a19c5d93e02b41c8a1b61bb09656f75b25"},
    {3, "fcdd823ebd0174aa6cf66106b82e378aaeb1c8e3"},
    {4, "656a286a77395ccae2ab69bf6541d8194f5f60ad"},
    {4, "9afdfd32396e64808ec1c181fad25df28e5582e9"},
    {4, "1a2666b75add11454f13cb2dee30539d62720958"},
    {4, "ac26ad5958a6ea7049fd4de43815fc4111cacd72"},
    {4, "8acf46b0457528891a7defcf0b6b517a3bce23ac"}
};

static void MineGenesisBlock(CBlock * genesis, const string& pref) 
{
    uint256 proofOfWorkLimit = CBigNum().SetCompact(genesis->nBits).getuint256();
    genesis->nNonce = 0;
    uint256 hashGenesisBlock;

    do {
        genesis->nNonce++;
        genesis->SetPoK(genesis->CalculatePoK());
        hashGenesisBlock = genesis->GetHash();
    } while (hashGenesisBlock > proofOfWorkLimit);
    
    printf("%s pok: %u\n", pref.c_str(), genesis->GetPoK());
    printf("%s nonce: %u\n", pref.c_str(), genesis->nNonce);
    printf("%s time: %llu\n", pref.c_str(), (long long)genesis->nTime);
    printf("%s hash: %s\n", pref.c_str(), genesis->GetHash().ToString().c_str());
    printf("%s merkleRoot: %s\n", pref.c_str(), genesis->hashMerkleRoot.ToString().c_str());
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

        CTransaction txNew;

        // Set the input of the coinbase
        txNew.vin.resize(1);
        txNew.vin[0].scriptSig = CScript() << CScriptNum(0) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));

        // Set the output of the coinbase
        txNew.vout.resize(90);
        for (int i = 0; i < 90; i++) {
            txNew.vout[i].scriptPubKey.clear();

            if (COINBASE_ADDRESSES[i].nNumYearsDelay > 0)
                txNew.vout[i].scriptPubKey << CScriptNum(COINBASE_ADDRESSES[i].nNumYearsDelay * 365 * 24 * 60) << OP_CHECKLOCKTIMEVERIFY;

            txNew.vout[i].scriptPubKey 
                << OP_HASH160
                << ParseHex(COINBASE_ADDRESSES[i].p2shVal)
                << OP_EQUAL;

            txNew.vout[i].nValue = 5000000 * COIN;
        }

        genesis.vtx.push_back(txNew);
        
        genesis.nVersion          = 1;
        genesis.SetPoKFlag(true);
        genesis.SetPoK(0);
        genesis.nNonce            = 2334250;
        genesis.nTime             = 1424821808;
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
            MineGenesisBlock(&genesis, strDataDir);
        } 

        assert(hashGenesisBlock == uint256("0x0000035888fea2f96e3ef996beda4fa4c4c4d03b371184d1f575f9d144b7a164"));
        assert(genesis.hashMerkleRoot == uint256("0x3666f225d97542fb9f70a122aae6c40ad5dd454886f0226e8bf2d4875132b02a"));

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

        genesis.nTime             = 1424821917;
        genesis.nNonce            = 271066;
        genesis.SetPoK(genesis.CalculatePoK());
        
        hashGenesisBlock = genesis.GetHash();

        if (whichGenesisMine == 2) {
            MineGenesisBlock(&genesis, strDataDir);
        }

        assert(hashGenesisBlock == uint256("0x0000059f6eaa1fa17cdc2ef09aae2c772c8c50dc45f644d3bf6748d369a5cb23"));
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
        genesis.nTime             = 1424822042;
        genesis.nNonce            = 1;
        genesis.SetPoK(genesis.CalculatePoK());

        hashGenesisBlock = genesis.GetHash();

        if (whichGenesisMine == 3) {
            MineGenesisBlock(&genesis, strDataDir);
        }
        
        assert(hashGenesisBlock == uint256("0x1fb10915b22f24d4794495c2bb6fe51327a310e3577846ace24a6cb1acf515f1"));
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
