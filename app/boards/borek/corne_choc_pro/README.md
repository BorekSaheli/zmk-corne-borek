# Borek's Corne Choc Pro

This repository is a fork of [ZMK](https://github.com/zmkfirmware/zmk) that carries the firmware for one keyboard: a wireless split Corne with choc switches, two extra inner keys and two encoder positions per half, RGB underglow, and a 160x68 memory LCD on each half.

- `main` is upstream ZMK, untouched.
- `borek` is `main` plus a handful of commits that only add files. That keeps rebasing onto a newer ZMK free of conflicts.

## Where things are

| What                                               | Where                                           |
| -------------------------------------------------- | ----------------------------------------------- |
| Keymap                                             | `corne_choc_pro.keymap` (this folder)           |
| Keyboard name, radio tuning                        | `Kconfig.defconfig` (this folder)               |
| Sleep timeout, RGB, other ZMK settings (per half)  | `corne_choc_pro_{left,right}_defconfig`         |
| Pins, matrix, encoders, LEDs, battery gauge        | `corne_choc_pro.dtsi`, `corne_choc_pro_*.dts`   |
| Display: status screen, rotating art, layer name   | `app/boards/shields/borek_view/`                |
| Firmware build                                     | `.github/workflows/borek-firmware.yml`          |

## Getting firmware

Every push to `borek` that touches `app/` builds the firmware on GitHub Actions. Open the latest "Borek's Corne firmware" run and download the `firmware` artifact. It contains:

- `corne_left.uf2` and `corne_right.uf2`, the normal firmware
- `settings_reset_left.uf2` and `settings_reset_right.uf2`, which wipe the stored settings of a half

To flash a half, double-tap its reset button so it shows up as a USB drive, then copy the matching `.uf2` onto it. Flash both halves whenever the display code changes, because the halves talk to each other about what to show.

Two things to know after flashing:

- Keymap changes saved from ZMK Studio are stored on the keyboard and take priority over the keymap in the firmware. If a new keymap does not seem to apply, use "Restore Stock Settings" in ZMK Studio.
- If the halves stop finding each other, or a host refuses to reconnect no matter what, flash the settings reset image to both halves, then the normal firmware again, and pair the hosts again.

## Keymap

Four layers plus five spare ones for ZMK Studio:

- `QWERTY`: base layer with home row mods on S D F and J K L. Print Screen is left of A, Win is left of Z.
- `NUMBER`: hold the left outer thumb.
- `SYMBOL`: hold the right outer thumb.
- `SYSTEM`: hold both outer thumbs. Bluetooth profiles 1 to 5 are on the left home row, clear-pairing is on Z, reset and bootloader are on C and V (left half) and on comma and M (right half).

Everything that can drop the connection is on `SYSTEM` only, and that layer is only active while both thumbs are held.

The home row mod timings are at the top of the keymap, with a note on what each one does.

## Display

Each half shows, from right to left: battery and connection, a picture, and the active layer.

The picture changes every hour (`CONFIG_BOREK_VIEW_ART_ROTATE_MINUTES`) and the two halves always show different ones. The left half decides and tells the right half, along with the layer name, over a small private Bluetooth service between the halves.

To change the pictures, put PNG files of any size in `app/boards/shields/borek_view/images/` and run:

```sh
uv run app/boards/shields/borek_view/images/convert.py
```

Commit the PNGs together with the regenerated files in `widgets/art/`.

## Updating to a newer ZMK

```sh
git fetch upstream
git switch main
git merge --ff-only upstream/main
git push origin main
git switch borek
git rebase main
git push --force-with-lease origin borek
```

The push to `borek` builds the firmware against the new ZMK. If that build fails, upstream changed something this keyboard relies on; the display code in `borek_view` is the most likely place, since it uses LVGL and Bluetooth APIs directly.

## Building locally

With a [ZMK development environment](https://zmk.dev/docs/development/local-toolchain/setup):

```sh
west build -s app -d build/left -b corne_choc_pro_left -S studio-rpc-usb-uart -- -DSHIELD=borek_view
west build -s app -d build/right -b corne_choc_pro_right -- -DSHIELD=borek_view
```
