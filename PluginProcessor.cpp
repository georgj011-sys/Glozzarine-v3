#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <cstring>

namespace
{
    const char* wizardTargets[GlizzarineProcessor::numWizardSteps] = {
        "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8",
        "o1_level", "o2_level", "o3_level", "amp_attack", "amp_release", "delay_mix", "reverb_mix", "master"
    };
}

GlizzarineProcessor::GlizzarineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", Params::createLayout())
{
    for (auto* p : getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            paramIndex[rp->paramID.toStdString()] = (int) params.size();
            params.push_back (rp);
        }
    }

    // Parameters that belong to an effect switch: turning them up switches the effect on
    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto sw = Params::switchFor (params[i]->paramID);
        if (sw.isNotEmpty())
        {
            const auto it = paramIndex.find (sw.toStdString());
            if (it != paramIndex.end())
                switched.push_back ({ (int) i, it->second, params[i]->getValue() });
        }
    }

    for (int i = 0; i < 128; ++i)
    {
        ccValue[(size_t) i].store (0.0f);
        ccDirty[(size_t) i].store (false);
        ccLast[(size_t) i] = -1.0f;
    }

    noteStack.ensureStorageAllocated (128);
    physicalHeld.ensureStorageAllocated (128);
    sustained.ensureStorageAllocated (128);
    arpNotes.ensureStorageAllocated (128);

    loadFavourites();
    scanUserPresets();
    loadControllerMap();
    loadFactory (startupPresetIndex());
    undoManager.clearUndoHistory();

    startTimerHz (30);
}

GlizzarineProcessor::~GlizzarineProcessor()
{
    stopTimer();
}

//==============================================================================
float GlizzarineProcessor::valueOf (const juce::String& id) const
{
    const auto it = paramIndex.find (id.toStdString());
    if (it == paramIndex.end())
        return 0.0f;

    auto* p = params[(size_t) it->second];
    return p->convertFrom0to1 (p->getValue());
}

// The 8 Play knobs control real parameters directly; this returns the parameter behind knob n
juce::String GlizzarineProcessor::playKnobParam (int n) const
{
    const auto* id = getMacroTemplate (macroTemplate.load()).macros[juce::jlimit (0, 7, n)].targets[0].id;
    return id != nullptr ? juce::String (id) : juce::String();
}

// When a knob of an effect that is switched off gets turned up, switch that effect on
void GlizzarineProcessor::autoSwitchEffects()
{
    for (auto& w : switched)
    {
        const float v = params[(size_t) w.param]->getValue();
        if (v > w.last + 1.0e-4f && params[(size_t) w.sw]->getValue() < 0.5f)
            params[(size_t) w.sw]->setValueNotifyingHost (1.0f);
        w.last = v;
    }
}

void GlizzarineProcessor::syncSwitched()
{
    for (auto& w : switched)
        w.last = params[(size_t) w.param]->getValue();
}

void GlizzarineProcessor::setParam (const juce::String& id, float realValue)
{
    const auto it = paramIndex.find (id.toStdString());
    if (it == paramIndex.end())
        return;
    auto* p = params[(size_t) it->second];
    p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

//==============================================================================
void GlizzarineProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    for (auto& voice : voices)
        voice.prepare (sr, &(*waveBank));

    fx.prepare (sr, samplesPerBlock);

    noteStack.clearQuick();
    physicalHeld.clearQuick();
    sustained.clearQuick();
    arpNotes.clearQuick();
    pendingCount = 0;
    arpRunning = false;
    lastNote = -1;
    clock = 0.0;
}

bool GlizzarineProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GlizzarineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Tempo and time signature
    double ppq = 0.0;
    bool hasPpq = false;
    const bool ownClock = intOf ("clock_mode") == 1;

    int hostNum = 4, hostDen = 4;

    if (ownClock)
    {
        bpm = juce::jlimit (40.0, 240.0, (double) valueOf ("own_bpm"));
        ppq = ownBeat;
        hasPpq = true;
        ownBeat += numSamples * bpm / 60.0 / sampleRate;
    }
    else if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto ts = pos->getTimeSignature())
            {
                hostNum = juce::jmax (1, ts->numerator);
                hostDen = juce::jmax (1, ts->denominator);
            }
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition())
                {
                    ppq = *q;
                    hasPpq = true;
                }
        }
    }

    // Time signature: "Project" follows the host, anything else is fixed
    const int tsChoice = intOf ("time_sig");
    if (tsChoice == 0) { tsNum = hostNum; tsDen = hostDen; }
    else               Params::timeSig (tsChoice, tsNum, tsDen);
    arpBarSteps.store (juce::jlimit (1, 48, tsNum * (1 << juce::jlimit (0, 2, intOf ("arp_speed")))));

    VoiceParams vp;
    FxParams fp;
    mapParams ([this] (const juce::String& id) { return valueOf (id); }, vp, fp, bpm, modWheel);
    fp.ppq = ppq;
    fp.hasPpq = hasPpq;

    mode = juce::jlimit (0, 2, intOf ("mode"));
    if (mode != lastMode)
    {
        if (lastMode != -1)
        {
            engineReleaseAll();
            for (size_t i = 1; i < voices.size(); ++i)
                voices[i].kill();
        }
        lastMode = mode;
    }

    blockStart = clock;
    arpOn = intOf ("arp_on") == 1;
    arpHoldParam = intOf ("arp_hold") == 1;

    if (arpOn != arpWasOn)
    {
        stopArp();
        engineReleaseAll();
        if (arpOn)
        {
            arpNotes = physicalHeld;
            if (! arpNotes.isEmpty())
                startArp (blockStart);
        }
        arpWasOn = arpOn;
    }

    if (arpOn && ! arpLatched() && ! arpNotes.isEmpty())
    {
        for (int i = arpNotes.size(); --i >= 0;)
            if (! physicalHeld.contains (arpNotes[i]))
                arpNotes.remove (i);
        if (arpNotes.isEmpty())
            stopArp();
    }

    if (buffer.getNumChannels() < 2)
    {
        clock += numSamples;
        return;
    }

    outL = buffer.getWritePointer (0);
    outR = buffer.getWritePointer (1);

    int cur = 0;
    for (const auto metadata : midi)
    {
        const int t = juce::jlimit (0, numSamples, metadata.samplePosition);
        advanceTo (cur, t, vp);
        handleMidi (metadata.getMessage(), blockStart + t, vp);
    }
    advanceTo (cur, numSamples, vp);

    clock += numSamples;
    fx.process (buffer, fp);

    // Feed the on-screen scope
    int w = scopeWrite.load();
    for (int i = 0; i < numSamples; i += 2)
    {
        scope[(size_t) w] = 0.5f * (outL[i] + outR[i]);
        w = (w + 1) & 2047;
    }
    scopeWrite.store (w);
}

void GlizzarineProcessor::advanceTo (int& cur, int target, const VoiceParams& vp)
{
    const double absTarget = blockStart + target;

    for (int guard = 0; guard < 4096; ++guard)
    {
        double next = 1.0e300;
        int which = -2;

        if (arpRunning)
        {
            next = arpNext;
            which = -1;
        }

        for (int k = 0; k < pendingCount; ++k)
            if (pending[(size_t) k].time < next)
            {
                next = pending[(size_t) k].time;
                which = k;
            }

        if (which == -2 || next >= absTarget)
            break;

        const int off = juce::jlimit (cur, target, (int) std::floor (next - blockStart));
        renderVoices (cur, off - cur, vp);
        cur = off;

        if (which == -1)
        {
            doArpStep (arpNext, vp);
        }
        else
        {
            const Pending ev = pending[(size_t) which];
            pending[(size_t) which] = pending[(size_t) (--pendingCount)];
            if (ev.on) engineNoteOn (ev.note, ev.vel, vp, ev.swell);
            else       engineNoteOff (ev.note);
        }
    }

    renderVoices (cur, target - cur, vp);
    cur = target;
}

void GlizzarineProcessor::renderVoices (int start, int num, const VoiceParams& vp)
{
    if (num <= 0)
        return;

    const float bendSemis = bend * 2.0f;
    const int count = mode == Poly ? (int) voices.size() : 1;

    for (int i = 0; i < count; ++i)
        voices[(size_t) i].render (outL, outR, start, num, vp, bendSemis, modWheel, aftertouch);
}

void GlizzarineProcessor::handleMidi (const juce::MidiMessage& msg, double absTime, const VoiceParams& vp)
{
    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();

        if (arpOn)
        {
            if (arpLatched() && physicalHeld.isEmpty())
                arpNotes.clearQuick();

            physicalHeld.addIfNotAlreadyThere (n);
            if (! arpNotes.contains (n) && arpNotes.size() < Arp::maxHeld)
                arpNotes.add (n);

            if (! arpRunning && ! arpNotes.isEmpty())
                startArp (absTime);
        }
        else
        {
            physicalHeld.addIfNotAlreadyThere (n);
            sustained.removeAllInstancesOf (n);
            engineNoteOn (n, msg.getFloatVelocity(), vp);
        }
    }
    else if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        physicalHeld.removeAllInstancesOf (n);

        if (arpOn)
        {
            if (! arpLatched())
            {
                arpNotes.removeAllInstancesOf (n);
                if (arpNotes.isEmpty())
                    stopArp();
            }
        }
        else if (pedalDown)
        {
            sustained.addIfNotAlreadyThere (n);
        }
        else
        {
            engineNoteOff (n);
        }
    }
    else if (msg.isPitchWheel())
    {
        bend = ((float) msg.getPitchWheelValue() - 8192.0f) / 8192.0f;
    }
    else if (msg.isChannelPressure())
    {
        aftertouch = (float) msg.getChannelPressureValue() / 127.0f;
    }
    else if (msg.isAftertouch())
    {
        aftertouch = (float) msg.getAfterTouchValue() / 127.0f;
    }
    else if (msg.isProgramChange())
    {
        pendingProgram.store (msg.getProgramChangeNumber());
    }
    else if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        if (cc == 1)
        {
            modWheel = (float) val / 127.0f;
        }
        else if (cc == 64)
        {
            const bool down = val >= 64;
            if (pedalDown && ! down)
            {
                if (arpOn)
                {
                    if (! arpHoldParam)
                    {
                        for (int i = arpNotes.size(); --i >= 0;)
                            if (! physicalHeld.contains (arpNotes[i]))
                                arpNotes.remove (i);
                        if (arpNotes.isEmpty())
                            stopArp();
                    }
                }
                else
                {
                    for (auto n : sustained)
                        if (! physicalHeld.contains (n))
                            engineNoteOff (n);
                    sustained.clearQuick();
                }
            }
            pedalDown = down;
        }
        else if (cc >= 120)
        {
            if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                physicalHeld.clearQuick();
                sustained.clearQuick();
                arpNotes.clearQuick();
                stopArp();
                engineReleaseAll();
            }
        }
        else
        {
            ccValue[(size_t) cc].store ((float) val / 127.0f);
            ccDirty[(size_t) cc].store (true);
            lastCC.store (cc);
        }
    }
}

//==============================================================================
void GlizzarineProcessor::startArp (double time)
{
    arpRunning = true;
    arpStep = arpNoteIndex = walkPos = 0;
    arpGrid = arpNext = time;
    mutRest.fill (false);
    mutOct.fill (0);
}

void GlizzarineProcessor::doArpStep (double time, const VoiceParams& vp)
{
    if (arpNotes.isEmpty())
    {
        stopArp();
        return;
    }

    Arp::NoteList sorted, played;
    for (int i = 0; i < arpNotes.size() && i < Arp::maxHeld; ++i)
    {
        played.add (arpNotes[i]);
        sorted.add (arpNotes[i]);
    }
    std::sort (sorted.n, sorted.n + sorted.count);

    // One step per beat of the time signature (7/8 = 7 eighths); Speed 2x / 4x subdivides each beat
    const int speed = 1 << juce::jlimit (0, 2, intOf ("arp_speed"));
    const double stepBeats = (4.0 / (double) tsDen) / (double) speed;
    const double stepLen = stepBeats * 60.0 / bpm * sampleRate;
    const int stepsPerBar = juce::jlimit (1, 48, tsNum * speed);
    const int posInBar = arpStep % stepsPerBar;
    const int idx16 = posInBar % 16;

    // Rhythm: built-in pattern or the 16 custom step buttons
    const int rhythm = intOf ("arp_rhythm");
    bool hit, accent;
    if (rhythm == Params::customRhythm)
    {
        hit = valueOf (Params::arpStep (idx16)) > 0.5f;
        accent = false;
    }
    else
    {
        const char* pattern = Params::rhythmPattern (rhythm);
        const int len = juce::jmax (1, (int) std::strlen (pattern));
        const int idx = posInBar % len;
        hit = pattern[idx] != '.';
        accent = pattern[idx] == 'X';
    }
    accent = accent || posInBar == 0;
    arpDisplayStep.store (posInBar);

    // Mutation: the pattern slowly rewrites itself
    const float mutate = valueOf ("arp_mutate");
    if (mutate > 0.001f && arpRng.nextFloat() < mutate * 0.15f)
    {
        const int k = arpRng.nextInt (16);
        const int action = arpRng.nextInt (3);
        int rests = 0;
        for (auto r : mutRest) rests += r ? 1 : 0;
        if (action == 0 && (mutRest[(size_t) k] || rests < 10))
            mutRest[(size_t) k] = ! mutRest[(size_t) k];
        else if (action == 1)
        {
            static constexpr int shifts[4] = { -12, 0, 12, 7 };
            mutOct[(size_t) k] = shifts[arpRng.nextInt (4)];
        }
        else
        {
            mutRest[(size_t) k] = false;
            mutOct[(size_t) k] = 0;
        }
    }

    if (hit && ! mutRest[(size_t) idx16])
    {
        Arp::NoteList result;
        const bool ok = Arp::pick (intOf ("arp_mode"), sorted, played, intOf ("arp_octaves"), arpNoteIndex, walkPos,
                                   [this] (int m) { return arpRng.nextInt (juce::jmax (1, m)); }, result);
        if (ok)
        {
            ++arpNoteIndex;

            float vel = accent ? 1.0f : 0.65f;
            int shift = mutOct[(size_t) idx16];
            int count = 1;
            double gateFactor = (double) valueOf ("arp_gate");
            float swell = 0.0f;
            bool drop = false;

            // Glitch: stutter, chop, drop, jump or swell
            const float glitch = valueOf ("arp_glitch");
            if (glitch > 0.001f && arpRng.nextFloat() < glitch * 0.45f)
            {
                switch (arpRng.nextInt (5))
                {
                    case 0:  count = arpRng.nextBool() ? 3 : 4; gateFactor = 0.5; break;
                    case 1:  gateFactor *= 0.15; break;
                    case 2:  drop = true; break;
                    case 3:
                    {
                        static constexpr int jumps[3] = { 12, -12, 7 };
                        shift += jumps[arpRng.nextInt (3)];
                        break;
                    }
                    default:
                        swell = (float) (stepLen * 1.5 / sampleRate);
                        gateFactor = juce::jmax (gateFactor, 1.2);
                        vel *= 0.9f;
                        break;
                }
            }

            if (! drop)
            {
                const double sub = stepLen / count;
                const double gateLen = juce::jmax (0.002 * sampleRate, gateFactor * sub);

                for (int r = 0; r < count; ++r)
                {
                    const double tOn = time + r * sub;
                    for (int j = 0; j < result.count; ++j)
                    {
                        const int note = juce::jlimit (0, 127, result.n[j] + shift);
                        if (r == 0) engineNoteOn (note, vel, vp, swell);
                        else        schedule (tOn, note, vel * 0.85f, true);
                        schedule (tOn + gateLen, note, 0.0f, false);
                    }
                }
            }
        }
    }

    ++arpStep;
    arpGrid += stepLen;
    arpNext = arpGrid + ((arpStep % 2 == 1) ? (double) valueOf ("arp_swing") * 0.33 * stepLen : 0.0);
}

void GlizzarineProcessor::schedule (double time, int note, float vel, bool on, float swell)
{
    if (pendingCount < (int) pending.size())
        pending[(size_t) (pendingCount++)] = { time, note, vel, on, swell };
}

void GlizzarineProcessor::stopArp()
{
    arpRunning = false;
    for (int k = 0; k < pendingCount; ++k)
        if (! pending[(size_t) k].on)
            engineNoteOff (pending[(size_t) k].note);
    pendingCount = 0;
    arpDisplayStep.store (-1);
}

//==============================================================================
void GlizzarineProcessor::engineNoteOn (int note, float velocity, const VoiceParams& vp, float swell)
{
    static constexpr float spreadSeq[8] = { -1.0f, 1.0f, -0.5f, 0.5f, -0.75f, 0.75f, -0.25f, 0.25f };

    noteStack.removeAllInstancesOf (note);
    noteStack.add (note);

    if (mode == Poly)
    {
        GlizzarineVoice* target = nullptr;

        for (auto& voice : voices)
            if (voice.isActive() && voice.getNote() == note)
                target = &voice;

        if (target == nullptr)
            for (auto& voice : voices)
                if (! voice.isActive()) { target = &voice; break; }

        if (target == nullptr)
        {
            for (auto& voice : voices)
            {
                if (target == nullptr) { target = &voice; continue; }
                const bool better = (! voice.isGateOn() && target->isGateOn())
                                 || (voice.isGateOn() == target->isGateOn() && voice.age < target->age);
                if (better)
                    target = &voice;
            }
        }

        const float from = (vp.glide > 0.0005f && lastNote >= 0) ? (float) lastNote : (float) note;
        target->start (note, velocity, from, vp, ++seed, spreadSeq[(spreadCounter++) & 7], swell);
        target->age = ++ageCounter;
    }
    else
    {
        auto& voice = voices[0];

        if (mode == Legato && noteStack.size() > 1 && voice.isActive() && voice.isGateOn())
        {
            voice.glideTo (note);
        }
        else
        {
            float from = (float) note;
            if (mode == Mono)
                from = voice.isActive() ? voice.getPitch() : (lastNote >= 0 ? (float) lastNote : (float) note);
            voice.start (note, velocity, from, vp, ++seed, 0.0f, swell);
        }
    }

    lastNote = note;
}

void GlizzarineProcessor::engineNoteOff (int note)
{
    const bool wasTop = ! noteStack.isEmpty() && noteStack.getLast() == note;
    noteStack.removeAllInstancesOf (note);

    if (mode == Poly)
    {
        for (auto& voice : voices)
            if (voice.isGateOn() && voice.getNote() == note)
                voice.release();
        return;
    }

    auto& voice = voices[0];

    if (noteStack.isEmpty())
    {
        if (voice.isGateOn())
            voice.release();
    }
    else if (wasTop && voice.isGateOn())
    {
        const int previous = noteStack.getLast();
        voice.glideTo (previous);
        lastNote = previous;
    }
}

void GlizzarineProcessor::engineReleaseAll()
{
    for (auto& voice : voices)
        if (voice.isGateOn())
            voice.release();
    noteStack.clearQuick();
}

//==============================================================================
// Presets
juce::File GlizzarineProcessor::dataDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Glizzarine");
    dir.createDirectory();
    return dir;
}

void GlizzarineProcessor::scanUserPresets()
{
    userPresets.clear();
    auto dir = dataDir().getChildFile ("Presets");
    dir.createDirectory();

    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
        userPresets.push_back ({ f.getFileNameWithoutExtension(), f });

    std::sort (userPresets.begin(), userPresets.end(),
               [] (const UserPreset& a, const UserPreset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

void GlizzarineProcessor::loadFavourites()
{
    favourites.clear();
    const auto f = dataDir().getChildFile ("favourites.txt");
    if (f.existsAsFile())
        favourites.addLines (f.loadFileAsString());
    favourites.removeEmptyStrings();
}

void GlizzarineProcessor::saveFavourites()
{
    dataDir().getChildFile ("favourites.txt").replaceWithText (favourites.joinIntoString ("\n"));
}

void GlizzarineProcessor::toggleFavourite()
{
    if (favourites.contains (presetName))
        favourites.removeString (presetName);
    else
        favourites.add (presetName);
    saveFavourites();
    ++presetVersion;
}

void GlizzarineProcessor::applyValues (const std::vector<std::pair<const char*, float>>& values)
{
    const bool keepArp = intOf ("arp_lock") == 1;

    for (auto* p : params)
    {
        const auto& id = p->paramID;
        if (Params::isGlobal (id) || (keepArp && Params::isArp (id)))
            continue;

        float normalised = p->getDefaultValue();
        for (const auto& kv : values)
            if (id == kv.first)
            {
                normalised = p->convertTo0to1 (kv.second);
                break;
            }
        p->setValueNotifyingHost (normalised);
    }
}

void GlizzarineProcessor::afterPresetLoad()
{
    syncSwitched();
    for (auto& c : ccCaught) c = false;
    ++presetVersion;
}

void GlizzarineProcessor::loadFactory (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    undoManager.beginNewTransaction ("Load preset");
    const auto& preset = presets[(size_t) index];
    applyValues (preset.values);
    macroTemplate.store (preset.macroTemplate);
    presetName = preset.name;
    presetCategory = preset.category;
    currentFactory = index;
    currentUser = -1;
    afterPresetLoad();
}

juce::ValueTree GlizzarineProcessor::getStateTree()
{
    auto state = apvts.copyState();
    state.setProperty ("macroTemplate", macroTemplate.load(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);
    state.setProperty ("factoryIndex", currentFactory, nullptr);
    return state;
}

void GlizzarineProcessor::restoreStateTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return;

    // Tempo settings and a locked arpeggiator survive loading
    const bool keepArp = intOf ("arp_lock") == 1;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> keep;
    for (auto* p : params)
        if (Params::isGlobal (p->paramID) || (keepArp && Params::isArp (p->paramID)))
            keep.push_back ({ p, p->getValue() });

    apvts.replaceState (tree.createCopy());

    for (auto& k : keep)
        k.first->setValueNotifyingHost (k.second);

    macroTemplate.store ((int) tree.getProperty ("macroTemplate", 0));
    presetName = tree.getProperty ("presetName", "Init").toString();
    presetCategory = tree.getProperty ("presetCategory", "").toString();
    currentFactory = (int) tree.getProperty ("factoryIndex", -1);
    afterPresetLoad();
}

void GlizzarineProcessor::loadUser (int index)
{
    if (index < 0 || index >= (int) userPresets.size())
        return;

    if (auto xml = juce::XmlDocument::parse (userPresets[(size_t) index].file))
    {
        undoManager.beginNewTransaction ("Load preset");
        restoreStateTree (juce::ValueTree::fromXml (*xml));
        presetName = userPresets[(size_t) index].name;
        presetCategory = "User";
        currentFactory = -1;
        currentUser = index;
        ++presetVersion;
    }
}

bool GlizzarineProcessor::saveUser (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;

    presetName = clean;
    presetCategory = "User";
    auto state = getStateTree();
    state.setProperty ("factoryIndex", -1, nullptr);

    auto file = dataDir().getChildFile ("Presets").getChildFile (clean + ".xml");
    bool ok = false;
    if (auto xml = state.createXml())
        ok = xml->writeTo (file);

    scanUserPresets();
    for (size_t i = 0; i < userPresets.size(); ++i)
        if (userPresets[i].name == clean)
            currentUser = (int) i;
    currentFactory = -1;
    ++presetVersion;
    return ok;
}

void GlizzarineProcessor::randomize()
{
    undoManager.beginNewTransaction ("Random sound");
    juce::Random r;
    auto pick = [&r] (std::initializer_list<float> options)
    {
        const int i = r.nextInt ((int) options.size());
        return *(options.begin() + i);
    };
    auto range = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto logRange = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto maybe = [&r] (float chance, float value) { return r.nextFloat() < chance ? value : 0.0f; };

    std::vector<std::pair<juce::String, float>> v;
    for (int i = 0; i < 3; ++i)
    {
        v.push_back ({ Params::osc (i, "type"), (float) r.nextInt (15) });
        v.push_back ({ Params::osc (i, "shape"), range (0.1f, 0.8f) });
        v.push_back ({ Params::osc (i, "color"), range (0.1f, 0.8f) });
        v.push_back ({ Params::osc (i, "pitch"), pick ({ 0.0f, 0.0f, 0.0f, 12.0f, -12.0f, 7.0f }) });
        v.push_back ({ Params::osc (i, "detune"), range (-15.0f, 15.0f) });
    }
    v.push_back ({ "o1_level", range (0.6f, 0.9f) });
    v.push_back ({ "o2_level", range (0.0f, 0.7f) });
    v.push_back ({ "o3_level", maybe (0.5f, range (0.1f, 0.5f)) });
    v.push_back ({ "xfm", maybe (0.3f, range (0.05f, 0.3f)) });
    v.push_back ({ "ring", maybe (0.25f, range (0.1f, 0.5f)) });
    v.push_back ({ "filter_type", pick ({ 0.0f, 0.0f, 1.0f, 2.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f }) });
    v.push_back ({ "cutoff", logRange (400.0f, 5000.0f) });
    v.push_back ({ "reso", range (0.0f, 0.55f) });
    v.push_back ({ "fenv_amt", range (-0.2f, 0.6f) });
    v.push_back ({ "filter_drive", range (0.0f, 0.4f) });
    v.push_back ({ "amp_attack", logRange (0.001f, 1.5f) });
    v.push_back ({ "amp_decay", logRange (0.1f, 2.5f) });
    v.push_back ({ "amp_sustain", range (0.2f, 1.0f) });
    v.push_back ({ "amp_release", logRange (0.1f, 3.0f) });
    v.push_back ({ "flt_attack", logRange (0.001f, 1.5f) });
    v.push_back ({ "flt_decay", logRange (0.1f, 2.5f) });
    v.push_back ({ "flt_sustain", range (0.0f, 0.8f) });
    v.push_back ({ "flt_release", logRange (0.1f, 2.5f) });
    v.push_back ({ "lfo1_wave", (float) r.nextInt (8) });
    v.push_back ({ "lfo1_rate", logRange (0.1f, 8.0f) });
    v.push_back ({ "lfo2_wave", (float) r.nextInt (8) });
    v.push_back ({ "lfo2_rate", logRange (0.05f, 2.0f) });

    for (int i = 0; i < 4; ++i)
    {
        if (r.nextFloat() < 0.5f)
            continue;
        const int dst = r.nextInt (12);
        v.push_back ({ Params::slot (i, "src"), (float) (1 + r.nextInt (8)) });
        v.push_back ({ Params::slot (i, "dst"), (float) dst });
        v.push_back ({ Params::slot (i, "amt"), dst == 0 ? range (-0.02f, 0.02f) : range (-0.35f, 0.35f) });
    }

    v.push_back ({ "mode", r.nextFloat() < 0.8f ? 2.0f : (float) r.nextInt (2) });
    v.push_back ({ "glide", maybe (0.3f, range (0.02f, 0.2f)) });
    v.push_back ({ "gravity", maybe (0.15f, range (-0.06f, 0.06f)) });
    v.push_back ({ "entropy", maybe (0.4f, range (0.05f, 0.4f)) });
    v.push_back ({ "drift", range (0.0f, 0.5f) });
    v.push_back ({ "orbit", maybe (0.4f, range (0.2f, 0.8f)) });
    v.push_back ({ "dist_type", (float) r.nextInt (5) });
    v.push_back ({ "dist_amt", maybe (0.4f, range (0.05f, 0.3f)) });
    v.push_back ({ "tape", maybe (0.4f, range (0.1f, 0.5f)) });
    v.push_back ({ "chorus_type", (float) r.nextInt (2) });
    v.push_back ({ "chorus_mix", maybe (0.5f, range (0.1f, 0.5f)) });
    v.push_back ({ "phaser_mix", maybe (0.3f, range (0.1f, 0.4f)) });
    v.push_back ({ "delay_sync", (float) (1 + r.nextInt (4)) });
    v.push_back ({ "delay_fb", range (0.2f, 0.5f) });
    v.push_back ({ "delay_mix", maybe (0.6f, range (0.1f, 0.3f)) });
    v.push_back ({ "cloud_size", range (0.2f, 0.8f) });
    v.push_back ({ "cloud_mix", maybe (0.35f, range (0.1f, 0.4f)) });
    v.push_back ({ "reverb_size", range (0.3f, 0.9f) });
    v.push_back ({ "reverb_mix", range (0.1f, 0.4f) });
    v.push_back ({ "reverb_shimmer", maybe (0.3f, range (0.1f, 0.5f)) });
    v.push_back ({ "wide", range (0.1f, 0.6f) });
    v.push_back ({ "master", 0.5f });

    // Switch on exactly the effects that got an amount
    const std::pair<const char*, const char*> switches[] = {
        { "dist_amt", "drive_on" }, { "tape", "tape_on" }, { "chorus_mix", "chorus_on" }, { "phaser_mix", "phaser_on" },
        { "delay_mix", "delay_on" }, { "cloud_mix", "grains_on" }, { "reverb_mix", "reverb_on" },
        { "reverb_shimmer", "shimmer_on" }, { "wide", "stereo_on" }, { "orbit", "physics_on" } };
    std::vector<std::pair<juce::String, float>> extra;
    for (const auto& sw : switches)
        for (const auto& kv : v)
            if (kv.first == sw.first && kv.second > 0.0f)
                extra.push_back ({ sw.second, 1.0f });
    for (const auto& e : extra)
        v.push_back (e);

    std::vector<std::pair<const char*, float>> raw;
    raw.reserve (v.size());
    for (auto& kv : v)
        raw.push_back ({ kv.first.toRawUTF8(), kv.second });

    applyValues (raw);
    static const int templates[4] = { TPad, TLead, TKeys, TFx };
    macroTemplate.store (templates[r.nextInt (4)]);
    presetName = "Random sound";
    presetCategory = "Random";
    currentFactory = -1;
    currentUser = -1;
    afterPresetLoad();
}

//==============================================================================
// A/B and tap tempo
void GlizzarineProcessor::switchAB (int slot)
{
    if (slot == abSlot || slot < 0 || slot > 1)
        return;

    abState[abSlot] = getStateTree();
    if (abState[slot].isValid())
        restoreStateTree (abState[slot]);
    abSlot = slot;
    ++presetVersion;
}

void GlizzarineProcessor::copyToOtherSlot()
{
    abState[1 - abSlot] = getStateTree();
}

void GlizzarineProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! tapTimes.isEmpty() && now - tapTimes.getLast() > 2000.0)
        tapTimes.clearQuick();

    tapTimes.add (now);
    while (tapTimes.size() > 5)
        tapTimes.remove (0);

    if (tapTimes.size() >= 2)
    {
        const double avg = (tapTimes.getLast() - tapTimes.getFirst()) / (tapTimes.size() - 1);
        if (avg > 0.0)
        {
            setParam ("own_bpm", (float) juce::jlimit (40.0, 240.0, 60000.0 / avg));
            setParam ("clock_mode", 1.0f);
        }
    }
}

//==============================================================================
// Controller (KeyLab)
void GlizzarineProcessor::timerCallback()
{
    autoSwitchEffects();

    const int program = pendingProgram.exchange (-1);
    if (program >= 0)
        loadFactory (program);

    const int cc = lastCC.exchange (-1);
    const bool learning = wizardStep >= 0 || learnTarget.isNotEmpty();

    if (learning && cc >= 0)
    {
        if (wizardStep >= 0)
        {
            assignCC (cc, wizardTargets[wizardStep]);
            ++wizardStep;
            if (wizardStep >= numWizardSteps)
                wizardStep = -1;
        }
        else
        {
            assignCC (cc, learnTarget);
            learnTarget.clear();
        }
        ccDirty[(size_t) cc].store (false);
        ++presetVersion;
        return;
    }

    for (int i = 0; i < 128; ++i)
    {
        if (! ccDirty[(size_t) i].exchange (false))
            continue;

        // "macro1".."macro8" are the 8 Play knobs; they point to a different parameter per preset
        juce::String id = ccMap[(size_t) i];
        if (id.startsWith ("macro"))
            id = playKnobParam (id.getTrailingIntValue() - 1);
        if (id.isEmpty())
            continue;

        const auto it = paramIndex.find (id.toStdString());
        if (it == paramIndex.end())
            continue;

        auto* p = params[(size_t) it->second];
        const float val = ccValue[(size_t) i].load();
        const float current = p->getValue();

        // Soft takeover: only grab the parameter once the control passes its current value
        if (! ccCaught[(size_t) i])
        {
            const float last = ccLast[(size_t) i];
            if (std::abs (val - current) < 0.04f || (last >= 0.0f && (last - current) * (val - current) <= 0.0f))
                ccCaught[(size_t) i] = true;
        }
        ccLast[(size_t) i] = val;

        if (ccCaught[(size_t) i])
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (val);
            p->endChangeGesture();
        }
    }
}

void GlizzarineProcessor::assignCC (int cc, const juce::String& id)
{
    for (auto& m : ccMap)
        if (m == id)
            m.clear();
    ccMap[(size_t) cc] = id;
    ccCaught[(size_t) cc] = false;
    ccLast[(size_t) cc] = -1.0f;
    saveControllerMap();
}

void GlizzarineProcessor::startWizard()   { learnTarget.clear(); wizardStep = 0; lastCC.store (-1); ++presetVersion; }
void GlizzarineProcessor::cancelWizard()  { wizardStep = -1; learnTarget.clear(); ++presetVersion; }

void GlizzarineProcessor::startLearn (const juce::String& id)
{
    wizardStep = -1;
    learnTarget = id;
    lastCC.store (-1);
    ++presetVersion;
}

void GlizzarineProcessor::forgetMapping (const juce::String& id)
{
    for (auto& m : ccMap)
        if (m == id)
            m.clear();
    saveControllerMap();
    ++presetVersion;
}

void GlizzarineProcessor::clearMappings()
{
    for (auto& m : ccMap)
        m.clear();
    saveControllerMap();
    ++presetVersion;
}

int GlizzarineProcessor::getCCFor (const juce::String& id) const
{
    for (int i = 0; i < 128; ++i)
        if (ccMap[(size_t) i] == id)
            return i;
    return -1;
}

juce::String GlizzarineProcessor::getWizardTarget (int step) const
{
    return wizardTargets[juce::jlimit (0, numWizardSteps - 1, step)];
}

juce::String GlizzarineProcessor::describeTarget (const juce::String& id) const
{
    if (id.startsWith ("macro"))
    {
        const int m = id.getTrailingIntValue() - 1;
        return "Play knob " + juce::String (m + 1) + " (" + getMacroTemplate (macroTemplate.load()).macros[juce::jlimit (0, 7, m)].name + ")";
    }

    const auto it = paramIndex.find (id.toStdString());
    if (it != paramIndex.end())
        return params[(size_t) it->second]->getName (40);
    return id;
}

juce::String GlizzarineProcessor::getWizardText() const
{
    if (wizardStep < 0)
        return {};

    const juce::String what = wizardStep < 8 ? "Turn knob " + juce::String (wizardStep + 1)
                                             : "Move fader " + juce::String (wizardStep - 7);
    return what + " on your KeyLab  (" + juce::String (wizardStep + 1) + " of 16)";
}

void GlizzarineProcessor::loadControllerMap()
{
    const auto f = dataDir().getChildFile ("controller.xml");
    if (auto xml = juce::XmlDocument::parse (f))
        for (auto* e : xml->getChildIterator())
        {
            const int cc = e->getIntAttribute ("cc", -1);
            if (cc >= 0 && cc < 128)
                ccMap[(size_t) cc] = e->getStringAttribute ("id");
        }
}

void GlizzarineProcessor::saveControllerMap()
{
    juce::XmlElement root ("MAP");
    for (int i = 0; i < 128; ++i)
        if (ccMap[(size_t) i].isNotEmpty())
        {
            auto* e = root.createNewChildElement ("M");
            e->setAttribute ("cc", i);
            e->setAttribute ("id", ccMap[(size_t) i]);
        }
    root.writeTo (dataDir().getChildFile ("controller.xml"));
}

//==============================================================================
juce::AudioProcessorEditor* GlizzarineProcessor::createEditor()
{
    return new GlizzarineEditor (*this);
}

int GlizzarineProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

const juce::String GlizzarineProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (index >= 0 && index < (int) presets.size())
        return presets[(size_t) index].name;
    return {};
}

void GlizzarineProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = getStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void GlizzarineProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (apvts.state.getType()))
        return;

    // Project state restores everything, including the tempo settings
    apvts.replaceState (tree.createCopy());
    macroTemplate.store ((int) tree.getProperty ("macroTemplate", 0));
    presetName = tree.getProperty ("presetName", "Init").toString();
    presetCategory = tree.getProperty ("presetCategory", "").toString();
    currentFactory = (int) tree.getProperty ("factoryIndex", -1);
    afterPresetLoad();
    undoManager.clearUndoHistory();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlizzarineProcessor();
}
