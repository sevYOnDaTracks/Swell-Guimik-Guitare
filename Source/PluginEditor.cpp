#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

static String U (const char* s) { return String::fromUTF8 (s); }

namespace col
{
    const Colour bg      { 0xff050505 };
    const Colour panel   { 0xff0e0e0e };
    const Colour panelLn { 0xff1e1e1e };
    const Colour control { 0xff171717 };
    const Colour ctrlLn  { 0xff303030 };
    const Colour text    { 0xfff2f2f2 };
    const Colour muted   { 0xff8c8c8c };
    const Colour white   { 0xfffafafa };
    const Colour black   { 0xff0a0a0a };
    const Colour layers[gk::kLayers] = { Colour (0xffafa9ec), Colour (0xff5dcaa5), Colour (0xfff0997b), Colour (0xffed93b1) };
}

//==============================================================================
SwellLnf::SwellLnf()
{
    setColour (ResizableWindow::backgroundColourId, col::bg);
    setColour (ComboBox::textColourId, col::text);
    setColour (PopupMenu::backgroundColourId, Colour (0xff141414));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::white);
    setColour (PopupMenu::highlightedTextColourId, col::black);
    setColour (TextButton::buttonColourId, col::control);
    setColour (TextButton::buttonOnColourId, col::white);
    setColour (TextButton::textColourOffId, col::text);
    setColour (TextButton::textColourOnId, col::black);
    setColour (Slider::thumbColourId, col::white);
    setColour (Slider::trackColourId, Colour (0xffd0d0d0));
    setColour (Slider::backgroundColourId, Colour (0xff2a2a2a));
    setColour (Slider::textBoxTextColourId, col::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TextEditor::backgroundColourId, col::control);
    setColour (TextEditor::outlineColourId, col::ctrlLn);
    setColour (TextEditor::focusedOutlineColourId, col::white);
    setColour (TextEditor::textColourId, col::text);
    setColour (TextEditor::highlightColourId, Colour (0xff3a3a3a));
    setColour (CaretComponent::caretColourId, col::white);
    setColour (ToggleButton::textColourId, col::text);
    setColour (ToggleButton::tickColourId, col::white);
    setColour (Label::textColourId, col::text);
}

void SwellLnf::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    g.setColour (box.isMouseOver (true) ? Colour (0xff1e1e1e) : col::control);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? col::white : col::ctrlLn);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    Path p;
    const float cx = (float) w - 15.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4.5f, cy - 2.0f);
    p.lineTo (cx, cy + 2.5f);
    p.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (col::muted);
    g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font SwellLnf::getComboBoxFont (ComboBox&) { return Font (FontOptions (14.5f)); }
Font SwellLnf::getPopupMenuFont() { return Font (FontOptions (14.5f)); }
Font SwellLnf::getTextButtonFont (TextButton&, int) { return Font (FontOptions (14.0f, Font::bold)); }

void SwellLnf::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour c = base;
    if (down) c = c.contrasting (0.18f);
    else if (over) c = c.contrasting (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    if (base.getBrightness() < 0.5f)
    {
        g.setColour (col::ctrlLn);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }
}

void SwellLnf::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const float x0 = (float) x, x1 = (float) (x + w);
    g.setColour (s.findColour (Slider::backgroundColourId));
    g.fillRoundedRectangle (x0, cy - 2.0f, x1 - x0, 4.0f, 2.0f);
    g.setColour (s.findColour (Slider::trackColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (x0, cy - 2.0f, jmax (0.0f, pos - x0), 4.0f, 2.0f);
    const float rad = s.isMouseOverOrDragging() ? 7.5f : 6.5f;
    g.setColour (col::bg);
    g.fillEllipse (pos - rad - 2.0f, cy - rad - 2.0f, (rad + 2.0f) * 2.0f, (rad + 2.0f) * 2.0f);
    g.setColour (s.findColour (Slider::thumbColourId));
    g.fillEllipse (pos - rad, cy - rad, rad * 2.0f, rad * 2.0f);
}

void SwellLnf::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    const bool on = b.getToggleState();
    const float h = 18.0f, w = 32.0f, y = ((float) b.getHeight() - h) * 0.5f;
    const Rectangle<float> pill (1.0f, y, w, h);
    g.setColour (on ? b.findColour (ToggleButton::tickColourId) : Colour (0xff2c2c2c).brighter (over ? 0.1f : 0.0f));
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (on ? col::black : Colour (0xff9a9a9a));
    const float kx = on ? pill.getRight() - h + 3.0f : pill.getX() + 3.0f;
    g.fillEllipse (kx, y + 3.0f, h - 6.0f, h - 6.0f);
    g.setColour (b.findColour (ToggleButton::textColourId).withAlpha (b.isEnabled() ? (on ? 1.0f : 0.7f) : 0.35f));
    g.setFont (FontOptions (14.0f));
    g.drawText (b.getButtonText(), Rectangle<float> (w + 10.0f, 0.0f, (float) b.getWidth() - w - 10.0f, (float) b.getHeight()),
                Justification::centredLeft);
}

//==============================================================================
void RollView::refresh()
{
    const auto snap = proc.getUiSnapshot();
    ui.cfg = proc.readConfig();
    ui.useProg = snap.useProg;
    ui.progLen = snap.progLen;
    for (int i = 0; i < snap.progLen; ++i) ui.prog[i] = snap.prog[i];
    ui.live = snap.live;
    ui.autoKey = ui.progKey = snap.key;
    ui.update();

    bars = ui.cfg.bars;
    totalTicks = bars * gk::kTPB;
    playTick = proc.uiTick.load();
    rects.clear();
    const gk::Key k = snap.key;
    for (int b = 0; b < bars; ++b)
    {
        char nm[16];
        gk::chordName (ui.chordAtBar (b), nm, sizeof (nm));
        barNames[b] = nm;
    }
    for (int l = 0; l < gk::kLayers; ++l)
    {
        if (! ui.cfg.layer[l].on) continue;
        const auto& ph = ui.phrase[l];
        for (int i = 0; i < ph.count; ++i)
        {
            const auto& e = ph.ev[(size_t) i];
            gk::Chord c = ui.chordAtBar (e.tick / gk::kTPB);
            if (! c.valid) continue;
            c = gk::resolveChord (c, k);
            const auto r = ui.realize (e, c, k, l);
            for (int j = 0; j < r.n; ++j)
                rects.push_back ({ (float) e.tick, (float) e.len, r.pitch[j], l, (e.flags & (gk::fHammer | gk::fGhost)) != 0 });
        }
    }
    repaint();
}

void RollView::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 10.0f);
    const float top = 26.0f, h = r.getHeight() - top - 8.0f, w = r.getWidth();
    const float barW = w / (float) bars;
    const int cur = playTick >= 0.0f ? (int) playTick / gk::kTPB : -1;
    for (int b = 0; b < bars; ++b)
    {
        const float bx = barW * (float) b;
        if (b % 2 == 0)
        {
            g.setColour (Colour (0xff131313));
            g.fillRect (bx, top, barW, h);
        }
        for (int q = 1; q < 4; ++q)
        {
            g.setColour (Colour (0xff1c1c1c));
            g.fillRect (bx + barW * (float) q / 4.0f, top, 1.0f, h);
        }
        if (b > 0) { g.setColour (Colour (0xff2e2e2e)); g.fillRect (bx, top, 1.0f, h); }

        // pastille d'accord
        const bool isCur = b == cur;
        const Rectangle<float> chip (bx + 8.0f, 5.0f, jmin (64.0f, barW - 16.0f), 17.0f);
        g.setColour (isCur ? col::white : Colour (0xff1c1c1c));
        g.fillRoundedRectangle (chip, 8.5f);
        g.setColour (isCur ? col::black : col::text.withAlpha (0.8f));
        g.setFont (FontOptions (12.5f, Font::bold));
        g.drawText (barNames[b], chip, Justification::centred);
    }

    int lo = 127, hi = 0;
    for (const auto& n : rects) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
    if (rects.empty()) { lo = 48; hi = 72; }
    lo -= 2; hi += 2;
    if (hi - lo < 16) { const int mid = (hi + lo) / 2; lo = mid - 8; hi = mid + 8; }
    const float noteH = jlimit (4.0f, 9.0f, h / (float) (hi - lo));
    for (const auto& n : rects)
    {
        const float x = n.x / (float) totalTicks * w;
        const float nw = jmax (5.0f, n.w / (float) totalTicks * w - 1.5f);
        const float y = top + h - ((float) (n.pitch - lo) + 0.5f) / (float) (hi - lo) * h - noteH * 0.5f;
        g.setColour (col::layers[n.layer].withAlpha (n.soft ? 0.5f : 1.0f));
        g.fillRoundedRectangle (x + 0.5f, y, jmin (nw, w - x - 1.0f), noteH, 2.0f);
    }
    if (rects.empty())
    {
        g.setColour (col::muted);
        g.setFont (FontOptions (13.5f));
        g.drawText (U ("Joue un accord en MIDI ou \xC3\xA9""cris une progression pour voir les notes"),
                    getLocalBounds().withTrimmedTop (20), Justification::centred);
    }
    if (playTick >= 0.0f)
    {
        g.setColour (col::white.withAlpha (0.9f));
        g.fillRect (playTick / (float) totalTicks * w, top, 1.5f, h);
    }
}

//==============================================================================
GuimikEditor::GuimikEditor (GuimikProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&lnf);
    logo = ImageCache::getFromMemory (BinaryData::swellmark_png, BinaryData::swellmark_pngSize);
    bebas = Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf, (size_t) BinaryData::BebasNeueRegular_ttfSize);

    setupCombo (style, "style", "Style");
    setupCombo (division, "division", "Division");
    setupCombo (bars, "bars", "Phrase");
    setupCombo (key, "key", U ("Tonalit\xC3\xA9"));
    setupCombo (scale, "scale", "Gamme");
    setupCombo (speed, "speed", "Vitesse");
    setupCombo (source, "source", "Accords");

    newIdea.setButtonText (U ("Nouvelle id\xC3\xA9""e"));
    newIdea.setColour (TextButton::buttonColourId, col::white);
    newIdea.setColour (TextButton::textColourOffId, col::black);
    newIdea.onClick = [this]
    {
        randomise ("seed");
        for (int i = 0; i < gk::kLayers; ++i) randomise ("lseed" + String (i + 1));
    };
    addAndMakeVisible (newIdea);

    play.setButtonText ("Lecture");
    play.setClickingTogglesState (true);
    addAndMakeVisible (play);
    buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "run", play));

    latch.setButtonText ("Maintenir l'accord");
    addAndMakeVisible (latch);
    buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "latch", latch));

    preview.setButtonText (U ("Son d'aper\xC3\xA7u"));
    addAndMakeVisible (preview);
    buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "preview", preview));

    prog.setText (proc.getProgressionText(), false);
    prog.setFont (FontOptions (15.0f));
    prog.setIndents (10, 5);
    prog.setTextToShowWhenEmpty ("Am F C G", col::muted);
    prog.onTextChange = [this] { proc.setProgressionText (prog.getText()); };
    addAndMakeVisible (prog);
    captions.push_back ({ &prog, U ("Progression \xC3\xA9""crite (un accord par mesure)") });

    setupSlider (bpm, "bpm", "Tempo hors DAW");
    bpm.setTextBoxStyle (Slider::TextBoxRight, false, 44, 20);
    bpm.setNumDecimalPlacesToDisplay (0);
    setupSlider (hammer, "hammer", "Hammer-on / pull-off");
    setupSlider (slide, "slide", "Slides");
    setupSlider (dbl, "dbl", "Doubles-cordes");
    setupSlider (ghost, "ghost", U ("Notes \xC3\xA9touff\xC3\xA9""es"));
    setupSlider (swing, "swing", "Swing");
    setupSlider (human, "human", "Humanisation");
    setupSlider (previewVol, "previewVol", U ("Volume d'aper\xC3\xA7u"));

    for (int i = 0; i < gk::kLayers; ++i)
    {
        auto& r = rows[i];
        const String n = String (i + 1);
        r.on.setButtonText ("Couche " + n);
        r.on.setColour (ToggleButton::tickColourId, col::layers[i]);
        addAndMakeVisible (r.on);
        buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "on" + n, r.on));
        setupCombo (r.role, "role" + n, {});
        setupCombo (r.oct, "oct" + n, {});
        setupSlider (r.dens, "dens" + n, {});
        setupSlider (r.vel, "vel" + n, {});
        r.dens.setColour (Slider::trackColourId, col::layers[i]);
        r.vel.setColour (Slider::trackColourId, col::layers[i]);
        r.reroll.setButtonText ("Relancer");
        r.reroll.onClick = [this, n] { randomise ("lseed" + n); };
        addAndMakeVisible (r.reroll);
    }

    addAndMakeVisible (roll);
    setSize (960, 764);
    startTimerHz (20);
}

GuimikEditor::~GuimikEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GuimikEditor::setupCombo (ComboBox& c, const String& id, const String& caption)
{
    if (auto* ch = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.addItemList (ch->choices, 1);
    addAndMakeVisible (c);
    comboAtt.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c));
    if (caption.isNotEmpty()) captions.push_back ({ &c, caption });
}

void GuimikEditor::setupSlider (Slider& s, const String& id, const String& caption)
{
    s.setSliderStyle (Slider::LinearHorizontal);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (s);
    sliderAtt.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, s));
    if (caption.isNotEmpty()) captions.push_back ({ &s, caption });
}

void GuimikEditor::randomise (const String& id)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (Random::getSystemRandom().nextFloat());
        prm->endChangeGesture();
    }
}

void GuimikEditor::timerCallback()
{
    const auto snap = proc.getUiSnapshot();
    char nm[16] = "-";
    if (snap.useProg)
    {
        const float t = proc.uiTick.load();
        if (snap.progLen > 0) gk::chordName (snap.prog[t < 0.0f ? 0 : ((int) t / gk::kTPB) % snap.progLen], nm, sizeof (nm));
    }
    else gk::chordName (snap.live, nm, sizeof (nm));
    const String c (nm), k = String (gk::noteName (snap.key.root)) + " " + U (gk::kScales[snap.key.scale].name).toLowerCase();
    // Tonalite forcee a la main qui contredit les accords = fausses notes garanties : on previent.
    int outOfKey = 0;
    auto count = [&] (const gk::Chord& ch)
    {
        if (! ch.valid) return;
        for (int pc = 0; pc < 12; ++pc)
            if (((ch.mask >> pc) & 1) != 0 && gk::scaleIndex (snap.key, pc) < 0) ++outOfKey;
    };
    if (snap.useProg) for (int i = 0; i < snap.progLen; ++i) count (snap.prog[i]);
    else count (snap.live);
    const bool manualKey = proc.apvts.getRawParameterValue ("key")->load() > 0.5f;
    const bool warn = manualKey && outOfKey >= 2;
    if (c != chordText || k != keyText || warn != keyWarn)
    {
        chordText = c; keyText = k; keyWarn = warn;
        repaint (0, 0, getWidth(), 66);
    }

    const bool written = snap.useProg;
    prog.setEnabled (written);
    prog.setAlpha (written ? 1.0f : 0.4f);
    latch.setEnabled (! written);
    play.setButtonText (play.getToggleState() ? U ("Arr\xC3\xAAter") : "Lecture");
    roll.refresh();
}

//==============================================================================
void GuimikEditor::paint (Graphics& g)
{
    g.fillAll (col::bg);

    // en-tete
    if (logo.isValid())
        g.drawImage (logo, Rectangle<float> (20.0f, 14.0f, 40.0f, 36.0f), RectanglePlacement::centred);
    g.setColour (col::text);
    g.setFont (bebas != nullptr ? Font (FontOptions (bebas).withHeight (38.0f)) : Font (FontOptions (30.0f, Font::bold)));
    g.drawText ("SWELL GUIMIK", 72, 12, 230, 42, Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (FontOptions (13.0f));
    g.drawText (U ("g\xC3\xA9n\xC3\xA9rateur de m\xC3\xA9los guitare"), 222, 24, 220, 20, Justification::centredLeft);

    auto chip = [&] (int right, const String& label, const String& value, int w)
    {
        const Rectangle<float> r ((float) (right - w), 17.0f, (float) w, 30.0f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 15.0f);
        g.setColour (col::panelLn);
        g.drawRoundedRectangle (r, 15.0f, 1.0f);
        g.setColour (col::muted);
        g.setFont (FontOptions (12.0f));
        g.drawText (label, r.withTrimmedLeft (14.0f), Justification::centredLeft);
        g.setColour (col::text);
        g.setFont (FontOptions (14.0f, Font::bold));
        g.drawText (value, r.withTrimmedRight (14.0f), Justification::centredRight);
    };
    chip (getWidth() - 16, U ("Tonalit\xC3\xA9"), keyText, 250);
    chip (getWidth() - 276, "Accord", chordText, 130);
    if (keyWarn)
    {
        g.setColour (Colour (0xffef9f27));
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText (U ("Cette tonalit\xC3\xA9 ne colle pas \xC3\xA0 tes accords : remets Tonalit\xC3\xA9 sur Auto"),
                    getWidth() - 516, 49, 500, 14, Justification::centredRight);
    }

    // panneaux
    for (const auto& p : panels)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (p.toFloat(), 10.0f);
        g.setColour (col::panelLn);
        g.drawRoundedRectangle (p.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setColour (col::muted);
    g.setFont (FontOptions (12.0f));
    for (const auto& c : captions)
    {
        const auto b = c.comp->getBounds();
        g.drawText (c.text, b.getX() + 1, b.getY() - 18, jmax (b.getWidth(), 230), 15, Justification::centredLeft);
    }

    // en-tete des couches
    const int hy = rows[0].on.getY() - 24;
    g.setColour (col::text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("Couches", rows[0].on.getX(), hy, 120, 16, Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (FontOptions (12.0f));
    g.drawText (U ("R\xC3\xB4le"), rows[0].role.getX() + 1, hy, 120, 16, Justification::centredLeft);
    g.drawText ("Octave", rows[0].oct.getX() + 1, hy, 80, 16, Justification::centredLeft);
    g.drawText (U ("Densit\xC3\xA9"), rows[0].dens.getX() + 1, hy, 120, 16, Justification::centredLeft);
    g.drawText ("Volume", rows[0].vel.getX() + 1, hy, 120, 16, Justification::centredLeft);
    g.drawText ("canal MIDI", rows[0].reroll.getRight() - 150, hy, 150, 16, Justification::centredRight);
    for (int i = 0; i < gk::kLayers; ++i)
    {
        const auto b = rows[i].reroll.getBounds();
        g.setColour (col::layers[i]);
        g.setFont (FontOptions (12.5f, Font::bold));
        g.drawText (String (i + 1), b.getRight() + 8, b.getY(), 24, b.getHeight(), Justification::centred);
    }
}

void GuimikEditor::resized()
{
    const int m = 16, pad = 16, W = getWidth() - 2 * m, inner = W - 2 * pad, x0 = m + pad, h = 30, gap = 12;

    // panneau 1 : idee + accords
    panels[0] = { m, 66, W, 190 };
    int y = panels[0].getY() + 30;
    {
        const int wBtn = 150, wPlay = 110;
        const int rest = inner - wBtn - wPlay - 4 * gap;
        const int wStyle = rest * 40 / 100, wDiv = rest * 34 / 100, wBars = rest - wStyle - wDiv;
        int x = x0;
        style.setBounds (x, y, wStyle, h); x += wStyle + gap;
        division.setBounds (x, y, wDiv, h); x += wDiv + gap;
        bars.setBounds (x, y, wBars, h); x += wBars + gap;
        newIdea.setBounds (x, y, wBtn, h); x += wBtn + gap;
        play.setBounds (x, y, wPlay, h);
    }
    y += 54;
    {
        const int wKey = 110, wSpeed = 130, wSrc = 220, wLatch = 180;
        const int wScale = inner - wKey - wSpeed - wSrc - wLatch - 4 * gap;
        int x = x0;
        key.setBounds (x, y, wKey, h); x += wKey + gap;
        scale.setBounds (x, y, wScale, h); x += wScale + gap;
        speed.setBounds (x, y, wSpeed, h); x += wSpeed + gap;
        source.setBounds (x, y, wSrc, h); x += wSrc + gap;
        latch.setBounds (x, y, wLatch, h);
    }
    y += 54;
    {
        const int wBpm = 250;
        prog.setBounds (x0, y, inner - wBpm - gap, h);
        bpm.setBounds (x0 + inner - wBpm, y, wBpm, h);
    }

    // piano roll
    roll.setBounds (m, panels[0].getBottom() + 8, W, 150);

    // panneau 2 : jeu de guitare
    panels[1] = { m, roll.getBottom() + 8, W, 62 };
    {
        const int w4 = (inner - 3 * gap) / 4, sy = panels[1].getY() + 30;
        Slider* s[4] = { &hammer, &slide, &dbl, &ghost };
        for (int i = 0; i < 4; ++i) s[i]->setBounds (x0 + i * (w4 + gap), sy, w4, 22);
    }

    // panneau 3 : couches
    panels[2] = { m, panels[1].getBottom() + 8, W, 36 + gk::kLayers * 36 + 8 };
    y = panels[2].getY() + 36;
    for (int i = 0; i < gk::kLayers; ++i)
    {
        auto& r = rows[i];
        const int wOn = 120, wRole = 190, wOct = 96, wRe = 92, wCh = 32;
        const int wS = (inner - wOn - wRole - wOct - wRe - wCh - 5 * gap) / 2;
        int x = x0;
        r.on.setBounds (x, y, wOn, h); x += wOn + gap;
        r.role.setBounds (x, y, wRole, h); x += wRole + gap;
        r.oct.setBounds (x, y, wOct, h); x += wOct + gap;
        r.dens.setBounds (x, y, wS, h); x += wS + gap;
        r.vel.setBounds (x, y, wS, h); x += wS + gap;
        r.reroll.setBounds (x, y, wRe, h);
        y += 36;
    }

    // panneau 4 : feeling + apercu
    panels[3] = { m, panels[2].getBottom() + 8, W, 62 };
    {
        const int wPrev = 160, sy = panels[3].getY() + 30;
        const int w3 = (inner - wPrev - 3 * gap) / 3;
        swing.setBounds (x0, sy, w3, 22);
        human.setBounds (x0 + w3 + gap, sy, w3, 22);
        previewVol.setBounds (x0 + 2 * (w3 + gap), sy, w3, 22);
        preview.setBounds (x0 + 3 * (w3 + gap), sy - 4, wPrev, h);
    }
}
