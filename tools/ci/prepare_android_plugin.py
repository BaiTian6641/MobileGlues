#!/usr/bin/env python3
"""Package this core in a disposable, pinned upstream launcher plugin checkout.

The reviewed FSR3 settings patch is applied to the pinned upstream source.
The maintained local plugin checkout is deliberately never used or modified.
Build each native ABI first, then pass --library ABI=/absolute/libmobileglues.so.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

PLUGIN_COMMIT = "f5105e4ddd1752cccdcc6421c402960d40ec3d9d"
GRADLE_SHA256 = "20f1b1176237254a6fc204d8434196fa11a4cfb387567519c61556e8710aed78"
CI_DIRECTORY = Path(__file__).resolve().parent


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Pinned plugin content changed: expected exactly one {old!r}")
    return text.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plugin", type=Path)
    parser.add_argument("--library", action="append", required=True)
    args = parser.parse_args()
    plugin = args.plugin.resolve()
    commit = subprocess.check_output(
        ["git", "-C", str(plugin), "rev-parse", "HEAD"], text=True
    ).strip()
    if commit != PLUGIN_COMMIT:
        parser.error(f"Expected plugin commit {PLUGIN_COMMIT}, got {commit}")
    status = subprocess.check_output(
        ["git", "-C", str(plugin), "status", "--porcelain"], text=True
    )
    if status:
        parser.error("Use a clean disposable plugin checkout; refusing to overwrite local work")
    libraries = {}
    for item in args.library:
        abi, filename = item.split("=", 1)
        if abi not in ("arm64-v8a", "x86_64") or abi in libraries:
            parser.error(f"Unsupported or duplicate ABI: {abi}")
        library = Path(filename).resolve(strict=True)
        if library.read_bytes()[:4] != b"\x7fELF":
            parser.error(f"Not an ELF library: {library}")
        libraries[abi] = library
    # Upstream does not yet contain our FSR3 switch, quality selector or codec.
    # Keep these app changes in the same reviewed source tree as the core so
    # local and Actions APKs cannot silently fall back to the older FSR1-only UI.
    patch = CI_DIRECTORY / "patches/fsr3-settings.patch"
    subprocess.run(["git", "-C", str(plugin), "apply", "--check", str(patch)], check=True)
    subprocess.run(["git", "-C", str(plugin), "apply", str(patch)], check=True)
    regression_test = CI_DIRECTORY / "plugin-tests/Fsr3ConfigTest.kt"
    shutil.copyfile(regression_test, plugin / "app/src/test/java/com/fcl/plugin/mobileglues/settings/Fsr3ConfigTest.kt")
    app_file = plugin / "app/build.gradle.kts"
    app = app_file.read_text()
    app = replace_once(app, 'ndkVersion = "27.3.13750724"', 'ndkVersion = "28.2.13676358"')
    app = replace_once(app, "versionCode = 2000", "versionCode = 2006")
    app = replace_once(app, 'versionName = "2.0.0"', 'versionName = "2.0.6-dev"')
    abi_list = ", ".join(json.dumps(abi) for abi in sorted(libraries))
    app = replace_once(app, "        minSdk = 26", f"        minSdk = 26\n        ndk {{ abiFilters += listOf({abi_list}) }}")
    # CI produces a debug-signed qualification APK and an unsigned release APK.
    # Release signing belongs to a separate trusted release process.
    app = replace_once(app, '            signingConfig = signingConfigs.getByName("release")\n', "")
    app_file.write_text(app)
    root_build = plugin / "build.gradle.kts"
    root_build.write_text('''// R8 8.13.19 is the minimum for this plugin's Kotlin 2.3 metadata.
buildscript {
    repositories { google(); mavenCentral() }
    dependencies { classpath("com.android.tools:r8:8.13.19") }
}
''' + root_build.read_text())
    # Upstream changed the decoder to mirror native's missing/invalid ANGLE
    # fallback but left three assertions on the old UI defaults. Pin that
    # documented behavior; do not weaken or skip the tests.
    test_file = plugin / "app/src/test/java/com/fcl/plugin/mobileglues/settings/MGConfigCodecTest.kt"
    test = test_file.read_text()
    test = replace_once(test, '`an empty object decodes to the defaults`',
                        '`an empty object uses the native missing-key fallback`')
    test = replace_once(test, 'assertEquals(MGConfig.Default, MGConfigCodec.decode(parse("{}")))',
                        'assertEquals(MGConfig.Default.copy(angle = AngleConfig.DisableIfPossible), '
                        'MGConfigCodec.decode(parse("{}")))')
    old_assertion = "assertEquals(MGConfig.Default.angle, decoded.angle)"
    if test.count(old_assertion) != 2:
        raise ValueError("Pinned plugin ANGLE fallback assertions changed")
    test = test.replace(old_assertion, "assertEquals(AngleConfig.DisableIfPossible, decoded.angle)")
    test_file.write_text(test)
    module = plugin / "MobileGlues"
    module.mkdir(exist_ok=True)
    (module / "build.gradle.kts").write_text('''plugins { id("com.android.library") }
android {
    namespace = "top.mobilegl.mobileglues"
    compileSdk = 36
    ndkVersion = "28.2.13676358"
    defaultConfig { minSdk = 26 }
}
''')
    source = module / "src/main"
    source.mkdir(parents=True, exist_ok=True)
    (source / "AndroidManifest.xml").write_text('<manifest xmlns:android="http://schemas.android.com/apk/res/android"/>\n')
    manifest = {
        "plugin_commit": commit,
        "fsr3_settings_patch_sha256": hashlib.sha256(patch.read_bytes()).hexdigest(),
        "fsr3_config_test_sha256": hashlib.sha256(regression_test.read_bytes()).hexdigest(),
        "ndk": "28.2.13676358",
        "libraries": {},
    }
    for abi, library in libraries.items():
        target = source / "jniLibs" / abi / "libmobileglues.so"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(library, target)
        manifest["libraries"][abi] = hashlib.sha256(target.read_bytes()).hexdigest()
    (plugin / "core-provenance.json").write_text(json.dumps(manifest, indent=2) + "\n")
    wrapper = plugin / "gradle/wrapper/gradle-wrapper.properties"
    wrapper.write_text(wrapper.read_text() + f"\ndistributionSha256Sum={GRADLE_SHA256}\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
