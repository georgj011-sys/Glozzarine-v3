#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

// Holds all built-in oscillator sources. Each source has several morph frames,
// and every frame is stored at several band-limited levels (mipmaps) so high
// notes do not alias. Everything is generated at load time via inverse FFT.
class WaveBank
{
public:
    static constexpr int tableSize    = 2048;
    static constexpr int numFrames    = 8;
    static constexpr int numLevels    = 8;    // 512, 256, ... 4 harmonics
    static constexpr int maxHarmonics = 512;
    static constexpr int numSources   = 16;

    WaveBank();

    static const juce::StringArray& getSourceNames();

    // Returned table has tableSize + 1 samples (last one wraps to the first).
    const float* getTable (int source, int frame, int level) const noexcept
    {
        source = juce::jlimit (0, numSources - 1, source);
        frame  = juce::jlimit (0, numFrames - 1, frame);
        level  = juce::jlimit (0, numLevels - 1, level);
        return data.data() + (size_t) (((source * numFrames + frame) * numLevels + level) * (tableSize + 1));
    }

    static int levelForFrequency (double freq, double sampleRate) noexcept
    {
        const double maxHarm = (sampleRate * 0.5) / juce::jmax (1.0, freq);
        int level = 0;
        int harms = maxHarmonics;

        while (level < numLevels - 1 && (double) harms > maxHarm)
        {
            harms >>= 1;
            ++level;
        }
        return level;
    }

private:
    static void fillSpectrum (int source, float t, std::vector<float>& amp);

    std::vector<float> data;
};
