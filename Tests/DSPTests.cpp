#include "TestSupport.h"
#include "dsp/BrickMawLimiter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
brickmaw::dsp::LimiterParameters brutalDefaults()
{
    brickmaw::dsp::LimiterParameters params;
    params.ceilingDb = -6.0f;
    params.lookaheadMs = 2.0f;
    params.releaseMs = 80.0f;
    params.adaptive = 0.7f;
    params.preDriveDb = 18.0f;
    params.clipShape = 0.35f;
    params.oversampleDetect = 1.0f;
    params.mawBite = 0.6f;
    params.recovery = 0.55f;
    params.link = 1.0f;
    params.mix = 1.0f;
    params.outputDb = 0.0f;
    return params;
}

float maxAbs(const std::vector<float>& samples)
{
    float peak = 0.0f;
    for (float sample : samples)
        peak = std::max(peak, std::abs(sample));
    return peak;
}

void processMono(brickmaw::dsp::BrickMawLimiter& limiter, std::vector<float>& samples)
{
    float* channels[] { samples.data() };
    limiter.processBlock(channels, 1, static_cast<int>(samples.size()));
}
} // namespace

int main()
{
    return test_support::run("brickmaw_dsp_tests", [] {
        constexpr double sampleRate = 48000.0;
        const auto ceiling = brickmaw::dsp::BrickMawLimiter::dbToGain(-6.0f);

        brickmaw::dsp::BrickMawLimiter limiter;
        limiter.prepare(sampleRate, 512, 2);
        limiter.setParameters(brutalDefaults());
        test_support::check(limiter.preparedChannels() == 2, "stereo prepare");
        test_support::check(limiter.latencySamples() == 96, "lookahead reports exact latency samples");

        std::vector<float> loud(1024, 4.0f);
        processMono(limiter, loud);
        test_support::check(maxAbs(loud) <= ceiling + 0.00001f, "steady sample peak never exceeds ceiling tolerance");
        for (float sample : loud)
            test_support::check(std::isfinite(sample), "steady output finite");

        limiter.reset();
        std::vector<float> impulse(256, 0.0f);
        impulse[0] = 2.0f;
        processMono(limiter, impulse);
        int firstAudible = -1;
        for (int i = 0; i < static_cast<int>(impulse.size()); ++i)
            if (std::abs(impulse[static_cast<std::size_t>(i)]) > 0.0001f)
            {
                firstAudible = i;
                break;
            }
        test_support::check(firstAudible == limiter.latencySamples(), "transient emerges after exact lookahead latency");
        test_support::check(maxAbs(impulse) <= ceiling + 0.00001f, "transient obeys ceiling");

        limiter.reset();
        std::vector<float> burst(320, 0.0f);
        std::fill(burst.begin(), burst.begin() + 128, 3.0f);
        processMono(limiter, burst);
        const float reducedGain = limiter.currentGain();
        std::vector<float> recovery(1600, 0.0f);
        processMono(limiter, recovery);
        test_support::check(reducedGain < 0.7f, "burst creates gain reduction");
        test_support::check(limiter.currentGain() > reducedGain, "release recovers after quiet material");

        auto slow = brutalDefaults();
        slow.adaptive = 0.0f;
        slow.recovery = 0.0f;
        brickmaw::dsp::BrickMawLimiter slowLimiter;
        slowLimiter.prepare(sampleRate, 512, 1);
        slowLimiter.setParameters(slow);
        std::vector<float> slowBurst(256, 3.0f);
        processMono(slowLimiter, slowBurst);
        std::vector<float> slowQuiet(300, 0.0f);
        processMono(slowLimiter, slowQuiet);

        auto fast = slow;
        fast.adaptive = 1.0f;
        fast.recovery = 1.0f;
        brickmaw::dsp::BrickMawLimiter fastLimiter;
        fastLimiter.prepare(sampleRate, 512, 1);
        fastLimiter.setParameters(fast);
        std::vector<float> fastBurst(256, 3.0f);
        processMono(fastLimiter, fastBurst);
        std::vector<float> fastQuiet(300, 0.0f);
        processMono(fastLimiter, fastQuiet);
        test_support::check(fastLimiter.currentGain() > slowLimiter.currentGain(), "adaptive recovery releases faster");

        auto clean = brutalDefaults();
        clean.preDriveDb = 0.0f;
        clean.clipShape = 0.0f;
        brickmaw::dsp::BrickMawLimiter cleanLimiter;
        cleanLimiter.prepare(sampleRate, 512, 1);
        cleanLimiter.setParameters(clean);
        std::vector<float> cleanTone(512);
        for (int i = 0; i < static_cast<int>(cleanTone.size()); ++i)
            cleanTone[static_cast<std::size_t>(i)] = 0.2f * std::sin(0.07f * static_cast<float>(i));
        auto drivenTone = cleanTone;
        processMono(cleanLimiter, cleanTone);

        auto driven = clean;
        driven.preDriveDb = 30.0f;
        driven.clipShape = 1.0f;
        brickmaw::dsp::BrickMawLimiter drivenLimiter;
        drivenLimiter.prepare(sampleRate, 512, 1);
        drivenLimiter.setParameters(driven);
        processMono(drivenLimiter, drivenTone);
        float difference = 0.0f;
        for (std::size_t i = 0; i < drivenTone.size(); ++i)
            difference += std::abs(drivenTone[i] - cleanTone[i]);
        test_support::check(difference > 0.1f, "predrive and clip shape change tone");
        test_support::check(maxAbs(drivenTone) <= ceiling + 0.00001f, "predrive cannot bypass final ceiling");

        const float intersampleRisk[] { 0.0f, 0.92f, -0.92f, 0.92f, -0.92f, 0.0f };
        const auto samplePeak = brickmaw::dsp::BrickMawLimiter::estimateWindowPeakSample(intersampleRisk, 6);
        const auto oversampledPeak = brickmaw::dsp::BrickMawLimiter::estimateWindowPeak4x(intersampleRisk, 6);
        test_support::check(oversampledPeak > samplePeak, "4x detector is more conservative on constructed intersample-risk waveform");

        limiter.prepare(sampleRate, 128, 1);
        limiter.setParameters(brutalDefaults());
        std::vector<float> mono(128, 0.1f);
        mono[5] = std::numeric_limits<float>::infinity();
        mono[6] = std::numeric_limits<float>::quiet_NaN();
        processMono(limiter, mono);
        test_support::check(maxAbs(mono) <= ceiling + 0.00001f, "mono non-finite input sanitized and ceiling held");
        for (float sample : mono)
            test_support::check(std::isfinite(sample), "mono output finite");

        brickmaw::dsp::BrickMawLimiter first;
        brickmaw::dsp::BrickMawLimiter second;
        first.prepare(sampleRate, 64, 1);
        second.prepare(sampleRate, 64, 1);
        first.setParameters(brutalDefaults());
        second.setParameters(brutalDefaults());
        std::vector<float> a(256);
        for (int i = 0; i < static_cast<int>(a.size()); ++i)
            a[static_cast<std::size_t>(i)] = (i % 17 == 0 ? 1.7f : -0.23f);
        auto b = a;
        processMono(first, a);
        processMono(second, b);
        float hashDelta = 0.0f;
        for (std::size_t i = 0; i < a.size(); ++i)
            hashDelta += std::abs(a[i] - b[i]);
        test_support::check(hashDelta == 0.0f, "deterministic output after reset-equivalent prepare");
    });
}
