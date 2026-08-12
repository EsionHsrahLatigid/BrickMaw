# Design

BrickMaw uses the shared EHL JUCE design module: 4 px base spacing with 8 px major spacing, four-level palette `#050505`, `#2A2A2A`, `#8A8A86`, `#F2F2F0`, no external images, and no external fonts. The editor default size is 640 x 360 and the minimum is 512 x 320. `GenericAudioProcessorEditor` is banned.

Every exposed parameter is represented by a focusable JUCE slider with an APVTS attachment, accessible name, component ID, title, description, and tooltip. The controls stay in a fixed two-column by six-row grid. The paint layer is intentionally minimal: product name begins at `y=8`, compact function label at `y=32`, and one 1 px divider at `y=56`; controls start at absolute `y=64`. Do not add a full-canvas grid, tagline, package ID, decorative motif, fake visualizer, fake meter, panel frame, outer border, or parameter-driven atmospheric drawing. DSP behavior, parameter IDs, bundle identity, accessibility, and host automation identity are unchanged.

The audio callback owns no file, network, logging, lock, or heap allocation work in steady state. `BrickMawLimiter::prepare` allocates fixed raw dry and clipped wet delay rings; `processBlock` uses bounded loops capped by channel count and the prepared maximum lookahead window. Non-finite samples are sanitized, dry and wet paths are aligned to the fixed reported latency, and the final output is clamped to the configured digital sample ceiling.

No limiter gain-reduction display is drawn in this simplification pass because the old paint path was not a validated live meter.
