#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
    using juce::StringArray;

    inline const StringArray& oscTypes()
    {
        static const StringArray s { "Wavetable", "Analog", "Swarm", "String", "FM", "Formant", "Noise",
                                     "Saw stack", "Pulse", "Fold", "Sync", "Vector", "Chip", "Phase dist", "Harmonic" };
        return s;
    }

    inline const char* shapeName (int type)
    {
        static const char* n[] = { "Morph", "Shape", "Spread", "Bright", "Index", "Formant", "Rate",
                                   "Detune", "Width", "Fold", "Sync", "X", "Crunch", "Warp", "Tilt" };
        return n[juce::jlimit (0, 14, type)];
    }

    inline const char* colorName (int type)
    {
        static const char* n[] = { "Table", "Width", "Density", "Sustain", "Ratio", "Focus", "Smooth",
                                   "Stack", "PWM", "Bias", "Blend", "Y", "Morph", "Reso", "Hollow" };
        return n[juce::jlimit (0, 14, type)];
    }

    inline const StringArray& filterTypes()
    {
        static const StringArray s { "Ladder", "Low pass 12", "Low pass 24", "Band pass", "High pass", "Notch", "Formant", "Comb" };
        return s;
    }

    // Key tracking is set automatically per filter type
    inline float filterKeytrack (int type)
    {
        static const float k[] = { 0.5f, 0.5f, 0.5f, 0.7f, 0.3f, 0.5f, 0.0f, 1.0f };
        return k[juce::jlimit (0, 7, type)];
    }

    inline const StringArray& lfoWaves()
    {
        static const StringArray s { "Sine", "Triangle", "Ramp up", "Ramp down", "Square", "Sample & hold", "Drift", "Stairs" };
        return s;
    }

    inline const StringArray& lfoSyncs()
    {
        static const StringArray s { "Free", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/4 T", "1/8 T" };
        return s;
    }

    inline double lfoSyncBeats (int i)
    {
        static const double b[] = { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0 };
        return b[juce::jlimit (0, 9, i)];
    }

    inline const StringArray& modSources()
    {
        static const StringArray s { "None", "LFO 1", "LFO 2", "Filter envelope", "Velocity", "Mod wheel",
                                     "Aftertouch", "Note random", "Key position" };
        return s;
    }

    inline const StringArray& modDests()
    {
        static const StringArray s { "Pitch", "Osc 1 shape", "Osc 2 shape", "Osc 3 shape", "All colors", "Cutoff",
                                     "Resonance", "Pan", "Volume", "Cross FM", "Ring mod", "LFO 1 speed",
                                     "Osc 2 level", "Osc 3 level" };
        return s;
    }

    inline const StringArray& arpModes()
    {
        static const StringArray s { "Up", "Down", "Up-down", "As played", "Random", "Converge", "Diverge",
                                     "Pinky", "Thumb", "Octave bounce", "Random walk", "Chord" };
        return s;
    }

    inline const StringArray& arpRates()
    {
        static const StringArray s { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" };
        return s;
    }

    inline double arpRateBeats (int i)
    {
        static const double b[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return b[juce::jlimit (0, 5, i)];
    }

    // X = accented hit, x = hit, . = rest. Index 10 = Custom (uses the 16 step buttons).
    inline const StringArray& rhythmNames()
    {
        static const StringArray s { "Straight", "Tresillo", "Cinquillo", "Euclid 5/16", "Euclid 7/16",
                                     "Gallop", "Offbeat", "Bossa", "Stutter", "Sparse", "Custom" };
        return s;
    }

    constexpr int customRhythm = 10;

    inline const char* rhythmPattern (int i)
    {
        static const char* p[] = { "Xxxx", "X..x..x.", "X.xx.xx.", "X..x..x..x..x...", "X..x.x.x..x.x.x.",
                                   "X.xx", ".x", "X..x..x...x..x..", "Xxx.xx.x", "X...x.x...x....." };
        return p[juce::jlimit (0, 9, i)];
    }

    inline const StringArray& clockModes()
    {
        static const StringArray s { "Follow project", "Own tempo" };
        return s;
    }

    inline const StringArray& timeSigs()
    {
        static const StringArray s { "Project", "4/4", "3/4", "5/4", "7/4", "6/8", "7/8", "9/8", "12/8", "5/8" };
        return s;
    }

    // Index 0 ("Project") is handled by the caller; this returns 4/4 for it.
    inline void timeSig (int i, int& num, int& den)
    {
        static const int n[] = { 4, 4, 3, 5, 7, 6, 7, 9, 12, 5 };
        static const int d[] = { 4, 4, 4, 4, 4, 8, 8, 8, 8, 8 };
        i = juce::jlimit (0, 9, i);
        num = n[i];
        den = d[i];
    }

    inline const StringArray& driveTypes()
    {
        static const StringArray s { "Soft", "Hard", "Fold", "Crush", "Rectify" };
        return s;
    }

    inline const StringArray& delaySyncs()
    {
        static const StringArray s { "Free", "1/4", "1/8", "1/8 dotted", "1/16", "1/4 T" };
        return s;
    }

    inline double delaySyncBeats (int i)
    {
        static const double b[] = { 0.0, 1.0, 0.5, 0.75, 0.25, 2.0 / 3.0 };
        return b[juce::jlimit (0, 5, i)];
    }

    inline double gateRateBeats (int) { return 0.25; }

    inline const StringArray& gatePatternNames()
    {
        static const StringArray s { "Pulse", "Trance", "Offbeat", "Stutter", "Broken", "Triplet" };
        return s;
    }

    inline const char* gatePattern (int i)
    {
        static const char* p[] = { "xxxxxxxxxxxxxxxx", "xx.xx.x.xx.xx.x.", ".x.x.x.x.x.x.x.x",
                                   "xxxx....xx..x...", "x.xx..x.x..xx.x.", "x..x..x..x..x..x" };
        return p[juce::jlimit (0, 5, i)];
    }

    inline juce::String osc (int index, const char* name)  { return "o" + juce::String (index + 1) + "_" + name; }
    inline juce::String slot (int index, const char* name) { return "m" + juce::String (index + 1) + "_" + name; }
    inline juce::String macro (int index)                  { return "macro" + juce::String (index + 1); }
    inline juce::String arpStep (int index)                { return "arp_step" + juce::String (index + 1); }

    // Parameters that never change when a preset is loaded
    inline bool isGlobal (const juce::String& id)
    {
        return id == "clock_mode" || id == "own_bpm" || id == "time_sig";
    }

    inline bool isArp (const juce::String& id) { return id.startsWith ("arp_"); }

    // Which on/off switch belongs to a parameter (empty if none)
    inline juce::String switchFor (const juce::String& id)
    {
        if (id == "reverb_mix" || id == "reverb_size")                                          return "reverb_on";
        if (id == "reverb_shimmer")                                                             return "shimmer_on";
        if (id == "delay_mix" || id == "delay_fb" || id == "delay_time" || id == "delay_tone")  return "delay_on";
        if (id == "chorus_mix" || id == "chorus_rate" || id == "chorus_depth")                  return "chorus_on";
        if (id == "phaser_mix" || id == "phaser_rate" || id == "phaser_depth")                  return "phaser_on";
        if (id == "tape")                                                                       return "tape_on";
        if (id == "dist_amt")                                                                   return "drive_on";
        if (id == "cloud_mix" || id == "cloud_size" || id == "cloud_density" || id == "cloud_spray") return "grains_on";
        if (id == "gate_depth")                                                                 return "gate_on";
        if (id == "wide" || id == "tremolo" || id == "tremolo_rate")                            return "stereo_on";
        if (id == "orbit" || id == "drift" || id == "entropy" || id == "gravity")               return "physics_on";
        return {};
    }

    inline bool isSwitch (const juce::String& id) { return id.endsWith ("_on") && id != "arp_on"; }

   #ifndef GLIZZARINE_NO_LAYOUT
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto skewed = [] (float lo, float hi, float centre)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (centre);
            return r;
        };

        auto addFloat = [&layout] (const String& id, const String& name,
                                   NormalisableRange<float> range, float def, const String& label = {})
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                               AudioParameterFloatAttributes().withLabel (label)));
        };

        auto addInt = [&layout] (const String& id, const String& name, int lo, int hi, int def)
        {
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
        };

        auto addBool = [&layout] (const String& id, const String& name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };

        auto addChoice = [&layout] (const String& id, const String& name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };

        const NormalisableRange<float> unit (0.0f, 1.0f);
        const NormalisableRange<float> bipolar (-1.0f, 1.0f);
        const auto timeRange = skewed (0.001f, 12.0f, 0.6f);
        const StringArray onOff { "Off", "On" };

        // Oscillators
        const int   defType[3]  = { 1, 1, 2 };
        const float defLevel[3] = { 0.8f, 0.0f, 0.0f };
        for (int i = 0; i < 3; ++i)
        {
            const String n = "Osc " + String (i + 1) + " ";
            addChoice (osc (i, "type"), n + "Model", oscTypes(), defType[i]);
            addFloat (osc (i, "shape"), n + "Shape", unit, 0.5f);
            addFloat (osc (i, "color"), n + "Color", unit, 0.4f);
            addInt (osc (i, "pitch"), n + "Pitch", -36, 36, 0);
            addFloat (osc (i, "detune"), n + "Detune", NormalisableRange<float> (-50.0f, 50.0f), 0.0f, "ct");
            addFloat (osc (i, "level"), n + "Level", unit, defLevel[i]);
        }
        addFloat ("xfm", "Cross FM", unit, 0.0f);
        addFloat ("ring", "Ring Mod", unit, 0.0f);

        // Filter
        addChoice ("filter_type", "Filter Type", filterTypes(), 0);
        addFloat ("cutoff", "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 4000.0f, "Hz");
        addFloat ("reso", "Resonance", unit, 0.2f);
        addFloat ("fenv_amt", "Filter Env Amount", bipolar, 0.3f);
        addFloat ("filter_drive", "Filter Drive", unit, 0.1f);

        // Envelopes
        addFloat ("amp_attack", "Attack", timeRange, 0.005f, "s");
        addFloat ("amp_decay", "Decay", timeRange, 0.4f, "s");
        addFloat ("amp_sustain", "Sustain", unit, 0.8f);
        addFloat ("amp_release", "Release", timeRange, 0.35f, "s");
        addFloat ("flt_attack", "Filter Attack", timeRange, 0.005f, "s");
        addFloat ("flt_decay", "Filter Decay", timeRange, 0.5f, "s");
        addFloat ("flt_sustain", "Filter Sustain", unit, 0.4f);
        addFloat ("flt_release", "Filter Release", timeRange, 0.4f, "s");

        // LFOs
        const auto rateRange = skewed (0.02f, 30.0f, 2.0f);
        addChoice ("lfo1_wave", "LFO 1 Wave", lfoWaves(), 0);
        addChoice ("lfo1_sync", "LFO 1 Sync", lfoSyncs(), 0);
        addFloat ("lfo1_rate", "LFO 1 Speed", rateRange, 5.0f, "Hz");
        addChoice ("lfo2_wave", "LFO 2 Wave", lfoWaves(), 1);
        addChoice ("lfo2_sync", "LFO 2 Sync", lfoSyncs(), 0);
        addFloat ("lfo2_rate", "LFO 2 Speed", rateRange, 0.3f, "Hz");

        for (int i = 0; i < 4; ++i)
        {
            const String n = "Mod " + String (i + 1) + " ";
            addChoice (slot (i, "src"), n + "Source", modSources(), 0);
            addChoice (slot (i, "dst"), n + "Target", modDests(), 0);
            addFloat (slot (i, "amt"), n + "Amount", bipolar, 0.0f);
        }

        // Playing
        addFloat ("vib_wheel", "Mod Wheel Vibrato", unit, 0.5f);
        addFloat ("vib_at", "Aftertouch Vibrato", unit, 0.3f);
        addFloat ("at_cutoff", "Aftertouch Filter", unit, 0.0f);
        addChoice ("mode", "Voice Mode", StringArray { "Mono", "Legato", "Poly" }, 2);
        addFloat ("glide", "Glide", skewed (0.0f, 2.0f, 0.2f), 0.0f, "s");

        // Strange physics
        addFloat ("gravity", "Gravity", bipolar, 0.0f);
        addFloat ("entropy", "Entropy", unit, 0.0f);
        addFloat ("drift", "Drift", unit, 0.0f);
        addFloat ("orbit", "Orbit", unit, 0.0f);

        // Arpeggiator
        addChoice ("arp_on", "Arp", onOff, 0);
        addChoice ("arp_lock", "Arp Lock", onOff, 0);
        addChoice ("arp_mode", "Arp Pattern", arpModes(), 0);
        addChoice ("arp_speed", "Arp Speed", StringArray { "1x", "2x", "4x" }, 0);
        addChoice ("arp_rhythm", "Arp Rhythm", rhythmNames(), 0);
        addInt ("arp_octaves", "Arp Octaves", 1, 4, 1);
        addFloat ("arp_gate", "Arp Length", NormalisableRange<float> (0.05f, 1.5f), 0.6f);
        addFloat ("arp_swing", "Arp Swing", unit, 0.0f);
        addFloat ("arp_mutate", "Arp Mutation", unit, 0.0f);
        addFloat ("arp_glitch", "Arp Glitch", unit, 0.0f);
        addChoice ("arp_hold", "Arp Hold", onOff, 0);
        const char* defSteps = "X.x.X.x.x.X.x.x.";
        for (int i = 0; i < 16; ++i)
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { arpStep (i), 1 }, "Arp Step " + String (i + 1), defSteps[i] != '.'));

        // Clock
        addChoice ("clock_mode", "Tempo Source", clockModes(), 0);
        addFloat ("own_bpm", "Own Tempo", NormalisableRange<float> (40.0f, 240.0f, 0.1f), 100.0f, "BPM");
        addChoice ("time_sig", "Time Signature", timeSigs(), 0);

        // On/off switches
        for (auto* id : { "physics_on", "drive_on", "tape_on", "chorus_on", "phaser_on", "delay_on",
                          "grains_on", "reverb_on", "shimmer_on", "gate_on", "stereo_on" })
            addBool (id, String (id).upToFirstOccurrenceOf ("_", false, false) + " on", false);

        // Effects
        addChoice ("dist_type", "Drive Type", driveTypes(), 0);
        addFloat ("dist_amt", "Drive", unit, 0.0f);
        addFloat ("tape", "Tape", unit, 0.0f);
        addChoice ("chorus_type", "Chorus Type", StringArray { "Classic", "Ensemble", "Rotary" }, 0);
        addFloat ("chorus_mix", "Chorus", unit, 0.0f);
        addFloat ("chorus_rate", "Chorus Speed", skewed (0.05f, 8.0f, 1.0f), 0.6f, "Hz");
        addFloat ("chorus_depth", "Chorus Depth", unit, 0.5f);
        addFloat ("phaser_mix", "Phaser", unit, 0.0f);
        addFloat ("phaser_rate", "Phaser Speed", skewed (0.02f, 10.0f, 1.0f), 0.25f, "Hz");
        addFloat ("phaser_depth", "Phaser Depth", unit, 0.7f);
        addChoice ("delay_sync", "Delay Sync", delaySyncs(), 3);
        addFloat ("delay_time", "Delay Time", skewed (0.02f, 1.5f, 0.35f), 0.3f, "s");
        addFloat ("delay_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.35f);
        addFloat ("delay_mix", "Delay", unit, 0.0f);
        addFloat ("delay_tone", "Delay Tone", unit, 0.55f);
        addFloat ("cloud_size", "Grain Size", unit, 0.4f);
        addFloat ("cloud_mix", "Grains", unit, 0.0f);
        addFloat ("cloud_density", "Grain Density", unit, 0.5f);
        addFloat ("cloud_spray", "Grain Spray", unit, 0.35f);
        addFloat ("reverb_size", "Reverb Size", unit, 0.6f);
        addFloat ("reverb_shimmer", "Shimmer", unit, 0.0f);
        addFloat ("reverb_mix", "Reverb", unit, 0.15f);
        addChoice ("gate_pattern", "Gate Pattern", gatePatternNames(), 1);
        addFloat ("gate_depth", "Gate", unit, 0.0f);
        addFloat ("wide", "Wide", unit, 0.2f);
        addFloat ("tremolo", "Tremolo", unit, 0.0f);
        addFloat ("tremolo_rate", "Tremolo Speed", skewed (0.1f, 12.0f, 3.0f), 4.5f, "Hz");
        addFloat ("master", "Volume", NormalisableRange<float> (0.0f, 1.5f), 0.7f);


        return layout;
    }
   #endif
}
