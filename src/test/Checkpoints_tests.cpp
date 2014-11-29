// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 pGenesis = uint256("0x000000637d51ae8dfebee2177a7f0b3d92f145b9863b1da24e746113d2ae7536");
    uint256 pNothing = uint256("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe");
    
    if (Checkpoints::fEnabled)
    {
        BOOST_CHECK(Checkpoints::CheckBlock(0, pGenesis));
        
        // Wrong hashes at checkpoints should fail:
        BOOST_CHECK(!Checkpoints::CheckBlock(0, pNothing));

        // ... but any hash not at a checkpoint should succeed:
        BOOST_CHECK(Checkpoints::CheckBlock(1, pNothing));

        BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 0);
    }
}    

BOOST_AUTO_TEST_SUITE_END()
