#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace brickmaw::dsp
{
struct LimiterParameters
{
    float ceilingDb { -3.0f };
    float lookaheadMs { 3.0f };
    float releaseMs { 85.0f };
    float adaptive { 0.55f };
    float preDriveDb { 9.0f };
    float clipShape { 0.55f };
    float oversampleDetect { 1.0f };
    float mawBite { 0.45f };
    float recovery { 0.5f };
    float link { 1.0f };
    float mix { 1.0f };
    float outputDb { 0.0f };
};

class BrickMawLimiter
{
public:
    static constexpr float minCeilingDb = -24.0f;
    static constexpr float maxCeilingDb = 0.0f;
    static constexpr float minLookaheadMs = 0.5f;
    static constexpr float maxLookaheadMs = 10.0f;
    static constexpr int oversampleFactor = 4;

    void prepare(double sampleRate, int maxBlockSize, int channels) noexcept;
    void reset() noexcept;
    void setParameters(const LimiterParameters& parameters) noexcept;
    void processBlock(float* const* channelData, int channels, int samples) noexcept;

    float processMonoSample(float input) noexcept;
    int preparedChannels() const noexcept { return channels_; }
    int latencySamples() const noexcept { return maxLookaheadSamples_; }
    int maxPreparedLookaheadSamples() const noexcept { return maxLookaheadSamples_; }
    float lastGainReductionDb() const noexcept { return lastGainReductionDb_; }
    float currentGain() const noexcept { return gains_[0]; }

    static float sanitize(float value) noexcept;
    static float clamp(float value, float lo, float hi) noexcept;
    static float dbToGain(float db) noexcept;
    static float gainToDb(float gain) noexcept;
    static float estimateWindowPeak4x(const float* values, int count) noexcept;
    static float estimateWindowPeakSample(const float* values, int count) noexcept;

private:
    static constexpr int maxChannels = 2;
    static constexpr float floorGain = 0.000001f;

    float shapeSample(float input) const noexcept;
    float detectPeakForChannel(int channel) const noexcept;
    float readDelayedWetSample(int channel) const noexcept;
    float readDelayedDrySample(int channel) const noexcept;
    void writeSamples(int channel, float dry, float wet) noexcept;
    void advanceWriteIndex() noexcept;
    void updateDerivedTargets() noexcept;

    double sampleRate_ { 44100.0 };
    int channels_ { 0 };
    int ringSize_ { 1 };
    int writeIndex_ { 0 };
    int maxLookaheadSamples_ { 1 };
    int lookaheadSamples_ { 1 };
    int samplesSinceReset_ { 0 };

    LimiterParameters params_;
    float ceilingGain_ { 0.70794576f };
    float preDriveGain_ { 2.8183829f };
    float outputGain_ { 1.0f };
    float releaseCoeff_ { 0.00025f };
    float lastGainReductionDb_ { 0.0f };
    std::array<float, maxChannels> gains_ { 1.0f, 1.0f };
    std::array<std::vector<float>, maxChannels> wetDelayLines_;
    std::array<std::vector<float>, maxChannels> dryDelayLines_;
};
} // namespace brickmaw::dsp
