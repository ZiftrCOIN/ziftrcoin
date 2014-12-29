// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"
#include "main.h"
#include "chainparams.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(main_tests)

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    // Takes a little longer, but is more accurate
    
	uint64_t nSum = 45000000000000; // Genesis block reward is this much
    int nNumDistrBlocks = Params().NumIncrBlocks() + Params().NumConstBlocks() + Params().NumDecrBlocks();
    for (int nHeight = 1; nHeight <= nNumDistrBlocks; nHeight++) {
        uint64_t nSubsidy = GetBlockValue(nHeight, 0);
        BOOST_CHECK(nSubsidy <= MAX_SUBSIDY);
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));

        // if (nHeight % (365*24*60)) == 0) 
        //     printf("I : %i, %llu\n", nHeight/nOneYear, nSum);
    }
    //printf("S : %llu\n", nSum);
    BOOST_CHECK(nSum == 1000200853746000ULL);
    

    // nSum = 45000000000000 - MAX_SUBSIDY;
    // for (int nHeight = 0; nHeight < Params().LastMaxSubsidyBlock(); nHeight += 1000) {
    //     uint64_t nSubsidy = GetBlockValue(nHeight, 0);
    //     BOOST_CHECK(nSubsidy == MAX_SUBSIDY);
    //     nSum += nSubsidy * 1000;
    //     BOOST_CHECK(MoneyRange(nSum));

    //     if (nHeight % nPrintDelta == 0) printf("I : %i, %llu\n", nHeight/nOneYear, nSum);
    // }
    // printf("S : %llu\n", nSum); // 1000200853746000
    // // Have included up to subsidies Params().LastMaxSubsidyBlock()-1 at this point

    // // The decreasing period can be calculated as an arithmetic series
    // // T1 + T2 + T3 + ... + TN = N * (T1 + TN) / 2
    // uint64_t N = (Params().LastDecreasingSubsidyBlock() - Params().LastMaxSubsidyBlock());
    // uint64_t T1 = GetBlockValue(Params().LastMaxSubsidyBlock(), 0);
    // uint64_t TN = GetBlockValue(Params().LastDecreasingSubsidyBlock(), 0);
    // uint64_t S = (N * (T1 + TN));
    // BOOST_CHECK(S % 2 == 0);
    // S /= 2;
    // nSum += S;
    // BOOST_CHECK(MoneyRange(nSum));
    // printf("S: %llu\n", S);
    
}

BOOST_AUTO_TEST_SUITE_END()
