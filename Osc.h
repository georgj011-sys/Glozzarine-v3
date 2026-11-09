#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include "WaveBank.h"

// Thirteen synthesis models. "shape" and "color" mean something different per model;
// the panel renames the two knobs when the model changes.
class GlizzarineOsc
{
public:
    enum Type { Wavetable = 0, Analog, Swarm, String, FM, Formant, Noise, Saw, Pulse, Fold, Sync, Vector, Chip, PhaseDist, Harmonic };
    static constexpr int numTypes = 15;

    void prepare (double sr, const WaveBank* waveBank)
    {
        bank = waveBank;
        sampleRate = sr;
        pwmInc = 0.6f / (float) sr;
        ksSize = (int) (sr / 16.0) + 4;
        ks.assign ((size_t) ksSize, 0.0f);
        ksW = 0;
        ksPrev = 0.0f;
    }

    void resetPhases (uint32_t seed) noexcept
    {
        rng = seed * 747796405u + 2891336453u;
        phase = rand01(); p2 = rand01(); p3 = rand01();
        slave = 0.0f; mod = 0.0f; sh = 0.0f; shVal = 0.0f; lp = 0.0f;
        pwm = rand01();
        for (auto& s : swarm) s = rand01();
    }

    void resetDeterministic() noexcept
    {
        phase = p2 = p3 = slave = mod = sh = shVal = lp = pwm = 0.0f;
        for (int i = 0; i < 6; ++i) swarm[i] = (float) i / 6.0f;
        rng = 12345u;
    }

    // Excites the string model; called on every new note.
    void trigger (float brightness) noexcept
    {
        if (ks.empty())
            return;

        const float c = 0.15f + 0.85f * juce::jlimit (0.0f, 1.0f, brightness);
        float s = 0.0f;
        for (auto& v : ks)
        {
            s += c * ((rand01() * 2.0f - 1.0f) - s);
            v = s;
        }
        ksPrev = 0.0f;
    }

    float process (int type, float dt, float shape, float color) noexcept
    {
        dt = juce::jlimit (1.0e-6f, 0.45f, dt);
        const float twoPi = juce::MathConstants<float>::twoPi;
        float out = 0.0f;

        switch (type)
        {
            case Wavetable:
            {
                if (bank == nullptr)
                    break;
                const int source = juce::jlimit (0, WaveBank::numSources - 1, (int) std::lround (color * (float) (WaveBank::numSources - 1)));
                const float fp = shape * (float) (WaveBank::numFrames - 1);
                const int f0 = juce::jlimit (0, WaveBank::numFrames - 1, (int) fp);
                const int f1 = juce::jmin (f0 + 1, WaveBank::numFrames - 1);
                const float ff = fp - (float) f0;
                const int mip = WaveBank::levelForFrequency ((double) dt * sampleRate, sampleRate);
                const float* t0 = bank->getTable (source, f0, mip);
                const float* t1 = bank->getTable (source, f1, mip);
                const float idx = phase * (float) WaveBank::tableSize;
                const int i0 = juce::jlimit (0, WaveBank::tableSize - 1, (int) idx);
                const float fr = idx - (float) i0;
                const float s0 = t0[i0] + fr * (t0[i0 + 1] - t0[i0]);
                const float s1 = t1[i0] + fr * (t1[i0 + 1] - t1[i0]);
                out = (s0 + ff * (s1 - s0)) * 0.9f;
                advance (phase, dt);
                break;
            }

            case Analog:
            {
                const float tri = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                const float sw = saw (phase, dt);
                if (shape < 0.5f)
                {
                    out = tri + (sw - tri) * (shape * 2.0f);
                }
                else
                {
                    const float w = 0.5f - 0.42f * color;
                    const float pu = pulse (phase, dt, w) - (2.0f * w - 1.0f);
                    out = sw + (pu - sw) * ((shape - 0.5f) * 2.0f);
                }
                advance (phase, dt);
                break;
            }

            case Saw:
            {
                const float det = shape * 0.025f;
                const float d2 = dt * (1.0f + det), d3 = dt * (1.0f - det);
                out = (saw (phase, dt) + color * (saw (p2, d2) + saw (p3, d3))) / (1.0f + color * 1.3f);
                advance (phase, dt); advance (p2, d2); advance (p3, d3);
                break;
            }

            case Pulse:
            {
                float w = 0.5f - 0.45f * shape + color * 0.4f * std::sin (twoPi * pwm);
                w = juce::jlimit (0.03f, 0.97f, w);
                pwm += pwmInc;
                if (pwm >= 1.0f) pwm -= 1.0f;
                out = pulse (phase, dt, w) - (2.0f * w - 1.0f);
                advance (phase, dt);
                break;
            }

            case Fold:
            {
                const float x = std::sin (twoPi * phase) * (1.0f + shape * 7.0f) + color * 1.5f;
                out = std::sin (x * 1.5707963f);
                advance (phase, dt);
                break;
            }

            case Sync:
            {
                const float ratio = 1.0f + shape * 7.0f;
                const float ds = juce::jmin (dt * ratio, 0.45f);
                out = saw (slave, ds) * (1.0f - 0.5f * color) + saw (phase, dt) * 0.5f * color;
                slave += ds;
                if (slave >= 1.0f) slave -= 1.0f;
                phase += dt;
                if (phase >= 1.0f)
                {
                    phase -= 1.0f;
                    slave = phase * ratio;
                    slave -= std::floor (slave);
                }
                break;
            }

            case FM:
            {
                static constexpr float ratios[12] = { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
                const int ri = juce::jlimit (0, 11, (int) std::lround (color * 11.0f));
                out = std::sin (twoPi * phase + shape * shape * 8.0f * std::sin (twoPi * mod));
                advance (mod, juce::jmin (dt * ratios[ri], 0.49f));
                advance (phase, dt);
                break;
            }

            case Formant:
            {
                const float win = std::pow (1.0f - phase, 1.0f + color * 6.0f);
                out = std::sin (twoPi * phase * (1.0f + shape * 11.0f)) * win * 1.6f;
                advance (phase, dt);
                break;
            }

            case Noise:
            {
                sh += dt * (0.5f + shape * shape * 24.0f);
                if (sh >= 1.0f)
                {
                    sh -= std::floor (sh);
                    shVal = rand01() * 2.0f - 1.0f;
                }
                lp += (shVal - lp) * (1.0f - color * 0.97f);
                out = lp;
                advance (phase, dt);
                break;
            }

            case Vector:
            {
                const float sine = std::sin (twoPi * phase);
                const float tri  = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                const float top  = sine + shape * (tri - sine);
                const float bot  = saw (phase, dt) + shape * (pulse (phase, dt, 0.5f) - saw (phase, dt));
                out = (top + color * (bot - top)) * 1.5f;
                advance (phase, dt);
                break;
            }

            case Chip:
            {
                const float tri = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                const float pul = phase < 0.25f ? 1.0f : -1.0f;
                const float base = tri + color * (pul - tri);
                const float levels = std::exp2 (2.0f + (1.0f - shape) * 5.0f) * 0.5f;
                out = std::round (base * levels) / levels;
                advance (phase, dt);
                break;
            }

            case Swarm:
            {
                static constexpr float spreadTable[6] = { -1.0f, -0.6f, -0.2f, 0.25f, 0.62f, 1.0f };
                float sum = 0.0f;
                for (int i = 0; i < 6; ++i)
                {
                    const float d = dt * (1.0f + spreadTable[i] * shape * 0.04f);
                    const float w = (i == 2 || i == 3) ? 1.0f : color;
                    sum += saw (swarm[i], d) * w;
                    advance (swarm[i], d);
                }
                out = sum / (2.0f + 4.0f * color) * 1.6f;
                break;
            }

            case PhaseDist:
            {
                const float m = 0.5f - 0.49f * shape;
                const float warped = phase < m ? phase * 0.5f / m : 0.5f + (phase - m) * 0.5f / (1.0f - m);
                const float pd = -std::cos (twoPi * warped);
                const float reso = std::sin (twoPi * phase * (1.0f + shape * 14.0f)) * (1.0f - phase) * 1.4f;
                out = pd + color * (reso - pd);
                advance (phase, dt);
                break;
            }

            case Harmonic:
            {
                if (shape != cachedShape || color != cachedColor)
                {
                    cachedShape = shape;
                    cachedColor = color;
                    const float tilt = 2.5f - shape * 2.4f;
                    for (int k = 0; k < 8; ++k)
                        partials[k] = std::pow ((float) (k + 1), -tilt) * (((k + 1) % 2 == 0) ? 1.0f - color : 1.0f);
                }

                float sum = 0.0f, norm = 0.0f;
                for (int k = 0; k < 8; ++k)
                {
                    if ((float) (k + 1) * dt >= 0.48f)
                        break;
                    sum += partials[k] * std::sin (twoPi * phase * (float) (k + 1));
                    norm += partials[k];
                }
                out = norm > 1.0e-4f ? sum / norm * 1.3f : 0.0f;
                advance (phase, dt);
                break;
            }

            case String:
            default:
            {
                if (ks.empty())
                    break;

                const float period = juce::jlimit (2.0f, (float) ksSize - 3.0f, 1.0f / dt);
                float pos = (float) ksW - period;
                if (pos < 0.0f) pos += (float) ksSize;
                int i0 = (int) pos;
                const float fr = pos - (float) i0;
                i0 %= ksSize;
                const int i1 = (i0 + 1) % ksSize;
                const float y = ks[(size_t) i0] + fr * (ks[(size_t) i1] - ks[(size_t) i0]);

                const float b = 0.5f + 0.5f * shape;
                const float decay = 0.985f + 0.0149f * color;
                ks[(size_t) ksW] = decay * (b * y + (1.0f - b) * ksPrev);
                ksPrev = y;
                ksW = (ksW + 1) % ksSize;

                out = y * 1.6f;
                advance (phase, dt);
                break;
            }
        }

        return out;
    }

    static void renderPreview (int type, float shape, float color, float* dest, int n, const WaveBank* waveBank)
    {
        GlizzarineOsc o;
        o.prepare (44100.0, waveBank);
        o.resetDeterministic();
        o.trigger (shape);
        const float dt = 1.0f / (float) n;

        for (int i = 0; i < n * 2; ++i)
            o.process (type, dt, shape, color);

        for (int i = 0; i < n; ++i)
            dest[i] = o.process (type, dt, shape, color);
    }

private:
    static float blep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt;             return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    static float saw (float t, float dt) noexcept { return 2.0f * t - 1.0f - blep (t, dt); }

    static float pulse (float t, float dt, float w) noexcept
    {
        float v = t < w ? 1.0f : -1.0f;
        v += blep (t, dt);
        float t2 = t + 1.0f - w;
        if (t2 >= 1.0f) t2 -= 1.0f;
        v -= blep (t2, dt);
        return v;
    }

    static void advance (float& p, float d) noexcept
    {
        p += d;
        if (p >= 1.0f) p -= 1.0f;
    }

    float rand01() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) (rng >> 8) / 16777216.0f;
    }

    float phase = 0.0f, p2 = 0.0f, p3 = 0.0f, slave = 0.0f, mod = 0.0f;
    float sh = 0.0f, shVal = 0.0f, lp = 0.0f, pwm = 0.0f, pwmInc = 0.0f;
    float swarm[6] {};
    float partials[8] {};
    float cachedShape = -1.0f, cachedColor = -1.0f;
    std::vector<float> ks;
    int ksSize = 0, ksW = 0;
    float ksPrev = 0.0f;
    uint32_t rng = 1u;
    const WaveBank* bank = nullptr;
    double sampleRate = 44100.0;
};
