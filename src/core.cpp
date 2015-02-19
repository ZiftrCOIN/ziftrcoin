// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bignum.h"
#include "chainparams.h"
#include "core.h"

#include "util.h"

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

void COutPoint::print() const
{
    LogPrintf("%s\n", ToString());
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, unsigned int nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn, unsigned int nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

void CTxIn::print() const
{
    LogPrintf("%s\n", ToString());
}

CTxOut::CTxOut(int64_t nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%05d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30));
}

void CTxOut::print() const
{
    LogPrintf("%s\n", ToString());
}

uint256 CTransaction::GetHash() const
{
    return SerializeHash(*this);
}

bool CTransaction::IsNewerThan(const CTransaction& old) const
{
    if (vin.size() != old.vin.size())
        return false;
    for (unsigned int i = 0; i < vin.size(); i++)
        if (vin[i].prevout != old.vin[i].prevout)
            return false;

    bool fNewer = false;
    unsigned int nLowest = std::numeric_limits<unsigned int>::max();
    for (unsigned int i = 0; i < vin.size(); i++)
    {
        if (vin[i].nSequence != old.vin[i].nSequence)
        {
            if (vin[i].nSequence <= nLowest)
            {
                fNewer = false;
                nLowest = vin[i].nSequence;
            }
            if (old.vin[i].nSequence < nLowest)
            {
                fNewer = true;
                nLowest = old.vin[i].nSequence;
            }
        }
    }
    return fNewer;
}

int64_t CTransaction::GetValueOut() const
{
    int64_t nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        nValueOut += txout.nValue;
        if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
    }
    return nValueOut;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
    BOOST_FOREACH(const CTxIn& txin, vin)
    {
        unsigned int offset = 41U + std::min(110U, (unsigned int)txin.scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    if (nTxSize == 0) return 0.0;
    return dPriorityInputs / nTxSize;
}

bool CTransaction::IsCoinBase() const
{
    // Must have at least one null input because the height needs to be put into it
    if (vin.size() == 0 || vin.size() > 25)
        return false;

    BOOST_FOREACH(const CTxIn& input, vin) {
        if (!input.prevout.IsNull())
            return false;
    }

    return true;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}

void CTransaction::print() const
{
    LogPrintf("%s", ToString());
}

// Amount compression:
// * If the amount is 0, output 0
// * first, divide the amount (in base units) by the largest power of 10 possible; call the exponent e (e is max 9)
// * if e<9, the last digit of the resulting number cannot be 0; store it as d, and drop it (divide by 10)
//   * call the result n
//   * output 1 + 10*(9*n + d - 1) + e
// * if e==9, we only know the resulting number is not zero, so output 1 + 10*(n - 1) + 9
// (this is decodable, as d is in [1-9] and e is in [0-9])

uint64_t CTxOutCompressor::CompressAmount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n*9 + d - 1)*10 + e;
    } else {
        return 1 + (n - 1)*10 + 9;
    }
}

uint64_t CTxOutCompressor::DecompressAmount(uint64_t x)
{
    // x = 0  OR  x = 1+10*(9*n + d - 1) + e  OR  x = 1+10*(n - 1) + 9
    if (x == 0)
        return 0;
    x--;
    // x = 10*(9*n + d - 1) + e
    int e = x % 10;
    x /= 10;
    uint64_t n = 0;
    if (e < 9) {
        // x = 9*n + d - 1
        int d = (x % 9) + 1;
        x /= 9;
        // x = n
        n = x*10 + d;
    } else {
        n = x+1;
    }
    while (e) {
        n *= 10;
        e--;
    }
    return n;
}

bool CBlockHeader::CheckProofOfWork() const
{
    uint256 powHash = GetHash();

    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > Params().ProofOfWorkLimit())
        return error("CBlockHeader::CheckProofOfWork() : nBits below minimum work");

    // Check proof of work matches claimed amount
    if (powHash > bnTarget.getuint256())
        return error("CBlockHeader::CheckProofOfWork() : powHash doesn't match nBits");

    return true;
}

uint256 CBlockHeader::GetHash() const
{
    return HashZR5(BEGIN(nVersion), END(nNonce)).trim256();
}

bool CBlock::CheckProofOfWork() const
{
    if (!CBlockHeader::CheckProofOfWork())
        return false;

    // In order to keep the hashing algorithm with/without PoK as similar as possible, the PoK
    // has to be set each time, and has to be verified. The difference is the result returned by PoK.
    // i.e. When the PoK flag is set, the PoK value actually proves knowledge or transaction data.
    try
    {
        return (this->GetPoK() == this->CalculatePoK());
    } 
    catch (std::exception& e) {}

    return false;
}

unsigned int CBlock::CalculatePoK(MapTxSerialized * pmapTxSerialized) const
{
    CBlockHeader header = this->GetBlockHeader();
    header.nVersion = header.nVersion & (~POK_DATA_MASK);

    // The first block of this hash will be the same from nonce to nonce, so if it is ever
    // profitable/worth it, miners can implement a midstate similar to the sha256 midstate
    uint512 hashTxChooser = HashZR5(BEGIN(header.nVersion), END(header.nNonce));
    unsigned int nDeterRand1 = hashTxChooser.getinnerint(0);

    if (!IsPoKBlock())
    {
        return nDeterRand1 & POK_DATA_MASK;
    }

    assert(vtx.size() > 0);
    
    unsigned int nDeterRand2 = hashTxChooser.getinnerint(1);
    unsigned int nDeterRand3 = hashTxChooser.getinnerint(2);
    
    unsigned int nTxIndex =  nDeterRand1 % vtx.size();

    const std::vector<unsigned char> * pvTxData = NULL;
    if (pmapTxSerialized != NULL)
    {
        MapTxSerialized::const_iterator it = pmapTxSerialized->find(std::make_pair(this->hashMerkleRoot, nTxIndex));
        if (it != pmapTxSerialized->end())
        {
            pvTxData = &(it->second);
        }
    }
    
    std::vector<unsigned char> vTxData2;
    if (pmapTxSerialized == NULL || pvTxData == NULL)
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << vtx[nTxIndex];
        vTxData2.insert(vTxData2.end(), ss.begin(), ss.end());

        if (pmapTxSerialized != NULL)
        {
            pmapTxSerialized->insert(std::make_pair(std::make_pair(this->hashMerkleRoot, nTxIndex), vTxData2));
        }
    }

    if (pvTxData == NULL)
        pvTxData = &vTxData2;
    
    assert((pvTxData->end() - pvTxData->begin()) >= 4);
    
    unsigned int nDeterRandIndex = nDeterRand2 % (pvTxData->size() - 3);
    unsigned int nRandTxData = *((unsigned int *)(&(pvTxData->begin()[nDeterRandIndex])));

    unsigned int nPoK = (nRandTxData ^ nDeterRand3) & POK_DATA_MASK;

    // printf("hashTxChooser: %s\n", hashTxChooser.ToString().c_str());
    // printf("nDeterRand1: %u\n", nDeterRand1);
    // printf("nDeterRand2: %u\n", nDeterRand2);
    // printf("nDeterRand3: %u\n", nDeterRand3);
    // printf("vtx size: %lu\n", vtx.size());
    // printf("nTxIndex: %u\n", nTxIndex);
    // printf("tx: %s\n", HexStr(pvTxData.begin(), pvTxData.end()).c_str());
    // printf("tx length: %ld\n", (pvTxData.end() - pvTxData.begin()));
    // printf("nDeterRandIndex: %u\n", nDeterRandIndex);
    // printf("nRandTxData: %u\n", nRandTxData);
    // printf("nPoK: %u\n", nPoK);
    // std::vector<unsigned char> vTxDataSample(&(pvTxData->begin()[nDeterRandIndex]), &(pvTxData->end()[nDeterRandIndex+4]));
    // printf("vTxDataSample: %s\n", HexStr(vTxDataSample.begin(), vTxDataSample.end()).c_str());
    // std::vector<unsigned char> data(BEGIN(nVersion), END(hashMerkleRoot));
    // printf("blockheader: %s\n", HexStr(data.begin(), data.end()).c_str());

    return nPoK;
}

uint256 CBlock::BuildMerkleTree() const
{
    vMerkleTree.clear();
    BOOST_FOREACH(const CTransaction& tx, vtx)
        vMerkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                       BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
        }
        j += nSize;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    BOOST_FOREACH(const uint256& otherside, vMerkleBranch)
    {
        if (nIndex & 1)
            hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
        nIndex >>= 1;
    }
    return hash;
}

void CBlock::print() const
{
    LogPrintf("CBlock(hash=%s, ver=%d, pok=%u, nonce=%u, nTime=%llu, nBits=%08x, hashPrevBlock=%s, hashMerkleRoot=%s, vtx.size()=%u)\n",
        GetHash().ToString(),
        nVersion,
        GetPoK(),
        nNonce,
        nTime,
        nBits,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        LogPrintf("  ");
        vtx[i].print();
    }
    LogPrintf("  vMerkleTree: ");
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        LogPrintf("%s ", vMerkleTree[i].ToString());
    LogPrintf("\n");
}
