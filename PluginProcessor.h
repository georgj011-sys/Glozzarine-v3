#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include "Mapping.h"
#include "Arp.h"

class GlizzarineProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    GlizzarineProcessor();
    ~GlizzarineProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Glizzarine"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentFactory); }
    void setCurrentProgram (int index) override { loadFactory (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Presets
    struct UserPreset { juce::String name; juce::File file; };

    void loadFactory (int index);
    void loadUser (int index);
    bool saveUser (const juce::String& name);
    void randomize();
    void toggleFavourite();
    bool isFavourite (const juce::String& name) const { return favourites.contains (name); }
    const juce::StringArray& getFavourites() const { return favourites; }
    const std::vector<UserPreset>& getUserPresets() const { return userPresets; }
    juce::String getPresetName() const { return presetName; }
    juce::String getPresetCategory() const { return presetCategory; }
    int getCurrentFactory() const { return currentFactory; }
    int getCurrentUser() const { return currentUser; }
    int getMacroTemplateIndex() const { return macroTemplate.load(); }
    int getPresetVersion() const { return presetVersion.load(); }

    // A/B
    void switchAB (int slot);
    void copyToOtherSlot();
    int getABSlot() const { return abSlot; }

    // Tempo
    void tap();

    //==============================================================================
    // Controller (KeyLab)
    static constexpr int numWizardSteps = 16;
    void startWizard();
    void cancelWizard();
    int getWizardStep() const { return wizardStep; }
    juce::String getWizardText() const;
    juce::String getWizardTarget (int step) const;
    juce::String describeTarget (const juce::String& id) const;
    void startLearn (const juce::String& paramID);
    void forgetMapping (const juce::String& paramID);
    void clearMappings();
    int getCCFor (const juce::String& paramID) const;
    juce::String getLearnTarget() const { return learnTarget; }

    //==============================================================================
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    std::atomic<int> arpDisplayStep { -1 };
    std::atomic<int> arpBarSteps { 4 };
    juce::String playKnobParam (int n) const;
    std::array<float, 2048> scope {};
    std::atomic<int> scopeWrite { 0 };

    const WaveBank& getWaveBank() const { return *waveBank; }

private:
    enum Mode { Mono = 0, Legato, Poly };

    struct Pending { double time = 0.0; int note = 0; float vel = 0.0f; bool on = false; float swell = 0.0f; };

    void timerCallback() override;

    float valueOf (const juce::String& id) const;
    int intOf (const juce::String& id) const { return (int) std::lround (valueOf (id)); }
    void autoSwitchEffects();
    void syncSwitched();
    void setParam (const juce::String& id, float realValue);
    void applyValues (const std::vector<std::pair<const char*, float>>& values);
    juce::ValueTree getStateTree();
    void restoreStateTree (const juce::ValueTree& tree);
    void afterPresetLoad();

    void advanceTo (int& cur, int target, const VoiceParams& vp);
    void renderVoices (int start, int num, const VoiceParams& vp);
    void handleMidi (const juce::MidiMessage& msg, double absTime, const VoiceParams& vp);

    void engineNoteOn (int note, float velocity, const VoiceParams& vp, float swell = 0.0f);
    void engineNoteOff (int note);
    void engineReleaseAll();

    void doArpStep (double time, const VoiceParams& vp);
    void schedule (double time, int note, float vel, bool on, float swell = 0.0f);
    void stopArp();
    void startArp (double time);
    bool arpLatched() const { return arpHoldParam || pedalDown; }

    juce::File dataDir() const;
    void scanUserPresets();
    void loadFavourites();
    void saveFavourites();
    void loadControllerMap();
    void saveControllerMap();
    void assignCC (int cc, const juce::String& id);

    juce::SharedResourcePointer<WaveBank> waveBank;

    std::vector<juce::RangedAudioParameter*> params;
    std::unordered_map<std::string, int> paramIndex;
    struct Switched { int param, sw; float last; };
    std::vector<Switched> switched;
    std::atomic<int> macroTemplate { 0 };

    std::array<GlizzarineVoice, 10> voices;
    GlizzarineFX fx;
    double sampleRate = 44100.0;

    float* outL = nullptr;
    float* outR = nullptr;
    double blockStart = 0.0, clock = 0.0, bpm = 120.0, ownBeat = 0.0;
    int tsNum = 4, tsDen = 4;

    juce::Array<int> noteStack, physicalHeld, sustained, arpNotes;
    int lastNote = -1;
    int mode = Poly, lastMode = -1;
    float bend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    bool pedalDown = false;
    juce::uint32 seed = 1;
    juce::uint64 ageCounter = 0;
    int spreadCounter = 0;

    bool arpOn = false, arpWasOn = false, arpRunning = false, arpHoldParam = false;
    double arpGrid = 0.0, arpNext = 0.0;
    int arpStep = 0, arpNoteIndex = 0, walkPos = 0;
    std::array<Pending, 256> pending;
    int pendingCount = 0;
    std::array<bool, 16> mutRest {};
    std::array<int, 16> mutOct {};
    juce::Random arpRng;

    // Presets
    std::vector<UserPreset> userPresets;
    juce::StringArray favourites;
    juce::String presetName { "Init" }, presetCategory { "Init" };
    int currentFactory = 0, currentUser = -1;
    std::atomic<int> presetVersion { 0 };
    juce::ValueTree abState[2];
    int abSlot = 0;
    juce::Array<double> tapTimes;

    // Controller
    std::array<juce::String, 128> ccMap;
    std::array<std::atomic<float>, 128> ccValue;
    std::array<std::atomic<bool>, 128> ccDirty;
    std::array<bool, 128> ccCaught {};
    std::array<float, 128> ccLast {};
    std::atomic<int> lastCC { -1 };
    std::atomic<int> pendingProgram { -1 };
    int wizardStep = -1;
    juce::String learnTarget;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlizzarineProcessor)
};
