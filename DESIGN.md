# Design

BrickMaw uses the shared DHN9 monochrome 8-bit system: 8 px grid, grayscale palette, procedural brick/jaw gain-reduction motif, no external images, and no external fonts. The editor default size is 960 x 544 and the minimum is 720 x 432. `GenericAudioProcessorEditor` is banned.

Every exposed parameter is represented by a focusable JUCE slider with an APVTS attachment, accessible name, component ID, title, description, and tooltip. The controls stay in a two-column grid on desktop widths and collapse to one column at narrower widths.

The audio callback owns no file, network, logging, lock, or heap allocation work in steady state. `BrickMawLimiter::prepare` allocates fixed raw dry and clipped wet delay rings; `processBlock` uses bounded loops capped by channel count and the prepared maximum lookahead window. Non-finite samples are sanitized, dry and wet paths are aligned to the fixed reported latency, and the final output is clamped to the configured digital sample ceiling.

The visual motif is intentionally not a meter with certified ballistics. It reflects recent gain reduction from the DSP and reinforces the brick/jaw ceiling identity.
