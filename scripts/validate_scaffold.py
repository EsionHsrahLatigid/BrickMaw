#!/usr/bin/env python3
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
required = [
    "CMakeLists.txt", "CMakePresets.json", "Source/PluginProcessor.cpp",
    "Source/PluginEditor.cpp", "Source/dsp/BrickMawLimiter.cpp",
    "Tests/DSPTests.cpp", "Tests/PluginTests.cpp", "Tests/EditorTests.cpp",
    ".github/workflows/ci.yml", ".github/workflows/release.yml",
]
missing = [p for p in required if not (root / p).exists()]
if missing:
    raise SystemExit("missing files: " + ", ".join(missing))
text = "\n".join((root / p).read_text() for p in required)
checks = {
    "juce commit": "91ad83ae34a81e0833b1a2b0866f54846370ae53" in text,
    "juce-ci commit": "926c5bb10335c29503f92869169759ec18d71449" in text,
    "EHL design module": "add_subdirectory(modules/juce-ehl-design-module)" in text
        and "EHL::JuceDesign" in text,
    "stage target": "ehl_stage_products" in text,
    "no generic editor": "GenericAudioProcessorEditor" in (root / "Tests/EditorTests.cpp").read_text()
        and "new juce::GenericAudioProcessorEditor" not in text,
    "stable artifacts": "artifacts/plugin-release/macos-arm64" in (root / "README.md").read_text()
        and "artifacts/plugin-release/windows-x64" in (root / "README.md").read_text(),
    "no asset font dirs": not (root / "Assets").exists() and not (root / "Fonts").exists(),
}
failed = [name for name, ok in checks.items() if not ok]
if failed:
    raise SystemExit("failed checks: " + ", ".join(failed))
if re.search(r"TemplatePlugin|MyPlugin", text):
    raise SystemExit("stale generic names found")
if re.search(r"placeholder|FoundationDSP", text, flags=re.IGNORECASE):
    raise SystemExit("placeholder DSP text found")
print("scaffold validation passed")
