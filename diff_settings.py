# Settings for asm-differ (tools/toolchain/asm-differ/diff.py), used by `make diff`.
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "tools"))
import versions  # noqa: E402


def apply(config, args):
    version = os.environ.get("VERSION", versions.DEFAULT_VERSION)
    exe = versions.get(version).exe
    config["arch"] = "mipsel"
    config["baseimg"] = f"disc/{exe}"
    config["myimg"] = f"build/{version}/{exe}"
    config["mapfile"] = f"build/{version}/main.map"
    config["build_dir"] = f"build/{version}/"
    config["expected_dir"] = "expected/"
    config["source_directories"] = ["src", "include"]
    config["objdump_executable"] = "mipsel-linux-gnu-objdump"
    config["makeflags"] = [f"VERSION={version}"]
