#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>

namespace Arp
{
    enum Mode { Up = 0, Down, UpDown, Played, Random, Converge, Diverge, Pinky, Thumb, OctaveBounce, RandomWalk, Chord };

    constexpr int maxHeld = 16;
    constexpr int maxList = maxHeld * 4;

    struct NoteList
    {
        int n[maxList] {};
        int count = 0;
        void add (int v) noexcept { if (count < maxList) n[count++] = v; }
    };

    // Expands a list across octaves
    inline void expand (const NoteList& base, int octaves, NoteList& out) noexcept
    {
        out.count = 0;
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < base.count; ++i)
                out.add (base.n[i] + 12 * o);
    }

    // Picks the note(s) for a step. Returns false if nothing to play.
    // For Chord mode, all notes go to "result"; otherwise result holds one note.
    template <typename RandomFn>
    bool pick (int mode, const NoteList& sorted, const NoteList& played, int octaves, int index,
               int& walkPos, RandomFn&& randomInt, NoteList& result) noexcept
    {
        result.count = 0;
        if (sorted.count == 0)
            return false;

        octaves = juce::jlimit (1, 4, octaves);

        if (mode == Chord)
        {
            const int shift = 12 * (index % octaves);
            for (int i = 0; i < sorted.count; ++i)
                result.add (sorted.n[i] + shift);
            return true;
        }

        NoteList L;
        expand (mode == Played ? played : sorted, octaves, L);
        const int n = L.count;
        if (n == 0)
            return false;

        int pos = 0;

        switch (mode)
        {
            case Up:     pos = index % n; break;
            case Down:   pos = n - 1 - index % n; break;
            case UpDown:
            {
                if (n == 1) { pos = 0; break; }
                const int period = 2 * n - 2;
                const int k = index % period;
                pos = k < n ? k : period - k;
                break;
            }
            case Played: pos = index % n; break;
            case Random: pos = randomInt (n); break;
            case Converge:
            case Diverge:
            {
                int k = index % n;
                if (mode == Diverge) k = n - 1 - k;
                pos = (k % 2 == 0) ? k / 2 : n - 1 - k / 2;
                break;
            }
            case Pinky:
            case Thumb:
            {
                if (n == 1) { pos = 0; break; }
                const int m = n - 1;
                const int k = index % (2 * m);
                if (k % 2 == 0)
                    pos = mode == Pinky ? 0 : n - 1;
                else
                    pos = mode == Pinky ? 1 + (k / 2) % m : (k / 2) % m;
                break;
            }
            case OctaveBounce:
            {
                const int j = (index / 2) % n;
                result.add (L.n[j] + ((index % 2) ? 12 : 0));
                return true;
            }
            case RandomWalk:
            default:
            {
                if (n == 1) { walkPos = 0; pos = 0; break; }
                walkPos += randomInt (2) == 0 ? -1 : 1;
                if (walkPos < 0) walkPos = 1;
                if (walkPos >= n) walkPos = n - 2;
                pos = juce::jlimit (0, n - 1, walkPos);
                break;
            }
        }

        result.add (L.n[juce::jlimit (0, n - 1, pos)]);
        return true;
    }
}
