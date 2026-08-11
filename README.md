# BrickMaw

BrickMaw is an EHL Digital Harsh Noise limiter. It combines fixed-latency lookahead limiting, a four-times oversampled peak detector estimate, destructive predrive clipping, adaptive release, stereo linking, wet/dry blend, and a final sample ceiling guard.

BrickMaw makes one narrow safety promise: finite output samples are clamped to the configured digital ceiling within floating-point tolerance. It is not a certified ITU-R BS.1770-5 true-peak or loudness meter, and it does not claim BS.1770 compliance.

## Identity

- Product: `BrickMaw`
- Repository slug: `brickmaw`
- Bundle ID: `jp.ehl.brickmaw`
- Manufacturer: `EsionHsrahLatigid`
- Manufacturer code: `EHL_`
- Plugin code: `BrMw`
- Reported latency: fixed maximum lookahead, `ceil(sampleRate * 10 ms / 1000)`. At 48 kHz this is 480 samples.

## Parameters

- `ceiling`: final digital sample ceiling, -24 to 0 dBFS
- `lookahead`: detector preview window, 0.5 to 10 ms. Output alignment and reported host latency remain fixed at the maximum lookahead.
- `release`: base limiter release, 10 to 500 ms
- `adaptive`: faster recovery during deep gain reduction
- `pre_drive`: gain into the creative clipper, 0 to 36 dB
- `clip_shape`: hard-to-rounded clip blend
- `oversample_detect`: four-times detector estimate on/off
- `maw_bite`: more aggressive clip and adaptive release behavior
- `recovery`: release speed character
- `link`: independent-to-linked stereo limiting
- `mix`: delayed dry/limited blend before the final ceiling guard
- `output`: post-limiter trim before the final ceiling guard

## Build

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure

cmake --preset plugin-release -DEHL_JUCE_SOURCE_DIR=/path/to/JUCE
cmake --build --preset plugin-release --target ehl_stage_products
ctest --preset plugin-release --output-on-failure
```

The project pins JUCE to `91ad83ae34a81e0833b1a2b0866f54846370ae53` when network FetchContent is used. Set `EHL_JUCE_SOURCE_DIR` for offline builds.

Stable artifacts:

```text
artifacts/plugin-release/macos-arm64/standalone/brickmaw_standalone_plugin.app
artifacts/plugin-release/macos-arm64/vst3/brickmaw_vst3_plugin.vst3
artifacts/plugin-release/macos-arm64/au/brickmaw_au_plugin.component
artifacts/plugin-release/macos-arm64/ARTIFACTS.txt

artifacts/plugin-release/windows-x64/standalone/brickmaw_standalone_plugin.exe
artifacts/plugin-release/windows-x64/vst3/brickmaw_vst3_plugin.vst3
artifacts/plugin-release/windows-x64/ARTIFACTS.txt
```

## Tests

Targets are fixed for CI and humans:

- `brickmaw_dsp_tests`
- `brickmaw_plugin_tests`
- `brickmaw_editor_tests`
- `ehl_stage_products`

The DSP tests cover sample ceiling, fixed output latency across lookahead and mix settings, release/adaptation, predrive/clip distinction, oversampled detector conservatism on a constructed waveform, reset determinism, silence, non-finite input, and mono/stereo processing.
