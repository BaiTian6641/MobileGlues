# Launcher plugin patches

`fsr3-settings.patch` restores the app-only FSR3 changes from maintained plugin
revision `f84ba048d76b6ded7657f44f6126c39bd41d33e7` on top of upstream
`f5105e4ddd1752cccdcc6421c402960d40ec3d9d`. It includes the mode switch, five
quality presets, shared JSON codec, Material and Miuix controls, and English and
Chinese resources. The reviewed patch corrects the misplaced cache-slider
comments and explains bridge-dependent scene scaling, FSR3 precedence over
FSR1, and the game restart requirement.

`prepare_android_plugin.py` checks the clean upstream revision, applies this
patch with `git apply`, and copies the focused codec regression tests into the
plugin test suite. It records both patch and test hashes in
`core-provenance.json`. The maintained plugin checkout is not a build input.

The Android workflow runs the plugin unit tests and lint, then checks both
debug and release APKs for the FSR3 UI resources and DEX config keys as well as
their native ABI/alignment contracts. These artifact checks reject the earlier
FSR1-only APKs. Resource presence is a packaging gate; actual navigation and
settings persistence still require app runtime verification.
