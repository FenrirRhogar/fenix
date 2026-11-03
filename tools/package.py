#!/usr/bin/env python3
"""Packages a Release build into dist/Fenix-<version>-<platform>.zip, ready to upload.

    tools/package.py <linux|windows|macos> [build-dir] [--juce PATH] [--signed]

--juce overrides the JUCE path recorded in the build's CMakeCache (needed when the build ran
in a container). --signed marks macOS bundles as Developer ID signed and notarised, which
drops the Gatekeeper workaround from the README.
"""

import argparse
import os
import re
import shutil
import stat
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGINS = ["Bus", "Drive", "Shelf", "HFComp", "Width", "Iron"]
FORMATS = {"linux": ["VST3", "LV2"], "windows": ["VST3"], "macos": ["VST3", "AU"]}
BUNDLE_SUFFIXES = {".vst3", ".component", ".lv2"}
LABELS = {"linux": "Linux-x64", "windows": "Windows-x64", "macos": "macOS"}

MAC_QUARANTINE = """
2. These builds are not notarised by Apple, so macOS blocks them at first. In Terminal, run:

     sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Fenix*
     sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Fenix*
"""


def version():
    text = (ROOT / "CMakeLists.txt").read_text()
    return re.search(r"project\(Fenix VERSION ([0-9.]+)", text).group(1)


def juce_dir(build, override):
    if override:
        return Path(override)
    cache = (build / "CMakeCache.txt").read_text()
    return Path(re.search(r"^JUCE_DIR:PATH=(.*)$", cache, re.M).group(1))


def render_readme(platform, values):
    """Keeps [platform] ... [/platform] blocks for this platform and drops the others."""
    text = (ROOT / "packaging" / "README.txt").read_text()
    text = re.sub(r"\[(\w+)\]\n(.*?)\[/\1\]\n",
                  lambda m: m.group(2) if m.group(1) == platform else "", text, flags=re.S)
    for key, value in values.items():
        text = text.replace(f"@{key}@", value)
    return text


def minimum_glibc(stage):
    """The newest glibc symbol any binary needs is the oldest glibc the package runs on."""
    versions = set()
    for so in stage.rglob("*.so"):
        out = subprocess.run(["objdump", "-T", str(so)], capture_output=True, text=True, check=True).stdout
        versions.update(re.findall(r"GLIBC_([0-9.]+)", out))
    return max(versions, key=lambda v: tuple(int(p) for p in v.split(".")))


def zip_tree(stage, archive):
    """Zips `stage` keeping executable bits, so install.sh still runs after unzipping."""
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(stage.rglob("*")):
            name = str(path.relative_to(stage.parent)).replace(os.sep, "/")
            if path.is_dir():
                continue
            info = zipfile.ZipInfo.from_file(path, name)
            info.external_attr = (stat.S_IMODE(path.stat().st_mode) | stat.S_IFREG) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                zf.writestr(info, f.read())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=FORMATS)
    parser.add_argument("build", nargs="?", default=str(ROOT / "build"))
    parser.add_argument("--juce")
    parser.add_argument("--signed", action="store_true")
    args = parser.parse_args()

    build = Path(args.build)
    ver = version()
    name = f"Fenix-{ver}-{LABELS[args.platform]}"
    stage = ROOT / "dist" / name
    archive = stage.parent / f"{name}.zip"
    shutil.rmtree(stage, ignore_errors=True)
    archive.unlink(missing_ok=True)

    for fmt in FORMATS[args.platform]:
        (stage / fmt).mkdir(parents=True)
        for plugin in PLUGINS:
            # Only the bundles: on Windows the linker leaves .exp/.lib files next to them.
            for bundle in (build / f"{plugin}_artefacts" / "Release" / fmt).iterdir():
                if bundle.is_dir() and bundle.suffix in BUNDLE_SUFFIXES:
                    shutil.copytree(bundle, stage / fmt / bundle.name, symlinks=True)

    values = {"VERSION": ver, "GLIBC": "", "MAC_QUARANTINE": "" if args.signed else MAC_QUARANTINE}
    if args.platform == "linux":
        for so in stage.rglob("*.so"):
            subprocess.run(["strip", "--strip-unneeded", str(so)], check=True)
        values["GLIBC"] = minimum_glibc(stage)
        for script in ("install.sh", "uninstall.sh"):
            shutil.copy(ROOT / "packaging" / script, stage / script)
            (stage / script).chmod(0o755)

    (stage / "README.txt").write_text(render_readme(args.platform, values))
    shutil.copy(ROOT / "LICENSE", stage / "LICENSE.txt")
    shutil.copy(ROOT / "packaging" / "CHANGELOG.txt", stage / "CHANGELOG.txt")

    sdk = juce_dir(build, args.juce) / "modules" / "juce_audio_processors_headless" / "format_types"
    notices = ["FENIX COLOUR SUITE - THIRD-PARTY NOTICES", "",
               "Made with JUCE (https://juce.com), used under the GNU AGPLv3.", "",
               "=== VST 3 SDK (Steinberg Media Technologies GmbH) ===",
               (sdk / "VST3_SDK" / "LICENSE.txt").read_text()]
    if args.platform == "linux":
        notices += ["=== LV2 (lv2plug.in) ===", (sdk / "LV2_SDK" / "lv2" / "COPYING").read_text()]
    (stage / "THIRD_PARTY_NOTICES.txt").write_text("\n".join(notices))

    if args.platform == "macos":
        # ditto keeps code signatures and extended attributes intact.
        subprocess.run(["ditto", "-c", "-k", "--keepParent", str(stage), str(archive)], check=True)
    else:
        zip_tree(stage, archive)

    note = f" (needs glibc {values['GLIBC']}+)" if values["GLIBC"] else ""
    print(f"{archive}{note}")


if __name__ == "__main__":
    main()
