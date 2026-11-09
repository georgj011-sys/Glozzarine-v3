#pragma once

#include "Voice.h"
#include "FX.h"
#include "Params.h"

// Turns parameter values into engine settings. "get" returns the current
// value of a parameter by ID. Shared by the plugin and the offline preset checker.
template <typename Getter>
void mapParams (Getter&& get, VoiceParams& vp, FxParams& fp, double bpm, float modWheel = 0.0f)
{
    auto geti = [&get] (const juce::String& id) { return (int) std::lround (get (id)); };

    for (int i = 0; i < 3; ++i)
    {
        auto& o = vp.osc[i];
        o.type   = geti (Params::osc (i, "type"));
        o.shape  = get (Params::osc (i, "shape"));
        o.color  = get (Params::osc (i, "color"));
        o.pitch  = get (Params::osc (i, "pitch"));
        o.detune = get (Params::osc (i, "detune"));
        o.level  = get (Params::osc (i, "level"));
    }

    vp.xfm        = get ("xfm");
    vp.ring       = get ("ring");
    vp.filterType = geti ("filter_type");
    vp.cutoff     = get ("cutoff");
    vp.reso       = get ("reso");
    vp.envAmt     = get ("fenv_amt");
    vp.keytrack   = Params::filterKeytrack (vp.filterType);
    vp.drive      = get ("filter_drive");
    vp.ampEnv     = { get ("amp_attack"), get ("amp_decay"), get ("amp_sustain"), get ("amp_release") };
    vp.fltEnv     = { get ("flt_attack"), get ("flt_decay"), get ("flt_sustain"), get ("flt_release") };

    for (int i = 0; i < 2; ++i)
    {
        const juce::String pre = "lfo" + juce::String (i + 1) + "_";
        vp.lfoWave[i] = geti (pre + "wave");
        const double beats = Params::lfoSyncBeats (geti (pre + "sync"));
        vp.lfoRate[i] = beats > 0.0 ? (float) (bpm / 60.0 / beats) : get (pre + "rate");
        vp.lfoRetrig[i] = beats > 0.0;
    }

    for (int i = 0; i < 4; ++i)
    {
        vp.slots[i].src = geti (Params::slot (i, "src"));
        vp.slots[i].dst = geti (Params::slot (i, "dst"));
        vp.slots[i].amt = get (Params::slot (i, "amt"));
    }

    vp.vibWheel = get ("vib_wheel");
    vp.vibAt    = get ("vib_at");
    vp.atCutoff = get ("at_cutoff");
    const bool physics = get ("physics_on") > 0.5f;
    vp.gravity  = physics ? get ("gravity") : 0.0f;
    vp.entropy  = physics ? get ("entropy") : 0.0f;
    vp.drift    = physics ? get ("drift") : 0.0f;
    vp.orbit    = physics ? get ("orbit") : 0.0f;
    vp.glide    = get ("glide");

    auto on = [&get] (const char* id) { return get (id) > 0.5f; };
    const float tape = on ("tape_on") ? get ("tape") : 0.0f;
    const int chorusType = geti ("chorus_type");
    const bool stereo = on ("stereo_on");
    const float wide = stereo ? get ("wide") : 0.0f;

    fp.distType      = geti ("dist_type");
    fp.distAmt       = on ("drive_on") ? get ("dist_amt") : 0.0f;
    fp.tapeWow       = tape * 0.6f;
    fp.tapeFlutter   = tape * 0.3f;
    fp.tapeAge       = tape * 0.7f;
    fp.chorusType    = chorusType;
    fp.chorusRate    = get ("chorus_rate");
    fp.chorusDepth   = get ("chorus_depth");
    fp.chorusMix     = on ("chorus_on") ? get ("chorus_mix") : 0.0f;
    fp.phaserRate    = get ("phaser_rate");
    fp.phaserDepth   = get ("phaser_depth");
    fp.phaserMix     = on ("phaser_on") ? get ("phaser_mix") : 0.0f;
    fp.delaySync     = geti ("delay_sync");
    fp.delayTime     = get ("delay_time");
    fp.delayFb       = get ("delay_fb");
    fp.delayTone     = get ("delay_tone");
    fp.delayMix      = on ("delay_on") ? get ("delay_mix") : 0.0f;
    fp.cloudSize     = get ("cloud_size");
    fp.cloudDensity  = get ("cloud_density");
    fp.cloudSpray    = get ("cloud_spray");
    fp.cloudMix      = on ("grains_on") ? get ("cloud_mix") : 0.0f;
    fp.reverbSize    = get ("reverb_size");
    fp.reverbShimmer = on ("shimmer_on") ? get ("reverb_shimmer") : 0.0f;
    fp.reverbMix     = on ("reverb_on") ? get ("reverb_mix") : 0.0f;
    fp.gatePattern   = geti ("gate_pattern");
    fp.gateDepth     = on ("gate_on") ? get ("gate_depth") : 0.0f;
    fp.panRate       = get ("tremolo_rate");
    fp.panDepth      = stereo ? get ("tremolo") : 0.0f;
    fp.haasMs        = wide * 14.0f;
    fp.width         = 1.0f + wide * 0.8f;
    fp.master        = get ("master");
    fp.bpm           = bpm;
    fp.modWheel      = modWheel;
}
