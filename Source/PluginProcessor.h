#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include "GuitarSampler.h"

//==============================================================================
class GuimikProcessor : public juce::AudioProcessor
{
public:
    GuimikProcessor();
    ~GuimikProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swell Guimik"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    void setProgressionText (const juce::String& text);
    juce::String getProgressionText() const { return progText; }

    gk::Config readConfig() const;

    struct UiSnapshot
    {
        gk::Chord live;
        gk::Key key;
        gk::Chord prog[gk::kMaxProg];
        int progLen = 0;
        bool useProg = false;
    };
    UiSnapshot getUiSnapshot();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiTick { -1.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    gk::Engine engine;
    GuitarSampler synth;
    juce::MidiBuffer outMidi;
    double sr = 44100.0;

    bool held[128] {};
    int heldCount = 0;
    long long onTime[128] {};
    long long sampleClock = 0, keyCandAge = 0;
    bool heldDirty = false, keyCandAdded = true;
    gk::Chord keyCand;
    bool wasHostPlaying = false, freeGen = false, wasPreview = false;
    double freeTick = 0.0;

    juce::SpinLock progLock;
    gk::Chord progChords[gk::kMaxProg];
    int progLen = 0;
    bool progDirty = false;
    juce::String progText { "Am F C G" };

    juce::SpinLock uiLock;
    UiSnapshot uiSnap;

    std::atomic<float>* pStyle = nullptr; std::atomic<float>* pDivision = nullptr; std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pBars = nullptr; std::atomic<float>* pKey = nullptr; std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pSeed = nullptr; std::atomic<float>* pSource = nullptr; std::atomic<float>* pRun = nullptr;
    std::atomic<float>* pLatch = nullptr; std::atomic<float>* pBpm = nullptr; std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pHuman = nullptr; std::atomic<float>* pHammer = nullptr; std::atomic<float>* pSlide = nullptr;
    std::atomic<float>* pDouble = nullptr; std::atomic<float>* pGhost = nullptr; std::atomic<float>* pPreview = nullptr;
    std::atomic<float>* pPreviewVol = nullptr;
    std::atomic<float>* pOn[gk::kLayers] {}; std::atomic<float>* pRole[gk::kLayers] {};
    std::atomic<float>* pOct[gk::kLayers] {}; std::atomic<float>* pDens[gk::kLayers] {};
    std::atomic<float>* pVel[gk::kLayers] {}; std::atomic<float>* pLSeed[gk::kLayers] {};

    void updateLiveChord (bool latch);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuimikProcessor)
};
