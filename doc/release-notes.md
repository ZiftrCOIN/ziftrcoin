ZiftrCOIN Core version 0.9.3 is now available from:

  https://www.ziftrcoin.com

This is the first ever release of ziftrCOIN.

Please report bugs using the issue tracker at github:

  https://github.com/ZiftrCOIN/ziftrcoin/issues

0.9.3 Release notes
=======================

RPC:
- Two new calls 'getusepok' and 'setusepok' (also new launch parameter '-usepok') to set default proof of knowledge configuration for block building
- The getwork command can also be called with "true" or "false" to temporarily override the '-usepok' argument
- Now that OP_CLT and OP_CLTV are supported, the 'listunspent' RPC call has another optional parameter fIncludeDelayedOutputs, default to true. Set this to false if you specifically want to only list transaction outputs that are guaranteed to be spendable in the next block. 
- When blocks are encoded into JSON, they now have a field for 'size' and 'chainsize', where size is the number of bytes in the serialized block and 'chainsize' is the number of bytes in the blockchain up to and including that block. 


Protocol and network code:
- Non-DER encoded signatures are considered non-canonical and invalid.
- 1 Coin = 10^5 ziftrCOIN-satoshis. Min fee is 10^3 satoshis and min relay fee is 10^2 satoshis.

Consensus-changes:
- Optional Proof of Knowledge mining. The hashing of a block header is done in two executions of the ZR5 hashing algorithm (Ziftr's randomized 5 algorithms, keccak first and then the order of the next 4 in the chain is determined from the keccak). Between the two hashes, some of the result of the first hash is stored in the nVersion field of the block header. In this step, if mining with proof of knowledge (a flag stored in the nVersion field as well), then some transaction data is XORed with the result stored in the nVersion of block header. As such, knowledge of transaction data is proven by randomly sampling transaction data. This is advantageous because you can now prove to an SPV node that a block header meets its proof of work with just one extra transaction and merkle branch. When mining with proof of knowledge on, the block reward is allowed to be 5% more than the base reward (fees not included in the extra 5%) to incentivize miners being aware of the transaction data they mine. Miners just not including transactions is mitigated with the tie breaking procedure. 
- Chain Tiebreaker - The chain with the most work is always considered the correct chain. However, when two (or more) blocks are solved on the tip of the chain near simultaneously, the one that spends more sufficiently old (older than 1/2 a day) coins is chosen as the correct block. Near simultaneosly means the node hears about the second new block within 13 seconds of hearing about the first new block. This gives an incentive to include transactions.
- Legacy signature operations counting methods are no longer used.
- Genesis block outputs are spendable.
- Coinbase transactions can have multiple inputs, and each input can be between 2 and 250 bytes long.
- Coinbase outputs need to mature 120 blocks before they can be spent (about 2 hours). But, coinbase outputs with nValue == 0 can be spent at any time, even in the same block. This is done to make it possible to make transactions that get included in the block if and only if the block is solved. For example, if you want to give a transaction spending some sufficiently mature coins to one miner that the miner cannot share with other miners who are not mining the same genesis block (see Tiebreaker section).
- New opcodes, OP_CHECKLOCKTIME and OP_CHECKLOCKTIMEVERIFY allow comparison of the spending transactions nLockTime with the current block height/time. 
- OP_CLT and OP_CLTV delayed transaction outputs are currently not considered standard to mitigate users getting sent coins that they might not be able to spend for a long time.
- LOCKTIME_THRESHOLD was changed from 500000000 (Tue Nov  5 00:53:20 1985 UTC) to 1200000000 (Thu, 10 Jan 2008 21:20:00 GMT), which was necessary to change because ziftrCOIN has faster block times, and this otherwise would become a problem in ~15 years.
- Dynamic block size limit - The main chain keeps track of block sizes and keeps blocks limited to a dynamic limit. The new block size limit is recalculated every 3 months. If during the last 3 months the average block size was more than 2/3 of the current limit and the median block size is greater than 1/2 of the current limit, then the maximum block size is allowed to increase by 10%. As such, artificially inflating the blockchain would either require >50% of the network colluding to keep blocks large or creating man artifically large transactions. The first is a 51% attack and is difficult, the second is expensive. 

Wallet:
- Support for OP_CLT and OP_CLTV. Wallet will not list them in the balance if they are not guaranteed to be spendable in the next block.

GUI:
- There is now a mining tab where the user can turn on mining, control the % of their maximum CPU used on mining, monitor the network hash rate, and see how long before their next expected block solve.

Miscellaneous:
- 
