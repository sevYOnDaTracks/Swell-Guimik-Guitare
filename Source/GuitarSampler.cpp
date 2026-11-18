#include "GuitarSampler.h"
#include "BinaryData.h"

using namespace juce;

GuitarSampler::GuitarSampler()
{
    struct Src { const char* data; int size; int center; int hi; };
    const Src src[13] = {
        { BinaryData::g36_flac, BinaryData::g36_flacSize, 36, 38 },
        { BinaryData::g41_flac, BinaryData::g41_flacSize, 41, 42 },
        { BinaryData::g45_flac, BinaryData::g45_flacSize, 45, 46 },
        { BinaryData::g48_flac, BinaryData::g48_flacSize, 48, 49 },
        { BinaryData::g52_flac, BinaryData::g52_flacSize, 52, 53 },
        { BinaryData::g55_flac, BinaryData::g55_flacSize, 55, 56 },
        { BinaryData::g59_flac, BinaryData::g59_flacSize, 59, 61 },
        { BinaryData::g64_flac, BinaryData::g64_flacSize, 64, 65 },
        { BinaryData::g67_flac, BinaryData::g67_flacSize, 67, 69 },
        { BinaryData::g71_flac, BinaryData::g71_flacSize, 71, 72 },
        { BinaryData::g74_flac, BinaryData::g74_flacSize, 74, 76 },
        { BinaryData::g80_flac, BinaryData::g80_flacSize, 80, 82 },
        { BinaryData::g85_flac, BinaryData::g85_flacSize, 85, 127 },
    };
    FlacAudioFormat flac;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        Zone& z = zones[i];
        z.center = src[i].center; z.hi = src[i].hi;
        std::unique_ptr<AudioFormatReader> reader (flac.createReaderFor (
            new MemoryInputStream (src[i].data, (size_t) src[i].size, false), true));
        if (reader == nullptr) continue;
        const int len = (int) jmin ((int64) (reader->sampleRate * 12.0), reader->lengthInSamples);
        AudioBuffer<float> buf ((int) jmax (1u, reader->numChannels), len);
        reader->read (&buf, 0, len, 0, true, true);
        z.rate = reader->sampleRate;
        z.data.resize ((size_t) len + 2, 0.0f);
        const int chans = buf.getNumChannels();
        for (int s = 0; s < len; ++s)
        {
            float v = 0.0f;
            for (int c = 0; c < chans; ++c) v += buf.getSample (c, s);
            z.data[(size_t) s] = v / (float) chans;
        }
        // on retire le silence avant l'attaque (sinon les notes arrivent en retard) et on egalise les niveaux
        float peak = 1.0e-6f;
        for (int s = 0; s < len; ++s) peak = jmax (peak, std::abs (z.data[(size_t) s]));
        int first = 0;
        while (first < len - 1 && std::abs (z.data[(size_t) first]) < 0.02f * peak) ++first;
        first = jmax (0, first - 24);
        if (first > 0)
        {
            z.data.erase (z.data.begin(), z.data.begin() + first);
            z.data.resize (z.data.size() + (size_t) first, 0.0f);
        }
        const float norm = 0.8f / peak;
        for (auto& v : z.data) v *= norm;
    }
}

void GuitarSampler::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (auto& v : voices) v.active = false;
    tmpL.assign ((size_t) jmax (maxBlock, 4096), 0.0f);
    tmpR.assign (tmpL.size(), 0.0f);
    Reverb::Parameters p;
    p.roomSize = 0.5f; p.damping = 0.5f; p.wetLevel = 0.13f; p.dryLevel = 0.9f; p.width = 1.0f; p.freezeMode = 0.0f;
    reverb.setParameters (p);
    reverb.setSampleRate (sr);
    reverb.reset();
}

void GuitarSampler::noteOn (int ch, int note, int vel)
{
    const Zone* z = &zones.back();
    for (const auto& zn : zones) if (note <= zn.hi) { z = &zn; break; }
    if (z->data.size() < 16) return;

    Voice* best = nullptr;
    for (auto& v : voices) if (v.active && v.ch == ch && v.note == note) { best = &v; break; }
    if (best == nullptr) for (auto& v : voices) if (! v.active) { best = &v; break; }
    if (best == nullptr)
    {
        best = &voices[0];
        for (auto& v : voices) if (v.age > best->age) best = &v;
    }
    Voice& v = *best;
    const float vn = jlimit (0.0f, 1.0f, (float) vel / 127.0f);
    const bool muted = vel < 56;                    // note etouffee : courte et sourde
    v.z = z;
    v.pos = 0.0;
    v.inc = (z->rate / sr) * std::pow (2.0, (note - z->center) / 12.0);
    v.gain = 0.2f + 0.8f * std::pow (vn, 1.4f);
    v.env = 1.0f;
    v.decay = muted ? std::exp (-1.0f / (float) (sr * 0.05)) : 1.0f;
    v.lpA = muted ? 0.18f : jlimit (0.2f, 1.0f, 0.3f + 0.75f * vn);
    v.lp = 0.0f;
    v.note = note; v.ch = ch; v.age = 0;
    v.active = true; v.held = true;
    const float pan = jlimit (0.3f, 0.7f, 0.5f + (float) (note - 62) / 110.0f + (float) (ch - 2) * 0.05f);
    v.panL = std::cos (pan * MathConstants<float>::halfPi);
    v.panR = std::sin (pan * MathConstants<float>::halfPi);
}

void GuitarSampler::noteOff (int ch, int note)
{
    for (auto& v : voices)
        if (v.active && v.held && v.ch == ch && v.note == note)
        {
            v.held = false;
            v.decay = jmin (v.decay, std::exp (-1.0f / (float) (sr * 0.09)));   // relachement naturel ~200 ms
        }
}

void GuitarSampler::allOff()
{
    for (auto& v : voices) v.active = false;
}

void GuitarSampler::render (AudioBuffer<float>& buffer, int start, int num, float gain)
{
    if (num <= 0 || buffer.getNumChannels() == 0) return;
    num = jmin (num, (int) tmpL.size());
    float* L = tmpL.data(); float* R = tmpR.data();
    FloatVectorOperations::clear (L, num);
    FloatVectorOperations::clear (R, num);

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float* d = v.z->data.data();
        const double end = (double) v.z->data.size() - 2.0;
        for (int i = 0; i < num; ++i)
        {
            if (v.pos >= end || v.env < 0.0005f) { v.active = false; break; }
            const int i0 = (int) v.pos;
            const float fr = (float) (v.pos - (double) i0);
            const float s = d[i0] + (d[i0 + 1] - d[i0]) * fr;
            v.lp += v.lpA * (s - v.lp);
            const float y = v.lp * v.gain * v.env;
            v.env *= v.decay;
            v.pos += v.inc;
            L[i] += y * v.panL;
            R[i] += y * v.panR;
        }
        v.age += num;
    }

    reverb.processStereo (L, R, num);
    // limiteur doux : plusieurs couches en meme temps ne saturent jamais la sortie
    for (int i = 0; i < num; ++i)
    {
        L[i] = std::tanh (L[i] * 1.1f);
        R[i] = std::tanh (R[i] * 1.1f);
    }
    if (buffer.getNumChannels() >= 2)
    {
        buffer.addFrom (0, start, L, num, gain);
        buffer.addFrom (1, start, R, num, gain);
    }
    else
    {
        buffer.addFrom (0, start, L, num, gain * 0.7f);
        buffer.addFrom (0, start, R, num, gain * 0.7f);
    }
}
