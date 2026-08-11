# Research

BrickMaw follows the DHN9 G001 limiter plan.

## Sources

- Giannoulis, Massberg, and Reiss, "Digital Dynamic Range Compressor Design - A Tutorial and Analysis", JAES 2012: used for limiter-like dynamics blocks, detector/envelope behavior, release behavior, and feedforward gain reduction vocabulary.
- JUCE `dsp::Oversampling` documentation: used as framework evidence that oversampled peak detection is an established JUCE-era implementation path. BrickMaw's core uses a JUCE-independent deterministic 4x detector estimate so DSP tests can run without loading JUCE.
- ITU-R BS.1770-5: used only as vocabulary and verification context for true-peak risk. BrickMaw does not implement the full BS.1770 measurement chain and does not claim certified true-peak or loudness compliance.

## Design Interpretation

The product is a destructive limiter for Digital Harsh Noise. The limiter may be ugly and clipped, but the output contract is narrow and testable: finite samples leaving the processor do not exceed the configured digital sample ceiling beyond tiny floating tolerance.

## Algorithm

1. Sanitize non-finite input to zero.
2. Apply bounded predrive.
3. Apply a creative hard/rounded clip blend controlled by Clip Shape and Maw Bite.
4. Store clipped samples in fixed delay rings allocated during `prepare`.
5. Detect peaks over the current lookahead window. With `oversample_detect` enabled, the detector evaluates Catmull-Rom interpolated quarter-sample phases as a conservative 4x peak estimate.
6. Apply immediate downward gain and bounded release recovery. Adaptive and Recovery controls alter the release coefficient without unbounded loops.
7. Apply stereo linking by blending each channel detector with the linked peak.
8. Blend dry and limited paths, then apply the final sample ceiling guard.

## Limits

- The four-times detector is a deterministic oversampled estimate, not a standards-certified true-peak meter.
- Latency is reported as `round(sampleRate * lookaheadMs / 1000)` when the processor is prepared or state is restored.
- The sample ceiling is a digital amplitude guard, not an SPL or hearing-safety guarantee.
