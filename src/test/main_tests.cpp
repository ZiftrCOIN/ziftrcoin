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

    int64_t nMinSum = 0;
    int64_t nMaxSum = 0;

    int nTotalDays = 2 * Params().GetMidwayPoint();
    for (int nDay = 0; nDay < nTotalDays; nDay++) 
    {    
        int nHeight = nDay * 24 * 60;

        nMinSum += GetBlockValue(nHeight, 0, false);
        nMaxSum += GetBlockValue(nHeight, 0, true);

        int64_t nMinSubsidy = GetBlockValue(nHeight + 1, 0, false);
        int64_t nMaxSubsidy = GetBlockValue(nHeight + 1, 0, true);

        BOOST_CHECK(nMinSubsidy >= MIN_SUBSIDY);
        BOOST_CHECK(nMaxSubsidy >= MIN_SUBSIDY);

        // Should have the same value through out the day (1440 block period)
        BOOST_CHECK(nMinSubsidy == GetBlockValue(nHeight + 24 * 60 - 2, 0, false));
        BOOST_CHECK(nMaxSubsidy == GetBlockValue(nHeight + 24 * 60 - 2, 0, true));

        nMinSum += nMinSubsidy * (24 * 60 - 1);
        nMaxSum += nMaxSubsidy * (24 * 60 - 1);

        BOOST_CHECK(MoneyRange(nMinSum));
        BOOST_CHECK(MoneyRange(nMaxSum));
    }

    // printf("min sum : %llu\n", nMinSum);
    // printf("max sum : %llu\n", nMaxSum);

    BOOST_CHECK(nMinSum == 993742021533401ULL);
    BOOST_CHECK(nMaxSum == 1041179115130496ULL);
    
}

BOOST_AUTO_TEST_SUITE_END()
