# Build Notes

Requirements: CMake 3.22+, C++17 compiler, and JUCE `91ad83ae34a81e0833b1a2b0866f54846370ae53`.

`engine-debug` builds only the JUCE-independent limiter DSP tests. `plugin-release` builds VST3 and Standalone on every platform and AU on Apple, then stages products through `ehl_stage_products`.

The reported latency is derived from the Lookahead parameter at prepare/state-restore time. At the default 3.0 ms lookahead this is 132 samples at 44.1 kHz and 144 samples at 48 kHz.
