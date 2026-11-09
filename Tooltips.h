#pragma once

#include <juce_core/juce_core.h>

// Short plain-language help shown when hovering over a control.
namespace Tooltips
{
    inline juce::String get (const juce::String& id)
    {
        if (id.startsWith ("macro"))
            return "Macro: one knob that moves several settings at once, tailored to this preset.";

        if (id.length() > 3 && id[0] == 'o' && id[2] == '_')
        {
            const auto part = id.substring (3);
            if (part == "type")   return "Sound source. Each model has its own character.";
            if (part == "shape")  return "Main character control. Its meaning depends on the model (see label).";
            if (part == "color")  return "Second character control. Its meaning depends on the model (see label).";
            if (part == "pitch")  return "Pitch in semitones. 12 = one octave up, 7 = a fifth.";
            if (part == "detune") return "Slight detuning. Small amounts sound wider and warmer.";
            if (part == "level")  return "Volume of this oscillator.";
        }

        if (id.length() > 3 && id[0] == 'm' && id[2] == '_')
        {
            const auto part = id.substring (3);
            if (part == "src") return "What does the moving: an LFO, the mod wheel, aftertouch and so on.";
            if (part == "dst") return "What gets moved.";
            if (part == "amt") return "How strongly. Negative values move it the other way.";
        }

        if (id.startsWith ("arp_step"))
            return "Click to switch this step on or off. Used when the rhythm is set to Custom.";

        static const std::pair<const char*, const char*> table[] = {
            { "xfm", "Oscillator 3 bends oscillator 1 very fast. Metallic, growling tones." },
            { "ring", "Multiplies oscillators 1 and 2. Bell-like and robotic tones." },
            { "filter_type", "Filter model. Ladder is warm and creamy like classic mono synths." },
            { "cutoff", "Brightness. Lower = darker and softer." },
            { "reso", "Emphasises the filter edge. High values whistle and sing." },
            { "fenv_amt", "How much the filter opens for each note." },
            { "filter_drive", "Gentle saturation before the filter. Warmth and grit." },
            { "amp_attack", "How slowly the sound fades in." },
            { "amp_decay", "How quickly the sound drops to the sustain level." },
            { "amp_sustain", "Volume while you hold the key." },
            { "amp_release", "How long the sound rings out after you let go." },
            { "flt_attack", "How slowly the filter opens." },
            { "flt_decay", "How quickly the filter closes again." },
            { "flt_sustain", "How open the filter stays while you hold the key." },
            { "flt_release", "How the filter moves after you let go." },
            { "lfo1_wave", "Shape of the automatic movement." },
            { "lfo1_sync", "Lock to the tempo, or leave free." },
            { "lfo1_rate", "Speed when not locked to the tempo." },
            { "lfo2_wave", "Shape of the automatic movement." },
            { "lfo2_sync", "Lock to the tempo. When locked, it restarts with every note (great for risers)." },
            { "lfo2_rate", "Speed when not locked to the tempo." },
            { "vib_wheel", "How much vibrato the mod wheel adds." },
            { "vib_at", "How much vibrato pressing harder adds (aftertouch)." },
            { "at_cutoff", "Pressing harder opens the filter (aftertouch)." },
            { "mode", "Poly plays chords. Mono plays one note. Legato glides only between connected notes." },
            { "glide", "How long the pitch slides from note to note." },
            { "gravity", "Notes slowly sink (left) or rise (right) while held." },
            { "entropy", "Every note comes out slightly different." },
            { "drift", "Unstable tuning like an old analog synth." },
            { "orbit", "The oscillators circle around the stereo field." },
            { "arp_on", "Turns held chords into patterns." },
            { "arp_lock", "Keeps the arpeggiator and its settings when you switch presets." },
            { "arp_mode", "Order in which your notes are played." },
            { "arp_speed", "1x = one step per beat of the bar (7/8 = 7 steps). 2x and 4x split every beat for faster patterns." },
            { "arp_rhythm", "Rhythm with rests and accents. Custom uses the 16 step buttons." },
            { "arp_octaves", "How many octaves the pattern climbs." },
            { "arp_gate", "Note length. Short = staccato, long = connected." },
            { "arp_swing", "Delays every second step for a shuffled groove." },
            { "arp_mutate", "The pattern slowly changes by itself." },
            { "arp_glitch", "Steps stutter, chop, drop out, jump or swell." },
            { "arp_hold", "Keeps playing after you let go. The sustain pedal does the same." },
            { "clock_mode", "Follow the project tempo, or run on your own tapped tempo." },
            { "own_bpm", "Own tempo in BPM. Use the Tap button to set it." },
            { "time_sig", "Time signature for the arpeggiator: one step per beat (7/8 = 7 steps), the 1 is accented. Project follows your DAW." },
            { "dist_type", "Type of distortion." },
            { "dist_amt", "Amount of distortion." },
            { "tape", "Old tape: wobble, warmth and a little hiss." },
            { "chorus_type", "Classic chorus, the lush ensemble of 70s string machines, or a rotary speaker (mod wheel = fast)." },
            { "physics_on", "Switches all strange physics on or off." },
            { "drive_on", "Switches the drive on or off." },
            { "tape_on", "Switches the tape effect on or off." },
            { "chorus_on", "Switches the chorus on or off." },
            { "phaser_on", "Switches the phaser on or off." },
            { "delay_on", "Switches the delay on or off." },
            { "grains_on", "Switches the grains on or off." },
            { "reverb_on", "Switches the reverb on or off." },
            { "shimmer_on", "Switches the shimmer on or off." },
            { "gate_on", "Switches the rhythm gate on or off." },
            { "stereo_on", "Switches width and tremolo on or off." },
            { "chorus_mix", "Chorus amount." },
            { "phaser_mix", "Slow, sweeping phaser." },
            { "delay_sync", "Echo locked to the tempo, or Free (use the time knob)." },
            { "delay_time", "Echo time when sync is Free." },
            { "delay_fb", "How many echoes repeat." },
            { "delay_mix", "Echo amount." },
            { "cloud_size", "Length of the tiny sound grains." },
            { "cloud_mix", "Granular cloud amount." },
            { "reverb_size", "Room size." },
            { "reverb_shimmer", "Sparkling octave-up trails. Works on its own or together with the reverb." },
            { "reverb_mix", "Reverb amount." },
            { "gate_pattern", "Rhythm of the volume chopping (16ths)." },
            { "gate_depth", "How strongly the gate chops the sound." },
            { "wide", "Makes the sound wider." },
            { "tremolo", "Gentle left-right movement, like a classic electric piano." },
            { "master", "Overall volume." },
        };

        for (const auto& t : table)
            if (id == t.first)
                return t.second;

        return {};
    }
}
