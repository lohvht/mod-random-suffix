# ![logo](https://raw.githubusercontent.com/azerothcore/azerothcore.github.io/master/images/logo-github.png) AzerothCore
## RandomSuffixModule
- Latest build status with azerothcore: [![Build Status](https://github.com/azerothcore/mod-random-enchants/workflows/core-build/badge.svg?branch=master&event=push)](https://github.com/azerothcore/mod-random-enchants)

<b>Make sure to support the main project:
https://github.com/azerothcore/azerothcore-wotlk/</b>

This module adds in more random custom suffixes and includes a system where all items can roll these new custom random suffixes.

## Important Note
This module makes use of client patching and modification. It is recommended to use a [clean unmodified enUS WoW client](https://www.chromiecraft.com/en/downloads/). A patcher for the WoW client is provided under the `patcher-WoWClient` folder taken from this [forum link](https://model-changing.net/index.php?app=downloads&module=downloads&controller=view&id=314&tab=details) which will remove signature checks, allowing for us to include custom suffixes with names and correct stat calculations on the WoW client side. Do keep a backup of the unmodified client just in case.

## Installation
1. Apply the `acore-modrandomsuffix.patch` to your azerothcore source code. There are multiple ways to achieve this. You can do it using git via this command:
```
git apply --ignore-space-change --ignore-whitespace modules/mod-random-suffix/acore-modrandomsuffix.patch
```
2. Compile, install and run Azerothcore. No replacement of DBC files on the *server side* should be required as the azerothcore DB importer should automagically pick up the custom suffixes to be imported via the `data/sql/db-world` folder.
3. Copy the whole `patch-Z.MPQ` folder into your WoW client `Data` folder.
4. Remove the signature checks via running the patcher inside `patcher-WoWClient`. Copy the `exe` file into the root of your WoW client folder (The folder with `WoW.exe`). Make a backup of `WoW.exe` just in case, doublecheck the checksum of your `WoW.exe` via this link https://github.com/anzz1/wow-client-checksums.

# Credits
- [3ndos](https://github.com/3ndos) for creating the original code for azerothcore of which the main azerothcore `mod-random-enchants` is forks https://github.com/azerothcore/mod-random-enchants
- [The Azerothcore team](https://github.com/azerothcore/) for creating Azerothcore (https://github.com/azerothcore/azerothcore-wotlk). A project that has rekindled my love for WoW as well as game modding.
- The [WoWGaming project](https://wowgaming.github.io/about-us/) with its tools such as the [node-dbc-reader](https://github.com/wowgaming/node-dbc-reader), of which I studied intensely to get a better understanding on how to implement a DBC reader and appender for this project.
- [heyitsbench](https://github.com/heyitsbench), for their [mod-worgoblin](https://github.com/heyitsbench/mod-worgoblin) module giving me the idea and possibility of actually modding the client to get custom suffixes up.
