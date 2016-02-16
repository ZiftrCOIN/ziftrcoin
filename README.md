=======
ZiftrCOIN Core integration/staging tree
=====================================

http://www.ziftrcoin.org

What is ZiftrCOIN?
----------------

ZiftrCOIN is an experimental new digital couponing system.

For more information, see http://www.ziftrcoin.com.

Each ziftrCOIN will have a minimum redemption value of $1 within the ziftr merchant network for up to 5% of the customers transaction.

License
-------

ZiftrCOIN Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see http://opensource.org/licenses/MIT.

Configuration
----------------

A few new configuration parameters have been added:

 - "-usepok": Controls how blocks are built, 1 for building blocks with proof of knowledge and 0 for without.
 - "-poolserver": The pool server URL.
 - "-poolport": The pool server port.
 - "-poolusername": The pool server username for authentication.
 - "-poolpassword": The pool server password, also (usually) for authentication.
 - "-usecuda": Used to manually force the GUI miner to try to GPU mine with Cuda miner.
 - "-useamd": Used to manually force the GUI miner to try to GPU mine with AMD miner.

In addition, the "-rpcuser" configuration parameter now has an extra function. If it is a valid ziftrCOIN address, then any coins that are mined with that node are sent to that address. The code for this can be found in `miner.cpp:CreateNewBlockWithKey`. 

<hr>

Technical Notes
----------------

These are the main features of ziftrCOIN.

### Dynamic block size

There is not a hard cap on the block size limit in ziftrCOIN. Instead, if both the mean of the last 3 months worth of blocks is greater than 2/3 of the current block size limit and the median is greater than 1/2 of the current block size limit, then the new block size limit for the next 3 months is increased by 10%. This allows for the network to grow dynamically according to its use, and avoids the need for a hard fork when transaction volume spikes. If this happens consistently every 3 months, then this creates about a 2x increase in block size every 2 years.

The block size limits are stored in the `CChain` class, defined in main.h and implemented in main.cpp. Internally, the blocksize limits are stored in `CChain`'s `mapBlockSizeLimits` property, which is a map of block height to limit at that height, with the caveat that the keys are multiples of 3 months (every 131400 blocks). Whenever a new tip is set via `CChain::SetTip`, the `mapBlockSizeLimits` is kept up to date.

With the `mapBlockSizeLimits` populated, the network's max allowed block size at any block height is accessible via `CChain::MaxBlockSize`, and the tip max block size via `CChain::TipMaxBlockSize`. In addition, the maximum number of signature operations scales linearly with the maximum block size, accessible via `CChain::MaxBlockSigOps` and `CChain::TipMaxBlockSigOps`.

### Proof of Knowledge Mining

ZiftrCOIN uses a mining process that can be configured to prove that a miner has knowledge of the transaction data they are mining. When mining with proof of knowledge turned on, you can claim 5% more in your block rewards.* Proof of Knowledge mining requires that you prove you know the transaction data in your block, meaning you likely built the block yourself instead of blindly mining on work that a pool operator gave you. This levels the playing field for mining pools and miners who build blocks themselves, as the pools will make less money without the 5% bonus but will also have less variance in rewards, due to the pooling of hash power.

The technical details of mining with and without Proof of Knowledge follow.

The mining process basically consists of:

 1. Build block and calculate merkle root.
 2. Set the POK_BOOL part of the nVersion field of the block header to 1 if mining with proof of knowledge, 0 for no PoK.
 3. Set the POK_DATA part of the nVersion field of the block header to all 0s.
 4. ZR5 hash the block header. This is basically Keccak first and then the next 4 (JH, Grøstl, Skein, Blake) are done in a different order each time depending on the result of the Keccak.
 5. Put a few bytes of the result of 4 into the POK_DATA part of the block header. If the POK_BOOl is set, also XOR the few bytes with some transaction data. Exactly which TX data is determined by the result of the ZR5 in step 4.
 6. Now that the fields are in place, ZR5 hash the block header and see if meets the current difficulty!

The byte-masks for the nVersion field are shown in [core.h](https://github.com/ZiftrCOIN/ziftrcoin/blob/master/src/core.h#L348-L350). The selection of which transaction data to prove knowledge of is shown in [core.cpp](https://github.com/ZiftrCOIN/ziftrcoin/blob/master/src/core.cpp#L272).

One of the nice things about proving knowledge of transaction data by pseudorandom sampling is that it can be verified with much less data than was required to actually mine the block. For example, an SPV node can verify a block header by downloading the header and just one extra transaction with its merkle branch.

Also note that the reason that the POK_DATA field is set each time regardless of whether you are mining with PoK or not is to keep mining with/without PoK as similar as possible, so that their work/hashes can be compared. Mining with PoK involves an initial serialization and then then an additional XOR each time, but the serialization is just done once (per update of merkle root) and the XOR is very fast.

#### ZR5

All that is left is to explain what happens in ZR5. As mentioned above, ZR5 (ziftr's set of 5 randomized algorithms) is basically a chained hash, with Keccak executed first and then the next 4 (JH, Grøstl, Skein, Blake) done in a pseudorandom order dependent on the result of the Keccak. The code for this is in [hash.h](https://github.com/ZiftrCOIN/ziftrcoin/blob/master/src/hash.h#L195).

After the Keccak is executed, the least significant 32 bits are used as in integer to calculate the order of the next 4 hashes. Take that interger, mod it by 24, and use the table [here](https://github.com/ZiftrCOIN/ziftrcoin/blob/master/src/hash.h#L203-L226) to determine the order of the next 4. Each entry in the table represents a permutation of the algorithms, where these are the [algo ids](https://github.com/ZiftrCOIN/ziftrcoin/blob/master/src/hash.h#L54-L57).

A sample ZR5 hash is included here for anyone looking to implement a compatible function:

    data      : 0180648600000000000000000000000000000000000000000000000000000000000000002ab0325187d4f28b6e22f0864845ddd50ac4e6aa22a1709ffb4275d925f26636300eed54ffff0f1e2a9e2300
    keccak    : 51b4309f789fbaa7e5f0402b70072a60e8a705b19592386bd1363779935c45f011e13363491820950392502a9d3c1f76451c379f9d983c98864322f2ecb95f84
    inner int : 2670769233
    ii hex    : 51b4309f
    int val   : 0x9f30b451
    ordr      : 9
    groestl   : a680a9a4efdb92358271c01bc8df3ce3a1422353f49c4ddc28b0aea97a8540fabefe14883e4c0d20be78266d35b67d03a07d6ae6cfd9d489f0322950135b2233
    jh        : 27000462af982b5076bc6c5edbbc7623c1bfe9a80557c5ce6651d710f418adba8e9027a2e01441c9973f1dac79a8ecfc7acc18b0a0b38c8a04f21e13d3ac0f4e
    skein     : 0080d2bf9e50cd459a80ec355dfc9e291f84333dc74b8f8c7be22020fdd5fac5a8df0d1ac4a016ccd1918e7a60a73335e2244cbd6434ca7c5534fdd4293b3537
    blake     : 64a1b744d1f975f5d18411373bd0c4c4a44fdabe96f93e6ef9a2fe8858030000354cc5ef96e59f00d186c5543f8c2b01c61e7698da0ba647ed49dce33b3d0cee
    zr5_512   : ee0c3d3be3dc49ed47a60bda98761ec6012b8c3f54c586d1009fe596efc54c350000035888fea2f96e3ef996beda4fa4c4c4d03b371184d1f575f9d144b7a164
    zr5       : 0000035888fea2f96e3ef996beda4fa4c4c4d03b371184d1f575f9d144b7a164

#### Relevant RPC calls

 1. Two new calls 'getusepok' and 'setusepok' (also new launch parameter '-usepok') to set default proof of knowledge configuration for block building. For example, 'setusepok true' turns mining with proof of knowledge on.
 2. The getwork command can also be called with "true" or "false" to temporarily override the '-usepok' argument

### Built in OP_CHECKLOCKTIME and OP_CHECKLOCKTIMEVERIFY

This is a feature added to the scripting language for verifying the validity of spends. It allows you to create a transaction output which cannot be spent until a certain time in the future.

Bitcoin recently soft-forked in OP_CLTV, but ziftrCOIN comes with both OP_CLTV and its counterpart OP_CLT built in. OP_CLT is basically OP_CLTV with an OP_VERIFY right afterward, where OP_CLT just pushes the boolean on the stack instead of verifying that it is true, which can be useful in some cases. OP_CLTV verifies that a spending transactions locktime is set, is in effect, and is at least a certain time specified by the script. Since transactions cannot be mined that a lock time that is in the past, OP_CLTV essentially creates outputs which cannot be spent until a certain time in the future. The relation looks like this:

```
(time specified by script using OP_CLTV) < Spending Transaction's locktime < Real time
```

The inequality that we actually care about is the inequality above without the middle parameter, so as to make sure that the real time that a transaction is spent is after the time specified in the script that uses OP_CLTV. The spending transactions locktime is a convenient intermediate comparison.

All relevant code is in `script.h` and `script.cpp`. OP_CLT uses opcode 0xBA and OP_CLTV uses opcode 0xBB.

### Mature coins spent as a Tie Breaking Algorithm

ZiftrCOIN uses a custom chain tie-breaking algorithm to choose locally correct chains in the event that a new block is solved while another is propagating. When nodes hear of a new solved block, they essentially start a 13-second timer. If before the timer ends, the node hears about a new block and the new block spends more mature coins than the alternate block did, then the node will choose it as the tip of the new correct chain. This will allow the network to quickly come to a consensus as to the correct chain in the event of multiple blocks simultaneously being solved.

The chain with the most work is always considered the correct chain. However, when two (or more) blocks are solved on the tip of the chain near simultaneously, the one that spends more sufficiently old (older than 1/2 a day) coins is chosen as the correct block. Near simultaneosly means the node hears about the second new block within 13 seconds of hearing about the first new block. This gives an incentive to include transactions.

In the code, the number of sufficiently mature coins that each block spends is stored in each `CBlockIndex`, in the `nMatureCoinsSpent` property (defined in `main.h`). This field is populated when `main.cpp:CountMatureCoins` is called, which is called by `main.cpp:AddToBlockIndex`.  Only coins that are older than the `TRANSACTION_MATURITY_DEPTH` are added to the mature coins count, and there is no multiplier.

After the mature coins are counted, then instances of `CBlockIndex` are compared to find the best chain in the `main.cpp:CBlockIndexWorkComparator` comparator. This comparator first compares by work, then by height, and then, if all of the necessary preconditions are met, it will compare by the number of mature coins spent in a block. Those preconditions include checking that the second received header came in less than 13 seconds after the first received header.


### Bell Curve Distribution

Most coins have a constant reward for a period of time and then that reward halves at a fixed frequency. The distribution of ziftrCOINs, however, follows a standard bell curve, as that is typically predicts the rate at which new technologies are adopted. The block reward is constant throughout the day, changing after a day's worth of block. This curve was chosen to model the adoption rates of new technologies.

This distribution is codified by the `main.cpp:GetBlockValue` function. The formula within that function is derived from the derivative of the logistic equation, which is the bell curve. The 4.5% of coins set aside for distribution is taken evenly from each day of the 30 year distribution, resulting in a constant decrease from the result of the bell curve calculation.

The formula for the bell curve distribution is below, where x is a constant spread multiplier times the number of days from the half way (15 year) marker.

```
    e^(-x)
----------------
(1 + e^(-x)) ^ 2
```

![ZRC](https://d19y4lldx7po3t.cloudfront.net/assets/images/coin_specs/charts_reward_690.png)

### Difficulty Calculations

ZiftrCOIN uses the same basic approach to calculating the next difficulty, although the frequency that this calculation is done is much higher, at once every 4 blocks. In addition, rather than a 10 minute block time, we use a 1 minute target block time. These constants are defined in `main.cpp`, as `RETARGET_INTERVAL` and `TARGET_SPACING`.

#### Fixed the Time Warp Attack

Bitcoin has a known bug in its difficulty calculation procedure where a coordinated effort of miners, or a 51% attack, may result in an artificial lowering of the difficulty by carefully chosen timestamps. This is possible because the last block of one difficulty recalculation period is not used as the first block of the next difficulty recalculation period. This is often called an 'off-by-one' error in the `main.cpp:GetNextWorkRequired` function.

In ziftrCOIN, this bug has been fixed by standardizing the blocks chosen as to calculate the next difficulty, ensuring that the last block of one period is used as the first block in the next period. As noted, the relevant code is in `main.cpp:GetNextWorkRequired`.

### What Defines a Coinbase Transaction?

Bitcoin requires that a coinbase transaction have only one input and that it have less than 100 bytes in its `scriptSig`. We found this to be unnecessarily restrictive, and changed the `MAX_COINBASE_SCRIPTSIG_SIZE` to be 250 bytes, and allow up to 25 inputs in the coinbase transaction. The script size limit is checked in `main.cpp:CheckTransaction` and the number of inputs is checked in `core.cpp:CTransaction::IsCoinBase()`.

### Accurate Signature Operation Counting

Bitcoin uses a legacy signature operation counting method. This has been removed without the need for backwards compatibility, and the main signature operation counting methods are now `GetSigOpCount` and `GetP2SHSigOpCount`. The max sig ops limit is checked in `main.cpp:ConnectBlock`. 

<hr>

Testing
----------------

All unit tests are passing. Note that strict DER encoding is required in ziftrCOIN.

The integration and regression tests (qa folder) have not been updated, so they cannot be expected to run successfully.

<hr>

GUI
------------

The GUI has been completely redone, and looks quite good. All of this work is merged into the `gpu-integration` branch, which has not yet been merged into master.



