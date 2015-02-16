// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "core.h"
#include "main.h"
#include "key.h"
#include "net.h"
#include "base58.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//


//
// SHA256 hashing requires messages which are an integer number of bytes (as we use) 
// to be padded with a 1 bit (0x80 byte), and then all zeroe bytes until reaching a 
// size l (in bytes) such that
//     l + 1 + k = 56 mod 64
// where k is the number of zero bytes.
// This is because the hashing takes place in steps, using 64 bytes in each step, and the last 8
// bytes are reserved for the size of the message. Since the size of the message given is just
// an unsigned int, we
//
// This method requires that there be a buffer of at least 64 bytes
// free after the pbuffer. 
//
// int static FormatHashBlocks(void* pbuffer, unsigned int len)
// {
//     unsigned char* pdata = (unsigned char*)pbuffer;
//     unsigned int blocks = 1 + ((len + 8) / 64);
//     unsigned char* pend = pdata + 64 * blocks;
//     memset(pdata + len, 0, 64 * blocks - len);
//     pdata[len] = 0x80;
//     unsigned int bits = len * 8;
//     pend[-1] = (bits >>  0) & 0xff;
//     pend[-2] = (bits >>  8) & 0xff;
//     pend[-3] = (bits >> 16) & 0xff;
//     pend[-4] = (bits >> 24) & 0xff;
//     return blocks;
// }

//
// SHA256 is done in steps, transforming a 64 byte state. This is the inital state. It is derived from the fractional parts
// of the square roots of the first 8 primtes. 
// See http://crypto.stackexchange.com/questions/1862/how-can-i-calculate-the-sha-256-midstate
// static const unsigned int pSHA256InitState[8] =
// {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};


// 
// Does one step of the SHA256 processing, using 64 bytes of the pinput
// and producing the next state variable in pstate.
// 
// pstate - the result of the transform
// pinput - the input data, the first 64 unsigned chars of which will be processed
// pinit  - the result of the last SHA256 transform on the previous 64 unsigned chars of data,
//          or the pSHA256InitState if this is the first 64 bytes of the message being processed.
// 
// Kept for some unit tests for now
void SHA256Transform(void* pstate, void* pinput, const void* pinit)
{
    SHA256_CTX ctx;
    unsigned char data[64];

    SHA256_Init(&ctx);

    for (int i = 0; i < 16; i++)
        ((uint32_t*)data)[i] = ByteReverse(((uint32_t*)pinput)[i]);

    for (int i = 0; i < 8; i++)
        ctx.h[i] = ((uint32_t*)pinit)[i];

    SHA256_Update(&ctx, data, sizeof(data));
    for (int i = 0; i < 8; i++)
        ((uint32_t*)pstate)[i] = ctx.h[i];
}

// Some explaining would be appreciated
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    double dPriority;
    double dFeePerKb;

    COrphan(const CTransaction* ptxIn)
    {
        ptx = ptxIn;
        dPriority = dFeePerKb = 0;
    }

    void print() const
    {
        LogPrintf("COrphan(hash=%s, dPriority=%.1f, dFeePerKb=%.1f)\n",
               ptx->GetHash().ToString(), dPriority, dFeePerKb);
        BOOST_FOREACH(uint256 hash, setDependsOn)
            LogPrintf("   setDependsOn %s\n", hash.ToString());
    }
};


uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee, so:
typedef boost::tuple<double, double, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;
public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }
    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if(!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // Create coinbase tx
    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(txNew);
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Collect memory pool transactions into the block
    int64_t nFees = 0;
    {
        LOCK2(cs_main, mempool.cs);

        // Largest block you're willing to create:
        unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        // Limit to betweeen 1K and TipMaxBlockSize-1K for sanity:
        nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(chainActive.TipMaxBlockSize()-1000), nBlockMaxSize));
        unsigned int nBlockMaxSigOps = chainActive.TipMaxBlockSigOps();

        // How much of the block should be dedicated to high-priority transactions,
        // included regardless of the fees they pay
        unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
        nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

        // Minimum block size you want to create; block will be filled with free transactions
        // until there are no more or the block reaches this size:
        unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
        nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

        CBlockIndex* pindexPrev = chainActive.Tip();
        CCoinsViewCache view(*pcoinsTip, true);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi)
        {
            const CTransaction& tx = mi->second.GetTx();
            if (tx.IsCoinBase() || !IsFinalTx(tx, pindexPrev->nHeight + 1))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;
            BOOST_FOREACH(const CTxIn& txin, tx.vin)
            {
                // Read prev transaction
                if (!view.HaveCoins(txin.prevout.hash))
                {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash))
                    {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) 
                            assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan)
                    {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }
                const CCoins &coins = view.GetCoins(txin.prevout.hash);

                int64_t nValueIn = coins.vout[txin.prevout.n].nValue;
                nTotalIn += nValueIn;

                int nConf = pindexPrev->nHeight - coins.nHeight + 1;

                dPriority += (double)nValueIn * nConf;
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            // This is a more accurate fee-per-kilobyte than is used by the client code, because the
            // client code rounds up the size to the nearest 1K. That's good, because it gives an
            // incentive to create smaller transactions.
            double dFeePerKb =  double(nTotalIn-tx.GetValueOut()) / (double(nTxSize)/1000.0);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->dFeePerKb = dFeePerKb;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, dFeePerKb, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        while (!vecPriority.empty())
        {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            double dFeePerKb = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Limits on sigOps:
            unsigned int nTxSigOps = GetSigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= nBlockMaxSigOps)
                continue;

            // Skip free transactions if we're past the minimum block size:
            if (fSortedByFee && (dFeePerKb < CTransaction::nMinRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritize by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            int64_t nTxFees = view.GetValueIn(tx)-tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= nBlockMaxSigOps)
                continue;

            CValidationState state;
            if (!CheckInputs(tx, state, view, true, SCRIPT_VERIFY_P2SH))
                continue;

            CTxUndo txundo;
            uint256 hash = tx.GetHash();
            UpdateCoins(tx, state, view, txundo, pindexPrev->nHeight+1, hash);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            nBlockTx++;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f feeperkb %.1f txid %s\n",
                       dPriority, dFeePerKb, tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->dFeePerKb, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }

        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("CreateNewBlock(): total size %u\n", nBlockSize);

        pblock->vtx[0].vout[0].nValue = GetBlockValue(pindexPrev->nHeight+1, nFees, GetBoolArg("-usepok", false));
        pblocktemplate->vTxFees[0] = -nFees;

        // Fill in header
        pblock->SetPoKFlag(GetBoolArg("-usepok", false));
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        UpdateTime(*pblock, pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce         = 0;
        pblock->vtx[0].vin[0].scriptSig = CScript() << OP_0 << OP_0;
        pblocktemplate->vTxSigOps[0] = GetSigOpCount(pblock->vtx[0]);

        CBlockIndex indexDummy(*pblock);
        indexDummy.pprev = pindexPrev;
        indexDummy.nHeight = pindexPrev->nHeight + 1;
        CCoinsViewCache viewNew(*pcoinsTip, true);
        CValidationState state;
        if (!ConnectBlock(*pblock, state, &indexDummy, viewNew, true))
            throw std::runtime_error("CreateNewBlock() : ConnectBlock failed");
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    nExtraNonce++;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required
    pblock->vtx[0].vin[0].scriptSig = (CScript() << CScriptNum(nHeight) << CScriptNum(nExtraNonce));
    unsigned int nScriptSigSize = pblock->vtx[0].vin[0].scriptSig.size();
    assert(MIN_COINBASE_SCRIPTSIG_SIZE <= nScriptSigSize && nScriptSigSize <= MAX_COINBASE_SCRIPTSIG_SIZE);

    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}

#ifdef ENABLE_WALLET

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;


// Returns the number of failed attempts. 
// If ret == nTry, then all failed.
// Else, there were ret+1 trials done and the last one succeeded
//
// (nDontTry should always be zero except for when simulating difficulty retargeting)
//
unsigned int static ScanHash(CBlock *pblock, unsigned int nTry, unsigned int nDontTry, MapTxSerialized * pmapTxSerialized) 
{
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
    uint256 hashBlockHeader;

    for (unsigned int i = 0; i < nTry; i++) 
    {
        pblock->nNonce++;
        pblock->SetPoK(pblock->CalculatePoK(pmapTxSerialized));
        hashBlockHeader = pblock->GetHash();

        if (hashBlockHeader <= hashTarget && i >= nDontTry)
            return i;
    }

    // LogPrintf("pok time: %llu, pow time: %llu, ratio: %d\n", (long long) nTimePoK, (long long)nTimePoW, ((double)nTimePoK) / ((double)nTimePoW));

    return nTry;
}

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reserveKey)
{
    CPubKey pubKey;
    if (!reserveKey.GetReservedKey(pubKey))
        throw std::runtime_error("CreateNewBlockWithKey() : Could not get public key");

    CScript scriptPubKey = CScript() << pubKey << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey);
}

//extern bool fJustPrint;

// If work is checked successfully, then keep the reserve key...
bool CheckWork(CBlock* pblock, CWallet& wallet, CReserveKey& reserveKey)
{
    uint256 hash = pblock->GetHash();
    uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

    if (hash > hashTarget)
        return error("BitcoinMiner : solved block did not have enought work");

    if (pblock->CalculatePoK() != pblock->GetPoK())
        return error("BitcoinMiner : solved block did not have consistent PoK");

    //// debug print
    //pblock->print();
    LogPrintf("BitcoinMiner:\n");
    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner : generated block is stale");

        // Remove key from key pool
        reserveKey.KeepKey();

        // Track how many getdata requests this block gets
        {
            LOCK(wallet.cs_wallet);
            wallet.mapRequestCount[hash] = 0;
        }

        // Process this block the same as if we had received it from another node
        CValidationState state;
        if (!ProcessBlock(state, NULL, pblock))
        {
            //pblock->print();
            return error("BitcoinMiner : ProcessBlock, block not accepted");
        }
    }

    return true;
}

// extern unsigned int nDontTry;

void static BitcoinMiner(CWallet *pwallet)
{
    LogPrintf("ZiftrCOINMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bitcoin-miner");

    // Each thread has its own key and counter
    // Use a reserve key because may not want to keep the key if you don't mine anything
    CReserveKey reserveKey(pwallet);
    unsigned int nExtraNonce = 0;

    try { 
        while (true) {
            if (CLIENT_VERSION_IS_RELEASE && !RegTest()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while (vNodes.empty()) {
                    MilliSleep(1000);
                }
            }

            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            // Automatically delete old block template after each round
            auto_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reserveKey));
            if (!pblocktemplate.get()) {
                LogPrintf("Error in BitcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }

            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);
            MapTxSerialized mapTxSerialized;

            LogPrintf("Running BitcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                   ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

            int64_t nStart = GetTime();
            while (true)
            {
                // Meter hashes/sec
                static int64_t nHashCounter = 0;
                static int64_t nLastLogTime = 0;

                if (nHPSTimerStart == 0) {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }

                unsigned int nTries = 1000; 
                unsigned int nDontHash = 0; // nDontTry; // For simulating increases/decreases of network hash power
                unsigned int nNumFailedAttempts = ScanHash(pblock, nTries, nDontHash, &mapTxSerialized);
                nHashCounter += (nNumFailedAttempts == nTries) ? nTries : nNumFailedAttempts + 1;
                nHashCounter -= nDontHash;

                if (nNumFailedAttempts < nTries) {
                    // Not all tries were fails, so we found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    CheckWork(pblock, *pwallet, reserveKey);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);

                    // In regression test mode, stop mining after a block is found. This
                    // allows developers to controllably generate a block on demand.
                    if (RegTest())
                        throw boost::thread_interrupted();

                    break;
                }

                if (GetTimeMillis() - nHPSTimerStart > 4000) {
                    static CCriticalSection cs; 
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000) {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            if (GetTime() - nLastLogTime > 30 * 60) {
                                nLastLogTime = GetTime();
                                LogPrintf("hashmeter %6.0f hash/s\n", dHashesPerSec);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                if (CLIENT_VERSION_IS_RELEASE && vNodes.empty() && !RegTest())
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 10)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                UpdateTime(*pblock, pindexPrev);
                // if (TestNet()) {
                //     // Changing pblock->nTime can change work required on testnet:
                //     hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
                // }
            }
        } 
    }
    catch (boost::thread_interrupted)
    {
        LogPrintf("BitcoinMiner terminated\n");
        throw;
    }
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        nThreads = RegTest() ? 1 : boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate) {
        return;
    }

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet));
}

#endif

