#include "dsp/BrickMawLimiter.h"

#include <algorithm>
#include <cmath>

namespace brickmaw::dsp
{
void BrickMawLimiter::prepare(double sampleRate, int, int channels) noexcept
{
    sampleRate_ = std::isfinite(sampleRate) && sampleRate > 1000.0 ? sampleRate : 44100.0;
    channels_ = clamp(static_cast<float>(channels), 0.0f, static_cast<float>(maxChannels)) == 0.0f
        ? 0
        : static_cast<int>(clamp(static_cast<float>(channels), 1.0f, static_cast<float>(maxChannels)));
    maxLookaheadSamples_ = std::max(1, static_cast<int>(std::ceil(sampleRate_ * maxLookaheadMs * 0.001)));
    ringSize_ = maxLookaheadSamples_ + 8;
    for (auto& line : delayLines_)
        line.assign(static_cast<std::size_t>(ringSize_), 0.0f);
    updateDerivedTargets();
    reset();
}

void BrickMawLimiter::reset() noexcept
{
    for (auto& line : delayLines_)
        std::fill(line.begin(), line.end(), 0.0f);
    gains_[0] = 1.0f;
    gains_[1] = 1.0f;
    writeIndex_ = 0;
    samplesSinceReset_ = 0;
    lastGainReductionDb_ = 0.0f;
}

void BrickMawLimiter::setParameters(const LimiterParameters& parameters) noexcept
{
    params_.ceilingDb = clamp(sanitize(parameters.ceilingDb), minCeilingDb, maxCeilingDb);
    params_.lookaheadMs = clamp(sanitize(parameters.lookaheadMs), minLookaheadMs, maxLookaheadMs);
    params_.releaseMs = clamp(sanitize(parameters.releaseMs), 10.0f, 500.0f);
    params_.adaptive = clamp(sanitize(parameters.adaptive), 0.0f, 1.0f);
    params_.preDriveDb = clamp(sanitize(parameters.preDriveDb), 0.0f, 36.0f);
    params_.clipShape = clamp(sanitize(parameters.clipShape), 0.0f, 1.0f);
    params_.oversampleDetect = clamp(sanitize(parameters.oversampleDetect), 0.0f, 1.0f);
    params_.mawBite = clamp(sanitize(parameters.mawBite), 0.0f, 1.0f);
    params_.recovery = clamp(sanitize(parameters.recovery), 0.0f, 1.0f);
    params_.link = clamp(sanitize(parameters.link), 0.0f, 1.0f);
    params_.mix = clamp(sanitize(parameters.mix), 0.0f, 1.0f);
    params_.outputDb = clamp(sanitize(parameters.outputDb), -24.0f, 6.0f);
    updateDerivedTargets();
}

void BrickMawLimiter::processBlock(float* const* channelData, int channels, int samples) noexcept
{
    if (channelData == nullptr || channels <= 0 || samples <= 0 || ringSize_ <= 0)
        return;

    const int usedChannels = std::min({ channels, channels_, maxChannels });
    if (usedChannels <= 0)
        return;

    for (int sample = 0; sample < samples; ++sample)
    {
        std::array<float, maxChannels> dry {};
        for (int channel = 0; channel < usedChannels; ++channel)
        {
            dry[static_cast<std::size_t>(channel)] = sanitize(channelData[channel][sample]);
            writeSample(channel, shapeSample(dry[static_cast<std::size_t>(channel)]));
        }

        const int available = std::min(lookaheadSamples_ + 1, samplesSinceReset_ + 1);
        std::array<float, maxChannels> detected {};
        float linkedPeak = 0.0f;
        for (int channel = 0; channel < usedChannels; ++channel)
        {
            detected[static_cast<std::size_t>(channel)] = detectPeakForChannel(channel, available) * outputGain_;
            linkedPeak = std::max(linkedPeak, detected[static_cast<std::size_t>(channel)]);
        }

        float deepestGain = 1.0f;
        for (int channel = 0; channel < usedChannels; ++channel)
        {
            const float channelPeak = detected[static_cast<std::size_t>(channel)];
            const float blendedPeak = channelPeak + (linkedPeak - channelPeak) * params_.link;
            const float targetGain = blendedPeak > ceilingGain_ && blendedPeak > floorGain
                ? clamp(ceilingGain_ / blendedPeak, floorGain, 1.0f)
                : 1.0f;

            auto& gain = gains_[static_cast<std::size_t>(channel)];
            if (targetGain < gain)
            {
                gain = targetGain;
            }
            else
            {
                const float depth = clamp(1.0f - gain, 0.0f, 1.0f);
                const float adaptiveSpeed = 1.0f + params_.adaptive * depth * (1.0f + params_.mawBite * 5.0f);
                gain += (targetGain - gain) * clamp(releaseCoeff_ * adaptiveSpeed, 0.0f, 1.0f);
            }

            const float wet = readDelayedSample(channel) * gain * outputGain_;
            float output = dry[static_cast<std::size_t>(channel)] + (wet - dry[static_cast<std::size_t>(channel)]) * params_.mix;
            output = clamp(sanitize(output), -ceilingGain_, ceilingGain_);
            channelData[channel][sample] = output;
            deepestGain = std::min(deepestGain, gain);
        }

        lastGainReductionDb_ = -gainToDb(std::max(deepestGain, floorGain));
        advanceWriteIndex();
        ++samplesSinceReset_;
    }
}

float BrickMawLimiter::processMonoSample(float input) noexcept
{
    float* channels[] { &input };
    processBlock(channels, 1, 1);
    return input;
}

float BrickMawLimiter::sanitize(float value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
}

float BrickMawLimiter::clamp(float value, float lo, float hi) noexcept
{
    return value < lo ? lo : (value > hi ? hi : value);
}

float BrickMawLimiter::dbToGain(float db) noexcept
{
    return std::pow(10.0f, sanitize(db) / 20.0f);
}

float BrickMawLimiter::gainToDb(float gain) noexcept
{
    return 20.0f * std::log10(std::max(sanitize(gain), floorGain));
}

float BrickMawLimiter::estimateWindowPeak4x(const float* values, int count) noexcept
{
    if (values == nullptr || count <= 0)
        return 0.0f;

    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float xm1 = values[i > 0 ? i - 1 : i];
        const float x0 = values[i];
        const float x1 = values[i + 1 < count ? i + 1 : i];
        const float x2 = values[i + 2 < count ? i + 2 : (i + 1 < count ? i + 1 : i)];
        peak = std::max(peak, std::abs(x0));
        if (i + 1 >= count)
            continue;

        for (int phase = 1; phase < oversampleFactor; ++phase)
        {
            const float t = static_cast<float>(phase) / static_cast<float>(oversampleFactor);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float interpolated = 0.5f * ((2.0f * x0)
                + (-xm1 + x1) * t
                + (2.0f * xm1 - 5.0f * x0 + 4.0f * x1 - x2) * t2
                + (-xm1 + 3.0f * x0 - 3.0f * x1 + x2) * t3);
            const float transitionRisk = x0 * x1 < 0.0f ? std::abs(x0 - x1) * 0.08f : 0.0f;
            const float edgePeak = std::max(std::abs(x0), std::abs(x1)) + transitionRisk;
            peak = std::max(peak, std::max(std::abs(sanitize(interpolated)), edgePeak));
        }
    }
    return peak;
}

float BrickMawLimiter::estimateWindowPeakSample(const float* values, int count) noexcept
{
    if (values == nullptr || count <= 0)
        return 0.0f;

    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
        peak = std::max(peak, std::abs(sanitize(values[i])));
    return peak;
}

float BrickMawLimiter::shapeSample(float input) const noexcept
{
    const float driven = clamp(sanitize(input) * preDriveGain_, -64.0f, 64.0f);
    const float hardLimit = 1.0f + params_.mawBite * 0.5f;
    const float hard = clamp(driven, -hardLimit, hardLimit);
    const float soft = std::tanh(driven * (0.75f + params_.mawBite * 2.25f)) * hardLimit;
    return sanitize(hard + (soft - hard) * params_.clipShape);
}

float BrickMawLimiter::detectPeakForChannel(int channel, int availableSamples) const noexcept
{
    if (channel < 0 || channel >= maxChannels || availableSamples <= 0 || ringSize_ <= 0)
        return 0.0f;

    const auto& line = delayLines_[static_cast<std::size_t>(channel)];
    const int count = std::min(availableSamples, lookaheadSamples_ + 1);
    float peak = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const int index = (writeIndex_ - count + 1 + i + ringSize_) % ringSize_;
        const float x0 = sanitize(line[static_cast<std::size_t>(index)]);
        peak = std::max(peak, std::abs(x0));
        if (params_.oversampleDetect < 0.5f)
            continue;

        const int prevIndex = (index - 1 + ringSize_) % ringSize_;
        const int nextIndex = (index + 1) % ringSize_;
        const int next2Index = (index + 2) % ringSize_;
        const float xm1 = sanitize(line[static_cast<std::size_t>(prevIndex)]);
        const float x1 = sanitize(line[static_cast<std::size_t>(nextIndex)]);
        const float x2 = sanitize(line[static_cast<std::size_t>(next2Index)]);
        for (int phase = 1; phase < oversampleFactor; ++phase)
        {
            const float t = static_cast<float>(phase) / static_cast<float>(oversampleFactor);
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float interpolated = 0.5f * ((2.0f * x0)
                + (-xm1 + x1) * t
                + (2.0f * xm1 - 5.0f * x0 + 4.0f * x1 - x2) * t2
                + (-xm1 + 3.0f * x0 - 3.0f * x1 + x2) * t3);
            const float transitionRisk = x0 * x1 < 0.0f ? std::abs(x0 - x1) * 0.08f : 0.0f;
            const float edgePeak = std::max(std::abs(x0), std::abs(x1)) + transitionRisk;
            peak = std::max(peak, std::max(std::abs(sanitize(interpolated)), edgePeak));
        }
    }
    return peak;
}

float BrickMawLimiter::readDelayedSample(int channel) const noexcept
{
    const int readIndex = (writeIndex_ - lookaheadSamples_ + ringSize_) % ringSize_;
    return delayLines_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(readIndex)];
}

void BrickMawLimiter::writeSample(int channel, float value) noexcept
{
    delayLines_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(writeIndex_)] = sanitize(value);
}

void BrickMawLimiter::advanceWriteIndex() noexcept
{
    writeIndex_ = (writeIndex_ + 1) % ringSize_;
}

void BrickMawLimiter::updateDerivedTargets() noexcept
{
    ceilingGain_ = dbToGain(params_.ceilingDb);
    preDriveGain_ = dbToGain(params_.preDriveDb);
    outputGain_ = dbToGain(params_.outputDb);
    lookaheadSamples_ = std::max(1, std::min(maxLookaheadSamples_,
        static_cast<int>(std::lround(sampleRate_ * params_.lookaheadMs * 0.001))));

    const float releaseMs = params_.releaseMs * (1.6f - params_.recovery);
    releaseCoeff_ = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate_ * releaseMs * 0.001));
    releaseCoeff_ = clamp(sanitize(releaseCoeff_), 0.0f, 1.0f);
}
} // namespace brickmaw::dsp
