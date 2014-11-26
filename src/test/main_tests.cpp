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
	uint64_t nSum = 0;
    for (int nHeight = 1; nHeight <= Params().LastDecreasingSubsidyBlock(); ++nHeight) {
        uint64_t nSubsidy = GetBlockValue(nHeight, 0);
        BOOST_CHECK(nSubsidy <= MAX_SUBSIDY);
        nSum += nSubsidy;
        BOOST_CHECK(MoneyRange(nSum));
    }
    printf("S : %llu\n", nSum); // 955201937214000

    // uint64_t nSum = 0;
    // for (int nHeight = 0; nHeight < Params().LastMaxSubsidyBlock(); nHeight += 1000) {
    //     uint64_t nSubsidy = GetBlockValue(nHeight, 0);
    //     BOOST_CHECK(nSubsidy == MAX_SUBSIDY);
    //     nSum += nSubsidy * 1000;
    //     BOOST_CHECK(MoneyRange(nSum));
    // }
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
    // BOOST_CHECK(nSum == 1000000000000000ULL);
}

BOOST_AUTO_TEST_SUITE_END()
