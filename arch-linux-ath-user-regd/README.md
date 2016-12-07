# linux-ath-user-regd

This repository contain the `PKGBUILD` and associated files for `linux-ath-user-regd`, an Arch Linux kernel with patched Atheros drivers that ignore regulatory domain settings in the wireless cards' EEPROM. The purpose of the patches is to enable consumer Atheros wireless cards to be used in master mode (i.e. AP mode) on 5GHz.

## Why?

Many Atheros cards are set to the "World" regulatory domain in EEPROM, putting many restraints on what they are allowed to do. In particualar, all channels in the 5GHz band have the "No-IR" flag set, meaning the card will not work in master mode on these channels. While a different regulatory domain can be imposed in software (for example by `iw reg set US`), the original restrictions will not be lifted, and both set of restrictions apply at the same time.

Some links on the regulatory domain setting: [drivers:ath][1] [regulatory:processing_rules][2]

## What?

2 patches are cherry picked from [OpenWRT's kernel patches][3]. The first one patch out reading regdomain from EEPROM, and the second disable ["Country IE" processing][2]. These should allow `ath5k` and `ath9k` devices to work in master mode on 5GHz channels. `ath10k` probably won't work: [see known bugs/limitations here][4]

(There is probably a way to apply the patches using `DKMS`, thus avoiding compiling the entire kernel, but I haven't figured out how.)

## How?

On Arch Linux, just build the packages with `makepkg` and install them using `pacman -U`.

The file `linux.diff` contains the differences between the vanilla ABS kernel build files and the files in this repository. It may be helpful if you are patching a different kernel (`linux-lts` anyone?) or a different version of kernel.

The patch files could be useful for other Linux distributions as well.

**Remember to set regulatory domain in software (`hostapd`, `iw reg set`, etc.) in order to comply with local regulations!**



[1]: https://wireless.wiki.kernel.org/en/users/Drivers/ath

[2]: https://wireless.wiki.kernel.org/en/developers/Regulatory/processing_rules

[3]: https://dev.openwrt.org/browser/trunk/package/kernel/mac80211/patches

[4]: http://linuxwireless.org/en/users/Drivers/ath10k/
