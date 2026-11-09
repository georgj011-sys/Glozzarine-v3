#include "WaveBank.h"
#include <cmath>
#include <cstdint>

namespace
{
    float hash01 (uint32_t a, uint32_t seed) noexcept
    {
        uint32_t x = a * 374761393u + seed * 668265263u;
        x = (x ^ (x >> 13)) * 1274126177u;
        x ^= x >> 16;
        return (float) (x & 0xffffffu) / 16777216.0f;
    }

    float gauss (float x, float centre, float width) noexcept
    {
        const float d = (x - centre) / width;
        return std::exp (-d * d);
    }

    // Sources 8+ are "textures": random but fixed phases give a softer, airier sound.
    bool isTexture (int source) noexcept { return source >= 8; }

    bool isPowerOfTwo (int v) noexcept { return v > 0 && (v & (v - 1)) == 0; }
}

const juce::StringArray& WaveBank::getSourceNames()
{
    static const juce::StringArray names {
        // Wavetables
        "Saw > Square", "Pulse Sweep", "Formant Vox", "Glass Bell",
        "Organ", "Spectral Comb", "Hollow Sync", "Digital Grit",
        // Textures
        "Choir Air", "Strings Ensemble", "Shimmer Octaves", "Dark Drone",
        "Glass Pad", "Bowed Metal", "Vowel Morph", "Spectral Cloud"
    };
    return names;
}

WaveBank::WaveBank()
{
    static_assert (tableSize == 2048, "FFT order below assumes 2048");

    data.assign ((size_t) numSources * numFrames * numLevels * (tableSize + 1), 0.0f);

    juce::dsp::FFT fft (11);
    std::vector<float> amp ((size_t) maxHarmonics + 1);
    std::vector<float> phase ((size_t) maxHarmonics + 1);
    std::vector<float> buffer ((size_t) tableSize * 2);

    for (int s = 0; s < numSources; ++s)
    {
        // Phases stay identical across frames so morphing never cancels out.
        for (int h = 1; h <= maxHarmonics; ++h)
            phase[(size_t) h] = isTexture (s)
                ? hash01 ((uint32_t) h, (uint32_t) (s + 101)) * juce::MathConstants<float>::twoPi
                : -juce::MathConstants<float>::halfPi;

        for (int f = 0; f < numFrames; ++f)
        {
            const float t = (float) f / (float) (numFrames - 1);
            std::fill (amp.begin(), amp.end(), 0.0f);
            fillSpectrum (s, t, amp);

            for (int l = 0; l < numLevels; ++l)
            {
                const int harms = maxHarmonics >> l;
                std::fill (buffer.begin(), buffer.end(), 0.0f);

                for (int h = 1; h <= harms && h < tableSize / 2; ++h)
                {
                    buffer[(size_t) (2 * h)]     = amp[(size_t) h] * std::cos (phase[(size_t) h]);
                    buffer[(size_t) (2 * h + 1)] = amp[(size_t) h] * std::sin (phase[(size_t) h]);
                }

                fft.performRealOnlyInverseTransform (buffer.data());

                float peak = 0.0f;
                for (int i = 0; i < tableSize; ++i)
                    peak = juce::jmax (peak, std::abs (buffer[(size_t) i]));

                const float gain = peak > 1.0e-9f ? 1.0f / peak : 0.0f;
                float* dest = data.data() + (size_t) (((s * numFrames + f) * numLevels + l) * (tableSize + 1));

                for (int i = 0; i < tableSize; ++i)
                    dest[i] = buffer[(size_t) i] * gain;

                dest[tableSize] = dest[0];
            }
        }
    }
}

void WaveBank::fillSpectrum (int source, float t, std::vector<float>& amp)
{
    auto set = [&amp] (int h, float value) { if (h >= 1 && h <= maxHarmonics) amp[(size_t) h] = value; };

    switch (source)
    {
        case 0: // Saw > Square
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, (1.0f / (float) h) * ((h & 1) ? 1.0f : 1.0f - t));
            break;

        case 1: // Pulse Sweep
        {
            const float w = 0.5f - 0.45f * t;
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, std::sin (juce::MathConstants<float>::pi * (float) h * w) / (float) h);
            break;
        }

        case 2: // Formant Vox
        {
            const float f1 = 2.0f + 5.0f * t, f2 = 8.0f + 14.0f * t;
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float x = (float) h;
                set (h, (gauss (x, f1, 1.5f) + 0.7f * gauss (x, f2, 3.0f) + 0.3f * gauss (x, f2 * 1.8f, 5.0f)) / std::sqrt (x)
                        + 0.02f / x);
            }
            break;
        }

        case 3: // Glass Bell
        {
            const int partials[] = { 1, 2, 3, 5, 7, 9, 11, 13, 17, 19, 23, 29, 31, 37, 41, 47, 53, 59 };
            bool first = true;
            for (int h : partials)
            {
                const float weight = first ? 1.0f : 0.15f + 0.85f * t;
                set (h, weight / std::sqrt ((float) h) * (1.0f - 0.5f * hash01 ((uint32_t) h, 3u)));
                first = false;
            }
            break;
        }

        case 4: // Organ
        {
            const int bars[]      = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            const float weights[] = { 1.0f, 0.8f, 0.6f, 0.7f, 0.5f, 0.45f, 0.3f, 0.35f, 0.3f };
            for (int k = 0; k < 9; ++k)
                set (bars[k], weights[k] * (bars[k] <= 2 ? 1.0f : t));
            break;
        }

        case 5: // Spectral Comb
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, std::pow ((float) h, -0.7f)
                        * (0.5f + 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) h * (0.05f + 0.3f * t))));
            break;

        case 6: // Hollow Sync
        {
            const float r = 1.0f + 7.0f * t;
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, (1.0f + 3.0f * gauss ((float) h, r, 1.0f + 0.2f * r)) / (float) h);
            break;
        }

        case 7: // Digital Grit
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, (h == 1 || hash01 ((uint32_t) h, 7u) < 1.0f - 0.75f * t) ? 1.0f / (float) h : 0.0f);
            break;

        case 8: // Choir Air
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float x = (float) h;
                set (h, std::pow (x, -0.6f) * (gauss (x, 3.0f + 2.0f * t, 2.0f)
                                               + 0.8f * gauss (x, 9.0f + 6.0f * t, 4.0f)
                                               + 0.4f * gauss (x, 20.0f + 10.0f * t, 8.0f))
                        + 0.002f);
            }
            break;

        case 9: // Strings Ensemble
            for (int h = 1; h <= maxHarmonics; ++h)
                set (h, std::pow ((float) h, -(1.2f - 0.6f * t)) * (0.7f + 0.6f * hash01 ((uint32_t) h, 9u)));
            break;

        case 10: // Shimmer Octaves
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                if (isPowerOfTwo (h))
                    set (h, 1.0f / std::sqrt ((float) h));
                else if (h % 3 == 0 && isPowerOfTwo (h / 3))
                    set (h, t / std::sqrt ((float) h));
            }
            break;

        case 11: // Dark Drone
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float x = (float) h;
                set (h, 1.0f / (x * x) + (((h & 1) && h > 1) ? t * 0.3f / x : 0.0f));
            }
            break;

        case 12: // Glass Pad
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const bool core = (h == 1 || h == 2 || h == 4 || h == 5 || h == 8);
                set (h, (core ? 1.0f / std::sqrt ((float) h) : 0.0f) + t * 0.15f * gauss ((float) h, 28.0f, 10.0f));
            }
            break;

        case 13: // Bowed Metal
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float r = hash01 ((uint32_t) h, 13u);
                const float limit = 16.0f + t * 200.0f;
                set (h, r * r * r / std::sqrt ((float) h) * ((float) h <= limit ? 1.0f : 0.1f));
            }
            amp[1] = juce::jmax (amp[1], 0.6f);
            break;

        case 14: // Vowel Morph: A > E > I > O > U
        {
            const float f1s[] = { 5.3f, 2.7f, 2.0f, 3.3f, 2.0f };
            const float f2s[] = { 8.0f, 13.0f, 17.0f, 6.0f, 4.7f };
            const float pos = t * 4.0f;
            const int i0 = juce::jmin (3, (int) pos);
            const float fr = pos - (float) i0;
            const float f1 = f1s[i0] + fr * (f1s[i0 + 1] - f1s[i0]);
            const float f2 = f2s[i0] + fr * (f2s[i0 + 1] - f2s[i0]);

            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float x = (float) h;
                set (h, (gauss (x, f1, 1.2f) + 0.8f * gauss (x, f2, 2.0f)) / std::sqrt (x) + 0.01f / x);
            }
            break;
        }

        case 15: // Spectral Cloud
        default:
        {
            const float c = 4.0f + 56.0f * t;
            for (int h = 1; h <= maxHarmonics; ++h)
            {
                const float x = (float) h;
                set (h, gauss (x, c, 4.0f + 0.5f * c) / std::pow (x, 0.3f) + 0.05f / x);
            }
            break;
        }
    }
}
