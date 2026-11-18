#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static String U (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout GuimikProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto choice = [&] (const String& id, const String& name, const StringArray& items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto flt = [&] (const String& id, const String& name, float lo, float hi, float def)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f), def)); };
    auto toggle = [&] (const String& id, const String& name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto integer = [&] (const String& id, const String& name, int lo, int hi, int def)
    { p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    StringArray styles, scales;
    for (const auto& s : gk::kStyles) styles.add (U (s.name));
    for (const auto& s : gk::kScales) scales.add (U (s.name));

    const bool standalone = PluginHostType::jucePlugInClientCurrentWrapperType == AudioProcessor::wrapperType_Standalone;

    choice ("style", "Style", styles, 0);
    choice ("division", "Division", { "Auto (selon le style)", "1/4 (noires)", "1/8 (croches)", "1/8 triolet", "1/16 (doubles)", "1/16 triolet" }, 0);
    choice ("speed", "Vitesse", { U ("Moiti\xC3\xA9"), "Normale", "Double" }, 1);
    choice ("bars", "Phrase", { "2 mesures", "4 mesures", "8 mesures" }, 1);
    choice ("key", U ("Tonalit\xC3\xA9"), { "Auto", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 0);
    choice ("scale", "Gamme", scales, 0);
    integer ("seed", U ("Id\xC3\xA9""e"), 1, 9999, 7);
    choice ("source", "Accords", { "Accords MIDI entrants", U ("Progression \xC3\xA9""crite") }, standalone ? 1 : 0);
    toggle ("run", "Lecture", false);
    toggle ("latch", "Maintien", true);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Tempo libre",
                                                        NormalisableRange<float> (60.0f, 200.0f, 1.0f), 110.0f));
    flt ("swing", "Swing", 0.0f, 1.0f, 0.15f);
    flt ("human", "Humanisation", 0.0f, 1.0f, 0.3f);
    flt ("hammer", "Hammer-on / pull-off", 0.0f, 1.0f, 0.5f);
    flt ("slide", "Slides", 0.0f, 1.0f, 0.5f);
    flt ("dbl", "Doubles-cordes", 0.0f, 1.0f, 0.5f);
    flt ("ghost", U ("Notes \xC3\xA9touff\xC3\xA9""es"), 0.0f, 1.0f, 0.5f);
    toggle ("preview", U ("Son d'aper\xC3\xA7u"), true);
    flt ("previewVol", U ("Volume d'aper\xC3\xA7u"), 0.0f, 1.0f, 0.7f);

    const StringArray roles { U ("M\xC3\xA9lodie principale"), U ("R\xC3\xA9ponse"), "Tierces / sixtes", U ("Arp\xC3\xA8ge picking"), "Basses" };
    const int defRole[gk::kLayers] = { 0, 1, 3, 2 };
    const float defVel[gk::kLayers] = { 1.0f, 0.85f, 0.8f, 0.75f };
    for (int i = 0; i < gk::kLayers; ++i)
    {
        const String n = String (i + 1);
        toggle ("on" + n, "Couche " + n, i == 0);
        choice ("role" + n, U ("R\xC3\xB4le ") + n, roles, defRole[i]);
        choice ("oct" + n, "Octave " + n, { "Oct -2", "Oct -1", "Oct 0", "Oct +1", "Oct +2" }, 2);
        flt ("dens" + n, U ("Densit\xC3\xA9 ") + n, 0.0f, 1.0f, 0.5f);
        flt ("vel" + n, "Volume " + n, 0.3f, 1.2f, defVel[i]);
        integer ("lseed" + n, "Variante " + n, 1, 9999, i * 37 + 1);
    }
    return { p.begin(), p.end() };
}

GuimikProcessor::GuimikProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto raw = [this] (const String& id) { return apvts.getRawParameterValue (id); };
    pStyle = raw ("style"); pDivision = raw ("division"); pSpeed = raw ("speed"); pBars = raw ("bars");
    pKey = raw ("key"); pScale = raw ("scale"); pSeed = raw ("seed"); pSource = raw ("source");
    pRun = raw ("run"); pLatch = raw ("latch"); pBpm = raw ("bpm"); pSwing = raw ("swing");
    pHuman = raw ("human"); pHammer = raw ("hammer"); pSlide = raw ("slide"); pDouble = raw ("dbl");
    pGhost = raw ("ghost"); pPreview = raw ("preview"); pPreviewVol = raw ("previewVol");
    for (int i = 0; i < gk::kLayers; ++i)
    {
        const String n = String (i + 1);
        pOn[i] = raw ("on" + n); pRole[i] = raw ("role" + n); pOct[i] = raw ("oct" + n);
        pDens[i] = raw ("dens" + n); pVel[i] = raw ("vel" + n); pLSeed[i] = raw ("lseed" + n);
    }
    setProgressionText (progText);
}

bool GuimikProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void GuimikProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.setSampleRate (sampleRate);
    synth.prepare (sampleRate, samplesPerBlock);
    outMidi.ensureSize ((size_t) jmax (2048, samplesPerBlock * 4));
    std::fill (std::begin (held), std::end (held), false);
    heldCount = 0;
    wasHostPlaying = false; freeGen = false; freeTick = 0.0;
}

gk::Config GuimikProcessor::readConfig() const
{
    gk::Config c;
    static const int barsTab[3] = { 2, 4, 8 };
    c.style = (int) pStyle->load();
    c.division = (int) pDivision->load();
    c.speed = (int) pSpeed->load();
    c.bars = barsTab[jlimit (0, 2, (int) pBars->load())];
    c.keyRoot = (int) pKey->load() - 1;
    c.scale = (int) pScale->load();
    c.seed = (int) pSeed->load();
    c.swing = pSwing->load();
    c.human = pHuman->load();
    c.artHammer = pHammer->load();
    c.artSlide = pSlide->load();
    c.artDouble = pDouble->load();
    c.artGhost = pGhost->load();
    for (int i = 0; i < gk::kLayers; ++i)
    {
        c.layer[i].on = pOn[i]->load() > 0.5f;
        c.layer[i].role = (int) pRole[i]->load();
        c.layer[i].octave = (int) pOct[i]->load() - 2;
        c.layer[i].density = pDens[i]->load();
        c.layer[i].vel = pVel[i]->load();
        c.layer[i].seed = (int) pLSeed[i]->load();
    }
    return c;
}

void GuimikProcessor::setProgressionText (const String& text)
{
    progText = text;
    gk::Chord tmp[gk::kMaxProg];
    const int n = gk::parseProgression (text.toRawUTF8(), tmp, gk::kMaxProg);
    const SpinLock::ScopedLockType lock (progLock);
    for (int i = 0; i < n; ++i) progChords[i] = tmp[i];
    progLen = n;
    progDirty = true;
}

GuimikProcessor::UiSnapshot GuimikProcessor::getUiSnapshot()
{
    const SpinLock::ScopedLockType lock (uiLock);
    return uiSnap;
}

void GuimikProcessor::updateLiveChord (bool latch)
{
    if (heldCount > 0)
    {
        // L'accord = le dernier groupe de notes plaquees ensemble (a 80 ms pres). Si l'accord precedent
        // deborde un peu sur le nouveau (notes liees dans le piano roll), on ne melange pas les deux.
        long long latest = 0;
        for (int n = 0; n < 128; ++n) if (held[n]) latest = jmax (latest, onTime[n]);
        const long long window = (long long) (0.08 * sr);
        int group = 0;
        for (int n = 0; n < 128; ++n) if (held[n] && onTime[n] >= latest - window) ++group;
        const bool useGroup = group >= 2 || group == heldCount;
        uint16_t pcs = 0; int bass = -1;
        for (int n = 0; n < 128; ++n)
            if (held[n] && (! useGroup || onTime[n] >= latest - window)) { pcs |= gk::pcBit (n); if (bass < 0) bass = n % 12; }
        const gk::Chord c = gk::analyseChord (pcs, bass);
        engine.setLive (c);
        if (! c.same (keyCand)) { keyCand = c; keyCandAge = 0; keyCandAdded = false; }
    }
    else if (! latch)
    {
        engine.setLive (gk::Chord {});
    }
}

void GuimikProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0) return;

    engine.cfg = readConfig();
    {
        const SpinLock::ScopedTryLockType lock (progLock);
        if (lock.isLocked() && progDirty) { engine.setProgression (progChords, progLen); progDirty = false; }
    }
    const bool useProg = pSource->load() > 0.5f;
    const bool latch = pLatch->load() > 0.5f;
    const bool run = pRun->load() > 0.5f;
    engine.useProg = useProg;
    engine.update();
    if (! latch && heldCount == 0 && engine.live.valid) engine.setLive (gk::Chord {});

    bool hostPlaying = false;
    double bpm = pBpm->load(), ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; hostPlaying = pos->getIsPlaying(); }
        }
    bpm = jlimit (30.0, 300.0, bpm);
    const double tps = 24.0 * bpm / 60.0 / sr;

    engine.outCount = 0;
    if (wasHostPlaying && ! hostPlaying) engine.flushAll (0);
    wasHostPlaying = hostPlaying;

    const double blockTick = ppq * 24.0;
    int pos = 0;
    double lastTickSeen = -1.0;
    auto span = [&] (int end)
    {
        const int len = end - pos;
        if (len <= 0) return;
        if (heldDirty) { heldDirty = false; updateLiveChord (latch); }   // une seule analyse par instant, apres toutes les notes simultanees
        const bool gen = hostPlaying ? (useProg || engine.live.valid) : (useProg ? run : engine.live.valid);
        double tA;
        if (hostPlaying) { tA = blockTick + pos * tps; freeGen = false; }
        else
        {
            if (gen && ! freeGen) freeTick = 0.0;
            tA = freeTick;
            if (gen) freeTick += len * tps;
            freeGen = gen;
        }
        engine.renderSpan (tA, tps, pos, len, gen);
        lastTickSeen = gen ? tA + len * tps : -1.0;
        pos = end;
    };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        span (jlimit (0, n, meta.samplePosition));
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            if (! held[note]) { held[note] = true; ++heldCount; }
            onTime[note] = sampleClock + meta.samplePosition;
            heldDirty = true;
        }
        else if (m.isNoteOff())
        {
            const int note = m.getNoteNumber();
            if (held[note]) { held[note] = false; --heldCount; }
            heldDirty = true;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            std::fill (std::begin (held), std::end (held), false);
            heldCount = 0;
            heldDirty = true;
        }
    }
    span (n);
    engine.endBlock (n);
    sampleClock += n;
    // la tonalite ne tient compte d'un accord qu'une fois qu'il est tenu depuis 60 ms
    if (heldCount > 0 && ! keyCandAdded)
    {
        keyCandAge += n;
        if (keyCandAge > (long long) (0.06 * sr)) { engine.confirmForKey (keyCand); keyCandAdded = true; }
    }

    outMidi.clear();
    for (int i = 0; i < engine.outCount; ++i)
    {
        const auto& o = engine.out[(size_t) i];
        outMidi.addEvent (MidiMessage ((int) o.status, (int) o.d1, (int) o.d2), jlimit (0, n - 1, o.sample));
    }

    const bool preview = pPreview->load() > 0.5f;
    if (preview)
    {
        const float vol = pPreviewVol->load();
        int p = 0;
        for (const auto meta : outMidi)
        {
            const int s = jlimit (0, n, meta.samplePosition);
            synth.render (buffer, p, s - p, vol);
            p = s;
            const auto m = meta.getMessage();
            if (m.isNoteOn()) synth.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
            else if (m.isNoteOff()) synth.noteOff (m.getChannel(), m.getNoteNumber());
        }
        synth.render (buffer, p, n - p, vol);
    }
    else if (wasPreview)
    {
        synth.allOff();
    }
    wasPreview = preview;

    midi.swapWith (outMidi);

    const int total = jlimit (1, gk::kMaxBars, engine.cfg.bars) * gk::kTPB;
    uiTick.store (lastTickSeen < 0.0 ? -1.0f : (float) std::fmod (lastTickSeen, (double) total));
    {
        const SpinLock::ScopedTryLockType lock (uiLock);
        if (lock.isLocked())
        {
            uiSnap.live = engine.live;
            uiSnap.key = engine.currentKey();
            uiSnap.useProg = useProg;
            uiSnap.progLen = engine.progLen;
            for (int i = 0; i < engine.progLen; ++i) uiSnap.prog[i] = engine.prog[i];
        }
    }
}

//==============================================================================
void GuimikProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("prog", progText, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GuimikProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            const String prog = vt.getProperty ("prog", "Am F C G").toString();
            apvts.replaceState (vt);
            setProgressionText (prog);
            // ne jamais redemarrer en lecture : pas de son surprise a l'ouverture
            if (auto* run = apvts.getParameter ("run")) run->setValueNotifyingHost (0.0f);
        }
}

AudioProcessorEditor* GuimikProcessor::createEditor() { return new GuimikEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GuimikProcessor(); }
