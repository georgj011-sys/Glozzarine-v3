#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "Params.h"

struct FxParams
{
    int distType = 0;
    float distAmt = 0.0f;
    float tapeWow = 0.0f, tapeFlutter = 0.0f, tapeAge = 0.0f;
    int chorusType = 0;
    float modWheel = 0.0f;
    float chorusRate = 0.5f, chorusDepth = 0.5f, chorusMix = 0.0f;
    float phaserRate = 0.3f, phaserDepth = 0.7f, phaserMix = 0.0f;
    int delaySync = 0;
    float delayTime = 0.3f, delayFb = 0.35f, delayTone = 0.5f, delayMix = 0.0f;
    float cloudSize = 0.4f, cloudDensity = 0.4f, cloudSpray = 0.3f, cloudMix = 0.0f;
    float reverbSize = 0.5f, reverbShimmer = 0.0f, reverbMix = 0.15f;
    int gateRate = 1, gatePattern = 1;
    float gateDepth = 0.0f;
    float panRate = 1.0f, panDepth = 0.0f, haasMs = 0.0f, width = 1.0f, master = 0.7f;

    double bpm = 120.0, ppq = 0.0;
    bool hasPpq = false;
};

// Octave-up pitch shifter (two crossfaded taps), used for the shimmer reverb
class OctaveShifter
{
public:
    void prepare() { buf.assign (size, 0.0f); w = 0; d = 0.0f; }

    float process (float x) noexcept
    {
        buf[(size_t) w] = x;
        d -= 1.0f;
        if (d < 0.0f) d += win;
        float d2 = d + win * 0.5f;
        if (d2 >= win) d2 -= win;
        const float out = read (d) * hann (d / win) + read (d2) * hann (d2 / win);
        w = (w + 1) & (size - 1);
        return out;
    }

private:
    float read (float delay) const noexcept
    {
        float pos = (float) w - delay;
        if (pos < 0.0f) pos += (float) size;
        int i0 = (int) pos;
        const float fr = pos - (float) i0;
        i0 &= size - 1;
        const int i1 = (i0 + 1) & (size - 1);
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    static float hann (float t) noexcept
    {
        const float s = std::sin (juce::MathConstants<float>::pi * t);
        return s * s;
    }

    static constexpr int size = 4096;
    static constexpr float win = 2048.0f;
    std::vector<float> buf;
    int w = 0;
    float d = 0.0f;
};

struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

    void shelf (bool high, double sr, double freq, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0), alpha = std::sin (w0) / 2.0 * std::sqrt (2.0);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        double B0, B1, B2, A0, A1, A2;
        if (high)
        {
            B0 = A * ((A + 1) + (A - 1) * cs + sq);  B1 = -2 * A * ((A - 1) + (A + 1) * cs);  B2 = A * ((A + 1) + (A - 1) * cs - sq);
            A0 = (A + 1) - (A - 1) * cs + sq;        A1 = 2 * ((A - 1) - (A + 1) * cs);       A2 = (A + 1) - (A - 1) * cs - sq;
        }
        else
        {
            B0 = A * ((A + 1) - (A - 1) * cs + sq);  B1 = 2 * A * ((A - 1) - (A + 1) * cs);   B2 = A * ((A + 1) - (A - 1) * cs - sq);
            A0 = (A + 1) + (A - 1) * cs + sq;        A1 = -2 * ((A - 1) + (A + 1) * cs);      A2 = (A + 1) + (A - 1) * cs - sq;
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
        z1 = z2 = 0.0f;
    }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

class GlizzarineFX
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        capacity = juce::jmax (1, maxBlock);

        tapeL.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        tapeR.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        chorusL.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        chorusR.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        delayL.assign ((size_t) (sr * 2.1) + 4, 0.0f);
        delayR.assign ((size_t) (sr * 2.1) + 4, 0.0f);
        cloudL.assign ((size_t) (sr * 3.0) + 4, 0.0f);
        cloudR.assign ((size_t) (sr * 3.0) + 4, 0.0f);
        haasBuf.assign ((size_t) (sr * 0.04) + 4, 0.0f);
        tapeW = chorusW = delayW = cloudW = haasW = 0;

        rvBuffer.setSize (2, capacity);
        shimmerFbL.assign ((size_t) capacity, 0.0f);
        shimmerFbR.assign ((size_t) capacity, 0.0f);
        shiftL.prepare();
        shiftR.prepare();

        delaySmooth.reset (sr, 0.25);
        delaySmooth.setCurrentAndTargetValue (0.3f * (float) sr);
        haasSmooth.reset (sr, 0.1);
        haasSmooth.setCurrentAndTargetValue (0.0f);

        reverb.prepare ({ sr, (juce::uint32) capacity, 2 });
        reverb.reset();

        for (auto& g : grains) g.active = false;
        for (int ch = 0; ch < 2; ++ch)
        {
            phFb[ch] = phA[ch] = 0.0f;
            for (auto& z : phZ[ch]) z = 0.0f;
        }

        dampL = dampR = ageL = ageR = 0.0f;
        tapeWowPh = tapeFlPh = chLfo = phLfo = panLfo = 0.0f;
        phCount = crushCount = 0;
        heldL = heldR = 0.0f;
        cloudCountdown = 0.0f;
        gateBeat = 0.0;
        gateEnv = 1.0f;
        lastMaster = -1.0f;
        limGain = 1.0f;
        limAttack = 1.0f - std::exp (-1.0f / (0.0005f * (float) sr));
        limRelease = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));

        // Tone balance: a touch more low end, softer top
        for (int ch = 0; ch < 2; ++ch)
        {
            lowShelf[ch].shelf (false, sr, 100.0, 3.5);
            highShelf[ch].shelf (true, sr, 7500.0, -3.0);
        }
    }

    void process (juce::AudioBuffer<float>& buffer, const FxParams& p)
    {
        if (buffer.getNumChannels() < 2 || delayL.empty())
            return;

        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        const float fs = (float) sr;
        const float twoPi = juce::MathConstants<float>::twoPi;

        processDrive (L, R, n, p);

        // Tape: wow, flutter, darkening, hiss and saturation
        if (p.tapeWow + p.tapeFlutter + p.tapeAge > 0.001f)
        {
            const int size = (int) tapeL.size();
            const float lpCoeff = 1.0f - p.tapeAge * 0.88f;
            const float sat = 1.0f + p.tapeAge * 2.0f;

            for (int i = 0; i < n; ++i)
            {
                tapeL[(size_t) tapeW] = L[i];
                tapeR[(size_t) tapeW] = R[i];

                const float wow = std::sin (twoPi * tapeWowPh);
                const float fl  = std::sin (twoPi * tapeFlPh);
                tapeWowPh += 0.55f / fs;  if (tapeWowPh >= 1.0f) tapeWowPh -= 1.0f;
                tapeFlPh  += 6.8f / fs;   if (tapeFlPh >= 1.0f) tapeFlPh -= 1.0f;

                const float dms = 5.0f + p.tapeWow * 3.5f * (1.0f + wow) + p.tapeFlutter * 0.35f * (1.0f + fl);
                const float dL = dms * 0.001f * fs;
                const float dR = (dms + p.tapeWow * 0.4f * std::sin (twoPi * (tapeWowPh + 0.3f))) * 0.001f * fs;

                float l = readInterp (tapeL, tapeW, dL);
                float r = readInterp (tapeR, tapeW, juce::jmax (1.0f, dR));

                ageL += lpCoeff * (l - ageL);
                ageR += lpCoeff * (r - ageR);
                const float hiss = p.tapeAge * 0.004f;
                l = std::tanh ((ageL + (rng.nextFloat() * 2.0f - 1.0f) * hiss) * sat) / sat * 1.1f;
                r = std::tanh ((ageR + (rng.nextFloat() * 2.0f - 1.0f) * hiss) * sat) / sat * 1.1f;

                L[i] = l;
                R[i] = r;
                tapeW = (tapeW + 1) % size;
            }
        }

        // Ensemble: three modulated voices per side, like 70s string machines
        if (p.chorusMix > 0.001f && p.chorusType == 1)
        {
            const int size = (int) chorusL.size();
            for (int i = 0; i < n; ++i)
            {
                chorusL[(size_t) chorusW] = L[i];
                chorusR[(size_t) chorusW] = R[i];

                chLfo += p.chorusRate * 0.6f / fs;
                if (chLfo >= 1.0f) chLfo -= 1.0f;
                chFast += 6.0f / fs;
                if (chFast >= 1.0f) chFast -= 1.0f;

                float wl = 0.0f, wr = 0.0f;
                for (int v = 0; v < 3; ++v)
                {
                    const float off = (float) v / 3.0f;
                    const float slowL = std::sin (twoPi * (chLfo + off));
                    const float slowR = std::sin (twoPi * (chLfo + off + 0.5f));
                    const float fast  = std::sin (twoPi * (chFast + off));
                    const float dL = (10.0f + p.chorusDepth * (3.0f * slowL + 0.4f * fast)) * 0.001f * fs;
                    const float dR = (10.0f + p.chorusDepth * (3.0f * slowR + 0.4f * fast)) * 0.001f * fs;
                    wl += readInterp (chorusL, chorusW, dL);
                    wr += readInterp (chorusR, chorusW, dR);
                }

                L[i] = L[i] * (1.0f - 0.6f * p.chorusMix) + wl * 0.4f * p.chorusMix;
                R[i] = R[i] * (1.0f - 0.6f * p.chorusMix) + wr * 0.4f * p.chorusMix;
                chorusW = (chorusW + 1) % size;
            }
        }

        // Rotary speaker (Leslie): mod wheel switches slow / fast, with a smooth spin-up
        if (p.chorusMix > 0.001f && p.chorusType == 2)
        {
            const int size = (int) chorusL.size();
            const float target = p.modWheel > 0.5f ? 6.7f : 0.8f;
            const float accel = 1.0f - std::exp (-1.0f / (0.9f * fs));
            for (int i = 0; i < n; ++i)
            {
                rotorSpeed += (target - rotorSpeed) * accel;
                rotorPhase += rotorSpeed / fs;
                if (rotorPhase >= 1.0f) rotorPhase -= 1.0f;

                const float mono = 0.5f * (L[i] + R[i]);
                chorusL[(size_t) chorusW] = mono;
                const float sL = std::sin (twoPi * rotorPhase);
                const float sR = -sL;
                const float wl = readInterp (chorusL, chorusW, (2.0f + 0.9f * sL) * 0.001f * fs) * (1.0f + 0.3f * std::cos (twoPi * rotorPhase));
                const float wr = readInterp (chorusL, chorusW, (2.0f + 0.9f * sR) * 0.001f * fs) * (1.0f - 0.3f * std::cos (twoPi * rotorPhase));
                L[i] = L[i] * (1.0f - p.chorusMix) + wl * p.chorusMix;
                R[i] = R[i] * (1.0f - p.chorusMix) + wr * p.chorusMix;
                chorusW = (chorusW + 1) % size;
            }
        }

        // Classic stereo chorus
        if (p.chorusMix > 0.001f && p.chorusType == 0)
        {
            const int size = (int) chorusL.size();
            for (int i = 0; i < n; ++i)
            {
                chorusL[(size_t) chorusW] = L[i];
                chorusR[(size_t) chorusW] = R[i];

                const float lfoL = std::sin (twoPi * chLfo);
                const float lfoR = std::sin (twoPi * (chLfo + 0.25f));
                chLfo += p.chorusRate / fs;
                if (chLfo >= 1.0f) chLfo -= 1.0f;

                const float wetL = readInterp (chorusL, chorusW, (7.0f + p.chorusDepth * 6.0f * (1.0f + lfoL)) * 0.001f * fs);
                const float wetR = readInterp (chorusR, chorusW, (7.0f + p.chorusDepth * 6.0f * (1.0f + lfoR)) * 0.001f * fs);

                L[i] = L[i] * (1.0f - 0.5f * p.chorusMix) + wetL * 0.7f * p.chorusMix;
                R[i] = R[i] * (1.0f - 0.5f * p.chorusMix) + wetR * 0.7f * p.chorusMix;
                chorusW = (chorusW + 1) % size;
            }
        }

        // Stereo phaser
        if (p.phaserMix > 0.001f)
        {
            for (int i = 0; i < n; ++i)
            {
                if (phCount <= 0)
                {
                    phCount = 16;
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float lfo = std::sin (twoPi * (phLfo + 0.25f * (float) ch));
                        const float f = juce::jmin (fs * 0.45f, 250.0f * std::exp2 ((0.5f + 0.5f * lfo) * p.phaserDepth * 4.5f));
                        const float t = std::tan (juce::MathConstants<float>::pi * f / fs);
                        phA[ch] = (t - 1.0f) / (t + 1.0f);
                    }
                }
                --phCount;
                phLfo += p.phaserRate / fs;
                if (phLfo >= 1.0f) phLfo -= 1.0f;

                for (int ch = 0; ch < 2; ++ch)
                {
                    float* s = ch == 0 ? L : R;
                    const float x = s[i];
                    float v = x + phFb[ch] * 0.55f;
                    for (int st = 0; st < 6; ++st)
                    {
                        const float y = phA[ch] * v + phZ[ch][st];
                        phZ[ch][st] = v - phA[ch] * y;
                        v = y;
                    }
                    phFb[ch] = v;
                    s[i] = x * (1.0f - 0.5f * p.phaserMix) + v * 0.5f * p.phaserMix;
                }
            }
        }

        // Ping-pong delay (free or tempo-synced)
        {
            const int size = (int) delayL.size();
            const double beats = Params::delaySyncBeats (p.delaySync);
            const float seconds = beats > 0.0 ? (float) (beats * 60.0 / juce::jmax (20.0, p.bpm)) : p.delayTime;
            delaySmooth.setTargetValue (juce::jlimit (1.0f, (float) size - 3.0f, seconds * fs));
            const float damp = 0.1f + 0.85f * p.delayTone;

            for (int i = 0; i < n; ++i)
            {
                const float d = delaySmooth.getNextValue();
                const float dl = readInterp (delayL, delayW, d);
                const float dr = readInterp (delayR, delayW, d);
                dampL += damp * (dl - dampL);
                dampR += damp * (dr - dampR);

                const float inL = L[i], inR = R[i];
                delayL[(size_t) delayW] = 0.5f * (inL + inR) + dampR * p.delayFb;
                delayR[(size_t) delayW] = dampL * p.delayFb;

                L[i] = inL + dl * p.delayMix;
                R[i] = inR + dr * p.delayMix;
                delayW = (delayW + 1) % size;
            }
        }

        processCloud (L, R, n, p);
        processReverb (buffer, L, R, n, p);
        processGate (L, R, n, p);

        // Auto-pan, Haas, width, master
        const int hsize = (int) haasBuf.size();
        haasSmooth.setTargetValue (juce::jlimit (0.0f, (float) hsize - 3.0f, p.haasMs * 0.001f * fs));
        if (lastMaster < 0.0f)
            lastMaster = p.master;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            if (p.panDepth > 0.001f)
            {
                const float pv = p.panDepth * std::sin (twoPi * panLfo);
                l *= pv > 0.0f ? 1.0f - pv : 1.0f;
                r *= pv < 0.0f ? 1.0f + pv : 1.0f;
            }
            panLfo += p.panRate / fs;
            if (panLfo >= 1.0f) panLfo -= 1.0f;

            const float hd = haasSmooth.getNextValue();
            haasBuf[(size_t) haasW] = r;
            if (hd > 1.0f)
                r = readInterp (haasBuf, haasW, hd);
            haasW = (haasW + 1) % hsize;

            const float m = 0.5f * (l + r);
            const float s = 0.5f * (l - r) * p.width;

            const float g = lastMaster + (p.master - lastMaster) * (float) i / (float) n;
            float ol = highShelf[0].process (lowShelf[0].process (m + s)) * g;
            float orr = highShelf[1].process (lowShelf[1].process (m - s)) * g;

            // Clean peak limiter: turns the level down smoothly instead of bending the waveform
            const float peak = juce::jmax (std::abs (ol), std::abs (orr));
            const float target = peak > limThreshold ? limThreshold / peak : 1.0f;
            limGain += (target - limGain) * (target < limGain ? limAttack : limRelease);
            ol *= limGain;
            orr *= limGain;
            L[i] = juce::jlimit (-1.0f, 1.0f, ol);
            R[i] = juce::jlimit (-1.0f, 1.0f, orr);
        }
        lastMaster = p.master;
    }

private:
    void processDrive (float* L, float* R, int n, const FxParams& p)
    {
        if (p.distAmt <= 0.001f)
            return;

        if (p.distType == 3) // Crush
        {
            const float levels = std::exp2 (16.0f - p.distAmt * 13.0f);
            const int hold = 1 + (int) (p.distAmt * p.distAmt * 24.0f);
            for (int i = 0; i < n; ++i)
            {
                if (++crushCount >= hold)
                {
                    crushCount = 0;
                    heldL = std::round (L[i] * levels) / levels;
                    heldR = std::round (R[i] * levels) / levels;
                }
                L[i] = heldL;
                R[i] = heldR;
            }
            return;
        }

        const float g = 1.0f + p.distAmt * 12.0f;
        const float comp = 1.0f / (1.0f + p.distAmt * 1.5f);

        if (p.distType == 4) // Rectify: octave-up fuzz with DC removal
        {
            for (int i = 0; i < n; ++i)
            {
                const float rl = std::abs (L[i]) * 2.0f, rr = std::abs (R[i]) * 2.0f;
                rectDcL += 0.002f * (rl - rectDcL);
                rectDcR += 0.002f * (rr - rectDcR);
                L[i] = L[i] * (1.0f - p.distAmt) + std::tanh ((rl - rectDcL) * g * 0.5f) * comp * p.distAmt;
                R[i] = R[i] * (1.0f - p.distAmt) + std::tanh ((rr - rectDcR) * g * 0.5f) * comp * p.distAmt;
            }
            return;
        }

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float& x = ch == 0 ? L[i] : R[i];
                if (p.distType == 0)      x = std::tanh (x * g) * comp;
                else if (p.distType == 1) x = juce::jlimit (-1.0f, 1.0f, x * g) * comp;
                else                      x = std::sin (x * g * 1.2f) * 0.8f;
            }
        }
    }

    struct Grain
    {
        bool active = false;
        float pos = 0.0f, inc = 1.0f, gL = 0.0f, gR = 0.0f;
        int age = 0, len = 1;
    };

    void spawnGrain (const FxParams& p)
    {
        for (auto& g : grains)
        {
            if (g.active)
                continue;

            const float fs = (float) sr;
            const int size = (int) cloudL.size();
            g.len = juce::jmax (64, (int) ((0.04f + p.cloudSize * 0.36f) * fs));

            float ratio = 1.0f;
            const float r = rng.nextFloat();
            if (r < p.cloudSpray * 0.25f)      ratio = 2.0f;
            else if (r < p.cloudSpray * 0.4f)  ratio = 0.5f;
            ratio *= std::exp2 ((rng.nextFloat() * 2.0f - 1.0f) * p.cloudSpray * 15.0f / 1200.0f);
            g.inc = ratio;

            const float back = (float) g.len * ratio + rng.nextFloat() * fs * (0.05f + p.cloudSize * 1.2f) + 8.0f;
            float start = (float) cloudW - juce::jmin (back, (float) size - 8.0f);
            if (start < 0.0f) start += (float) size;
            g.pos = start;
            g.age = 0;

            const float pan = (rng.nextFloat() * 2.0f - 1.0f) * (0.3f + p.cloudSpray * 0.7f);
            g.gL = std::sqrt (0.5f * (1.0f - pan));
            g.gR = std::sqrt (0.5f * (1.0f + pan));
            g.active = true;
            return;
        }
    }

    void processCloud (float* L, float* R, int n, const FxParams& p)
    {
        const int size = (int) cloudL.size();
        const bool on = p.cloudMix > 0.001f;
        const float fs = (float) sr;

        for (int i = 0; i < n; ++i)
        {
            cloudL[(size_t) cloudW] = L[i];
            cloudR[(size_t) cloudW] = R[i];

            if (on)
            {
                cloudCountdown -= 1.0f;
                if (cloudCountdown <= 0.0f)
                {
                    spawnGrain (p);
                    cloudCountdown = fs / (1.0f + p.cloudDensity * 30.0f) * (0.5f + rng.nextFloat());
                }

                float wl = 0.0f, wr = 0.0f;
                for (auto& g : grains)
                {
                    if (! g.active)
                        continue;

                    const float t = (float) g.age / (float) g.len;
                    const float env = std::sin (juce::MathConstants<float>::pi * t);
                    int i0 = (int) g.pos;
                    const float fr = g.pos - (float) i0;
                    i0 %= size;
                    const int i1 = (i0 + 1) % size;
                    const float s = 0.5f * ((cloudL[(size_t) i0] + fr * (cloudL[(size_t) i1] - cloudL[(size_t) i0]))
                                          + (cloudR[(size_t) i0] + fr * (cloudR[(size_t) i1] - cloudR[(size_t) i0])));
                    wl += s * env * env * g.gL;
                    wr += s * env * env * g.gR;

                    g.pos += g.inc;
                    if (g.pos >= (float) size) g.pos -= (float) size;
                    if (++g.age >= g.len) g.active = false;
                }

                L[i] = L[i] * (1.0f - 0.5f * p.cloudMix) + wl * 0.8f * p.cloudMix;
                R[i] = R[i] * (1.0f - 0.5f * p.cloudMix) + wr * 0.8f * p.cloudMix;
            }

            cloudW = (cloudW + 1) % size;
        }

        if (! on)
            for (auto& g : grains)
                g.active = false;
    }

    void processReverb (juce::AudioBuffer<float>&, float* L, float* R, int n, const FxParams& p)
    {
        if (p.reverbMix <= 0.001f && p.reverbShimmer <= 0.001f)
            return;

        if (n > capacity)
        {
            capacity = n;
            rvBuffer.setSize (2, capacity, false, false, true);
            shimmerFbL.resize ((size_t) capacity, 0.0f);
            shimmerFbR.resize ((size_t) capacity, 0.0f);
        }

        float* rl = rvBuffer.getWritePointer (0);
        float* rr = rvBuffer.getWritePointer (1);
        const float fb = p.reverbShimmer * 0.72f;

        for (int i = 0; i < n; ++i)
        {
            rl[i] = L[i] + shimmerFbL[(size_t) i] * fb;
            rr[i] = R[i] + shimmerFbR[(size_t) i] * fb;
        }

        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.3f + 0.69f * p.reverbSize;
        rp.damping    = 0.35f;
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        juce::dsp::AudioBlock<float> block (rvBuffer.getArrayOfWritePointers(), 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        for (int i = 0; i < n; ++i)
        {
            if (p.reverbShimmer > 0.001f)
            {
                // Octave-up feedback, high-passed and soft-limited so it can never build up
                const float sl = shiftL.process (rl[i]);
                const float sr2 = shiftR.process (rr[i]);
                shimLpL += 0.02f * (sl - shimLpL);
                shimLpR += 0.02f * (sr2 - shimLpR);
                shimmerFbL[(size_t) i] = std::tanh (sl - shimLpL);
                shimmerFbR[(size_t) i] = std::tanh (sr2 - shimLpR);
            }
            else
            {
                shimmerFbL[(size_t) i] = shimmerFbR[(size_t) i] = 0.0f;
            }

            // Reverb plus a direct layer of the octave-up shimmer trail
            const float wetGain = juce::jmax (p.reverbMix, p.reverbShimmer * 0.6f);
            L[i] = L[i] * (1.0f - 0.3f * p.reverbMix) + rl[i] * wetGain + shimmerFbL[(size_t) i] * p.reverbShimmer * 0.55f;
            R[i] = R[i] * (1.0f - 0.3f * p.reverbMix) + rr[i] * wetGain + shimmerFbR[(size_t) i] * p.reverbShimmer * 0.55f;
        }
    }

    void processGate (float* L, float* R, int n, const FxParams& p)
    {
        const double beatInc = juce::jmax (20.0, p.bpm) / 60.0 / sr;
        if (p.hasPpq)
            gateBeat = p.ppq;

        if (p.gateDepth <= 0.001f)
        {
            gateBeat += beatInc * n;
            gateEnv = 1.0f;
            return;
        }

        const double stepBeats = Params::gateRateBeats (p.gateRate);
        const char* pattern = Params::gatePattern (p.gatePattern);
        const int len = (int) std::strlen (pattern);
        const float smooth = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const double stepPos = gateBeat / stepBeats;
            const long long step = (long long) std::floor (stepPos);
            const double frac = stepPos - (double) step;
            const int idx = (int) (((step % len) + len) % len);
            const bool open = pattern[idx] == 'x' && frac < 0.7;
            const float target = open ? 1.0f : 1.0f - p.gateDepth;

            gateEnv += (target - gateEnv) * smooth;
            L[i] *= gateEnv;
            R[i] *= gateEnv;
            gateBeat += beatInc;
        }
    }

    static float softLimit (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.8f)
            return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0.0f ? -y : y;
    }

    static float readInterp (const std::vector<float>& buf, int writePos, float delay) noexcept
    {
        const int size = (int) buf.size();
        float pos = (float) writePos - delay;
        while (pos < 0.0f) pos += (float) size;
        int i0 = (int) pos;
        const float fr = pos - (float) i0;
        i0 %= size;
        const int i1 = (i0 + 1) % size;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    double sr = 44100.0;
    int capacity = 512;
    juce::Random rng;

    std::vector<float> tapeL, tapeR, chorusL, chorusR, delayL, delayR, cloudL, cloudR, haasBuf;
    int tapeW = 0, chorusW = 0, delayW = 0, cloudW = 0, haasW = 0;

    float tapeWowPh = 0.0f, tapeFlPh = 0.0f, ageL = 0.0f, ageR = 0.0f;
    float chFast = 0.0f;
    float chLfo = 0.0f, phLfo = 0.0f, panLfo = 0.0f;
    float phZ[2][6] {}, phFb[2] {}, phA[2] {};
    int phCount = 0;
    float dampL = 0.0f, dampR = 0.0f;
    int crushCount = 0;
    float heldL = 0.0f, heldR = 0.0f;
    float rectDcL = 0.0f, rectDcR = 0.0f;

    Grain grains[16];
    float cloudCountdown = 0.0f;

    juce::AudioBuffer<float> rvBuffer;
    std::vector<float> shimmerFbL, shimmerFbR;
    OctaveShifter shiftL, shiftR;
    juce::dsp::Reverb reverb;

    double gateBeat = 0.0;
    float gateEnv = 1.0f;

    juce::SmoothedValue<float> delaySmooth, haasSmooth;
    float lastMaster = -1.0f;
    Biquad lowShelf[2], highShelf[2];
    float limGain = 1.0f, limAttack = 0.5f, limRelease = 0.001f;
    static constexpr float limThreshold = 0.89f;
    float shimLpL = 0.0f, shimLpR = 0.0f;
    float rotorPhase = 0.0f, rotorSpeed = 0.8f;
};
