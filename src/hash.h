// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2015 The ziftrCOIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include "bignum.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"
#include "util.h"

#include "sph_blake.h"
#include "sph_groestl.h"
#include "sph_jh.h"
#include "sph_keccak.h"
#include "sph_skein.h"

#include <vector>

#include <openssl/ripemd.h>
#include <openssl/sha.h>

#ifdef GLOBALDEFINED
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL sph_blake512_context    z_blake;
GLOBAL sph_groestl512_context  z_groestl;
GLOBAL sph_jh512_context       z_jh;
GLOBAL sph_keccak512_context   z_keccak;
GLOBAL sph_skein512_context    z_skein;

#define fillz() do { \
    sph_blake512_init(&z_blake); \
    sph_groestl512_init(&z_groestl); \
    sph_jh512_init(&z_jh); \
    sph_keccak512_init(&z_keccak); \
    sph_skein512_init(&z_skein); \
} while (0)

#define ZBLAKE   (memcpy(&ctx_blake,    &z_blake,    sizeof(z_blake)))
#define ZGROESTL (memcpy(&ctx_groestl,  &z_groestl,  sizeof(z_groestl)))
#define ZJH      (memcpy(&ctx_jh,       &z_jh,       sizeof(z_jh)))
#define ZKECCAK  (memcpy(&ctx_keccak,   &z_keccak,   sizeof(z_keccak)))
#define ZSKEIN   (memcpy(&ctx_skein,    &z_skein,    sizeof(z_skein)))

//static const int KECCAK  = -1;
static const int BLAKE   = 0;
static const int GROESTL = 1;
static const int JH      = 2;
static const int SKEIN   = 3;

template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

class CHashWriter
{
private:
    SHA256_CTX ctx;

public:
    int nType;
    int nVersion;

    void Init() {
        SHA256_Init(&ctx);
    }

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {
        Init();
    }

    CHashWriter& write(const char *pch, size_t size) {
        SHA256_Update(&ctx, pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 hash1;
        SHA256_Final((unsigned char*)&hash1, &ctx);
        uint256 hash2;
        SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
        return hash2;
    }

    template<typename T>
    CHashWriter& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};


template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end,
                    const T3 p3begin, const T3 p3end)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (p1begin == p1end ? pblank : (unsigned char*)&p1begin[0]), (p1end - p1begin) * sizeof(p1begin[0]));
    SHA256_Update(&ctx, (p2begin == p2end ? pblank : (unsigned char*)&p2begin[0]), (p2end - p2begin) * sizeof(p2begin[0]));
    SHA256_Update(&ctx, (p3begin == p3end ? pblank : (unsigned char*)&p3begin[0]), (p3end - p3begin) * sizeof(p3begin[0]));
    SHA256_Final((unsigned char*)&hash1, &ctx);
    uint256 hash2;
    SHA256((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

template<typename T>
uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

template<typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    uint256 hash1;
    SHA256((pbegin == pend ? pblank : (unsigned char*)&pbegin[0]), (pend - pbegin) * sizeof(pbegin[0]), (unsigned char*)&hash1);
    uint160 hash2;
    RIPEMD160((unsigned char*)&hash1, sizeof(hash1), (unsigned char*)&hash2);
    return hash2;
}

inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    return Hash160(vch.begin(), vch.end());
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

typedef struct
{
    SHA512_CTX ctxInner;
    SHA512_CTX ctxOuter;
} HMAC_SHA512_CTX;

int HMAC_SHA512_Init(HMAC_SHA512_CTX *pctx, const void *pkey, size_t len);
int HMAC_SHA512_Update(HMAC_SHA512_CTX *pctx, const void *pdata, size_t len);
int HMAC_SHA512_Final(unsigned char *pmd, HMAC_SHA512_CTX *pctx);

    // unsigned int nOrderRev = ByteReverse(nOrder);
    // std::cout << "data      : " << HexStr(pbegin, pend) << std::endl;
    // std::cout << "keccak    : " << HexStr(BEGIN(hash[0]), END(hash[0])) << std::endl;
    // std::cout << "inner int : " << nOrder << std::endl;
    // std::cout << "ii hex    : " << HexStr(BEGIN(nOrder), END(nOrder)) << std::endl;
    // std::cout << "int val   : 0x" << HexStr(BEGIN(nOrderRev), END(nOrderRev)) << std::endl;
    // nOrder = nOrder % ARRAYLEN(arrOrder);
    // std::cout << "ordr      : " << nOrder << std::endl;
    // std::cout << "zr5       : " << hash[4].ToString() << std::endl;
    // std::cout << "trimmed   : " << hash[4].trim256().ToString() << std::endl;
    //     std::cout << std::endl;
    // for (int i = 0; i < 5; i++) std::cout << hash[i].ToString() << std::endl;

/* ----------- ziftrCOIN Hash ------------------------------------------------ */
template<typename T1>
inline uint512 HashZR5(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1];
    pblank[0] = 0;

    // Pre-computed table of permutations
    static const int arrOrder[][4] = 
    {
        {0, 1, 2, 3},
        {0, 1, 3, 2},
        {0, 2, 1, 3},
        {0, 2, 3, 1},
        {0, 3, 1, 2},
        {0, 3, 2, 1},
        {1, 0, 2, 3},
        {1, 0, 3, 2},
        {1, 2, 0, 3},
        {1, 2, 3, 0},
        {1, 3, 0, 2},
        {1, 3, 2, 0},
        {2, 0, 1, 3},
        {2, 0, 3, 1},
        {2, 1, 0, 3},
        {2, 1, 3, 0},
        {2, 3, 0, 1},
        {2, 3, 1, 0},
        {3, 0, 1, 2},
        {3, 0, 2, 1},
        {3, 1, 0, 2},
        {3, 1, 2, 0},
        {3, 2, 0, 1},
        {3, 2, 1, 0}
    };

    uint512 hash[5];

    sph_blake512_context   ctx_blake;
    sph_groestl512_context ctx_groestl;
    sph_jh512_context      ctx_jh;
    sph_keccak512_context  ctx_keccak;
    sph_skein512_context   ctx_skein;

    const void * pStart = (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0]));
    size_t nSize        = (pend - pbegin) * sizeof(pbegin[0]);
    void * pPutResult   = static_cast<void*>(&hash[0]);

    sph_keccak512_init(&ctx_keccak);
    sph_keccak512 (&ctx_keccak, pStart, nSize);
    sph_keccak512_close(&ctx_keccak, pPutResult);

    unsigned int nOrder = hash[0].getinnerint(0) % ARRAYLEN(arrOrder);
    
    for (unsigned int i = 0; i < 4; i++)
    {
        pStart     = static_cast<const void*>(&hash[i]);
        nSize      = 64;
        pPutResult = static_cast<void*>(&hash[i+1]);

        switch (arrOrder[nOrder][i])
        {
        case BLAKE:
            sph_blake512_init(&ctx_blake);
            sph_blake512 (&ctx_blake, pStart, nSize);
            sph_blake512_close(&ctx_blake, pPutResult);
            break;
        case GROESTL: 
            sph_groestl512_init(&ctx_groestl);
            sph_groestl512 (&ctx_groestl, pStart, nSize);
            sph_groestl512_close(&ctx_groestl, pPutResult);
            break;
        case JH: 
            sph_jh512_init(&ctx_jh);
            sph_jh512 (&ctx_jh, pStart, nSize);
            sph_jh512_close(&ctx_jh, pPutResult);
            break;
        case SKEIN:
            sph_skein512_init(&ctx_skein);
            sph_skein512 (&ctx_skein, pStart, nSize);
            sph_skein512_close(&ctx_skein, pPutResult);
            break;
        default:
            break;
        }
    }

    return hash[4];
}

#endif
