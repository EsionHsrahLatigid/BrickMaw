# Research

BrickMaw is documented as a fixed-latency destructive limiter with bounded predrive, a deterministic detector estimate, adaptive recovery, stereo linking, and a final digital sample ceiling guard.

## Sources

- Giannoulis, Massberg, and Reiss, "Digital Dynamic Range Compressor Design - A Tutorial and Analysis", JAES 2012: used for limiter-like dynamics blocks, detector/envelope behavior, release behavior, and feedforward gain reduction vocabulary.
- JUCE `dsp::Oversampling` documentation: used as framework evidence that oversampled peak detection is an established JUCE-era implementation path. BrickMaw's core uses a JUCE-independent deterministic 4x detector estimate so DSP tests can run without loading JUCE.
- ITU-R BS.1770-5: used only as vocabulary and verification context for true-peak risk. BrickMaw does not implement the full BS.1770 measurement chain and does not claim certified true-peak or loudness compliance.

## Design Interpretation

The product is a destructive lookahead limiter. The limiter may be clipped and forceful, but the output contract is narrow and testable: finite samples leaving the processor do not exceed the configured digital sample ceiling beyond tiny floating tolerance.

## Algorithm

1. Sanitize non-finite input to zero.
2. Apply bounded predrive.
3. Apply a creative hard/rounded clip blend controlled by Clip Shape and Maw Bite.
4. Store raw dry and clipped wet samples in fixed delay rings allocated during `prepare`.
5. Read both dry and wet output from the fixed maximum-latency delay point.
6. Detect peaks forward from that delayed output point over the current Lookahead preview window. With `oversample_detect` enabled, the detector evaluates Catmull-Rom interpolated quarter-sample phases as a conservative 4x peak estimate.
7. Apply immediate downward gain and bounded release recovery. Adaptive and Recovery controls alter the release coefficient without unbounded loops.
8. Apply stereo linking by blending each channel detector with the linked peak.
9. Blend delayed dry and delayed limited paths, then apply the final sample ceiling guard.

## Limits

- The four-times detector is a deterministic oversampled estimate, not a standards-certified true-peak meter.
- Latency is fixed to the maximum lookahead allocation, `ceil(sampleRate * 10 ms / 1000)`, when the processor is prepared. Lookahead automation changes detector preview only, not host latency or output alignment.
- The sample ceiling is a digital amplitude guard, not an SPL or hearing-safety guarantee.
