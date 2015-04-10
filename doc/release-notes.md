ZiftrCOIN Core version 0.9.31 is now available from:

  http://www.ziftrpool.io/downloads

Please report bugs using the issue tracker at github:

  https://github.com/ZiftrCOIN/ziftrcoin/issues

0.9.31 Release notes
=======================

RPC:
 - New parameter for getaccountaddress: forceOld (bool). If true, this will return the same address. Note, for privacy reasons, you should not use this feature frequently. It is better to use a new address each time one is needed.
 - New rpc call: gettotalcoins {fExcludeGenesis}. Returns the number of coins currently created on the main chain. Call with true to get total coins NOT counting unspent genesis coins.
 - You can now specify the address to send all solo mined coins to by setting the your rpcuser to be a ziftrCOIN address. Configure your ziftrcoin.conf file with:

```
server=1
rpcuser={your ziftrcoin address}
rpcuser={your secret password}
```

 - You can now specify the default pool to use while mining with the following ziftrcoin.conf parameters:
```
poolserver={the pool url}
poolport={the pool port}
poolusername={your pool username}
poolpassword={your pool password}
```

Protocol and network code:
 - Older versions of the wallet would not forward blocks with the PoK data set incorrectly (applies to both PoK and non-PoK blocks). New version will update the PoK before checking validity, and will then forward the block with the PoK data set correctly. This does not expands the space of valid blocks, and is not a hard fork.


Wallet:
 - 


GUI:
 - The whole application has new and improved ziftrCOIN styling. 
 - The mining page now updates and shows your hash rate when mining with GPUs (both Nvidia and AMD) -- assumes you have drivers properly installed. 
 - The wallet no longer freezes after mining for a while, as the log windows is capped at 150 entries.
 - Share counts now properly reset to zero every time mining is restarted. 
 - The proof of knowledge check box has been removed, since none of the current miners support mining with Proof of Knowledge yet.
 - The "Percentage max hash rate" slider now is defaulted to 80% (intensity 16 for AMD cards) and actually adjusts the intensity FOR AMD CARDS (ccminer doesn't have a way to adjust intesnsity yet). 100% corresponds to intensity 20. 


Miscellaneous:
 - 


Known issues:
 - The version number is still at 0.9.31. This will update when it goes out of the experimental builds section and into the official releases. 
 - Some mining files may remain in the ZiftrCoin folder after running the uninstaller on Windows. These can safely be deleted manually, if you wish to uninstall.
 - GPU miner configuration files are untested with the Qt Wallet. They may be ignored or overwritten.
 - Average hash rate displayed for Nvidia GPUs is incredibly volatile and the average is not weighted properly. This is an issue in the stand alone miner and in the Wallet miner.
 - Mining in Mac OSX with an AMD card doesn't work. It hasn't been thoroughly tested though, so feel free to try it anyway.
 - Mining in Mac OSX with an Nvidia card does in fact require drivers.
 - Occasionally when attempting to start mining in OSX, the OS will not let the miner access the proper GPU, causing many errors. Try closing some programs or possibly restarting. It seems to be quite random.


