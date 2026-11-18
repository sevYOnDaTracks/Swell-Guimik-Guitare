#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Son integre : vraie guitare electrique clean (Fender, 13 notes echantillonnees).
// Samples : FreePats "FSBS Electric Guitar Clean", licence CC0 (domaine public) - voir Assets/guitar/LICENSE.txt
class GuitarSampler
{
public:
    GuitarSampler();
    void prepare (double sampleRate, int maxBlock);
    void noteOn (int ch, int note, int vel);
    void noteOff (int ch, int note);
    void allOff();
    void render (juce::AudioBuffer<float>& buffer, int start, int num, float gain);

private:
    struct Zone
    {
        int center = 60, hi = 127;
        double rate = 44100.0;
        std::vector<float> data;
    };
    struct Voice
    {
        const Zone* z = nullptr;
        double pos = 0.0, inc = 1.0;
        float gain = 0.0f, env = 1.0f, decay = 1.0f, lp = 0.0f, lpA = 1.0f, panL = 0.7f, panR = 0.7f;
        int note = -1, ch = -1;
        long age = 0;
        bool active = false, held = false;
    };
    std::array<Zone, 13> zones;
    std::array<Voice, 28> voices;
    std::vector<float> tmpL, tmpR;
    juce::Reverb reverb;
    double sr = 44100.0;
};
