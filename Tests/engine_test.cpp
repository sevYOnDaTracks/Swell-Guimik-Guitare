// Test hors-ligne du moteur : affiche les notes generees et ecrit un fichier MIDI par style.
#include "Engine.h"

#include <cstdio>
#include <string>
#include <vector>

struct Note { double beat; int ch, pitch, vel; double len; };

static std::string pitchName (int p)
{
    return std::string (gk::noteName (p)) + std::to_string (p / 12 - 1);
}

static void writeVar (std::vector<unsigned char>& d, unsigned long v)
{
    unsigned char b[5]; int n = 0;
    b[n++] = (unsigned char) (v & 0x7F);
    while (v >>= 7) b[n++] = (unsigned char) ((v & 0x7F) | 0x80);
    while (n > 0) d.push_back (b[--n]);
}

static void writeMidi (const std::string& path, const std::vector<Note>& notes, double bpm)
{
    struct E { long t; unsigned char a, b, c; };
    std::vector<E> ev;
    for (const auto& n : notes)
    {
        ev.push_back ({ std::lround (n.beat * 480.0), (unsigned char) (0x90 | n.ch), (unsigned char) n.pitch, (unsigned char) n.vel });
        ev.push_back ({ std::lround ((n.beat + n.len) * 480.0), (unsigned char) (0x80 | n.ch), (unsigned char) n.pitch, 0 });
    }
    std::stable_sort (ev.begin(), ev.end(), [] (const E& x, const E& y)
    {
        if (x.t != y.t) return x.t < y.t;
        return (x.a & 0xF0) < (y.a & 0xF0);
    });
    std::vector<unsigned char> trk;
    const unsigned long us = (unsigned long) (60000000.0 / bpm);
    trk.insert (trk.end(), { 0x00, 0xFF, 0x51, 0x03, (unsigned char) (us >> 16), (unsigned char) (us >> 8), (unsigned char) us });
    long last = 0;
    for (const auto& e : ev) { writeVar (trk, (unsigned long) (e.t - last)); last = e.t; trk.push_back (e.a); trk.push_back (e.b); trk.push_back (e.c); }
    trk.insert (trk.end(), { 0x00, 0xFF, 0x2F, 0x00 });
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::printf ("  (impossible d'ecrire %s)\n", path.c_str()); return; }
    const unsigned char head[] = { 'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0x01,0xE0, 'M','T','r','k',
                                   (unsigned char) (trk.size() >> 24), (unsigned char) (trk.size() >> 16),
                                   (unsigned char) (trk.size() >> 8), (unsigned char) trk.size() };
    std::fwrite (head, 1, sizeof (head), f);
    std::fwrite (trk.data(), 1, trk.size(), f);
    std::fclose (f);
}

static std::vector<Note> render (gk::Engine& e, int bars, double bpm)
{
    const double sr = 48000.0;
    e.setSampleRate (sr);
    e.update();
    const double tps = 24.0 * bpm / 60.0 / sr;
    const int block = 512;
    const long total = (long) (bars * 96 / tps);
    std::vector<Note> notes;
    struct On { double beat; int vel; bool on; };
    static On open[4][128];
    for (auto& c : open) for (auto& o : c) o.on = false;
    long pos = 0;
    auto handle = [&] (long base)
    {
        std::stable_sort (e.out.begin(), e.out.begin() + e.outCount,
                          [] (const gk::Engine::Out& a, const gk::Engine::Out& b) { return a.sample < b.sample; });
        for (int i = 0; i < e.outCount; ++i)
        {
            const auto& m = e.out[(size_t) i];
            const double beat = double (base + m.sample) * tps / 24.0;
            const int ch = m.status & 0x0F;
            On& o = open[ch][m.d1];
            if ((m.status & 0xF0) == 0x90)
            {
                if (o.on) std::printf ("  !! note deja ouverte %d\n", m.d1);
                o = { beat, m.d2, true };
            }
            else if (o.on) { notes.push_back ({ o.beat, ch, m.d1, o.vel, beat - o.beat }); o.on = false; }
        }
    };
    while (pos < total)
    {
        e.outCount = 0;
        e.renderSpan (pos * tps, tps, 0, block, true);
        handle (pos);
        e.endBlock (block);
        pos += block;
    }
    e.outCount = 0;
    e.flushAll (0);
    handle (pos);
    for (auto& c : open) for (auto& o : c) if (o.on) std::printf ("  !! note restee ouverte\n");
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.beat < b.beat; });
    return notes;
}

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "demos";
    const char* progs[gk::kNumStyles] = { "C G Am F", "C F G F", "Am F C G", "Em C Am B7", "Am Dm G C" };
    const double bpms[gk::kNumStyles] = { 104, 120, 140, 140, 80 };
    const char* files[gk::kNumStyles] = { "afrobeats", "rumba_soukous", "rap_melodique", "trap_guitare", "melancolique" };

    for (int s = 0; s < gk::kNumStyles; ++s)
    {
        gk::Engine e;
        gk::Chord chords[gk::kMaxProg];
        const int n = gk::parseProgression (progs[s], chords, gk::kMaxProg);
        e.useProg = true;
        e.setProgression (chords, n);
        e.cfg.style = s;
        e.cfg.seed = 7;
        e.cfg.bars = 4;
        e.cfg.human = 0.0f;
        e.cfg.layer[0] = { true, gk::Lead, 0, 0.55f, 1.0f, 1 };
        const gk::Key k = e.currentKey();
        std::printf ("\n=== %s | %s | tonalite detectee : %s %s ===\n", files[s], progs[s], gk::noteName (k.root), gk::kScales[k.scale].name);

        auto lead = render (e, 4, bpms[s]);
        int bar = -1;
        for (const auto& nt : lead)
        {
            const int b = (int) (nt.beat / 4.0 + 1.0e-6);
            if (b != bar) { bar = b; char nm[16]; gk::chordName (chords[b % n], nm, sizeof (nm)); std::printf ("\n  mes.%d [%s] ", b + 1, nm); }
            std::printf ("%s ", pitchName (nt.pitch).c_str());
        }
        std::printf ("\n  -> %d notes (melodie seule)\n", (int) lead.size());

        // version complete : 4 couches
        e.cfg.layer[1] = { true, gk::Answer, 0, 0.4f, 0.85f, 38 };
        e.cfg.layer[2] = { true, gk::Arp, 0, 0.5f, 0.8f, 75 };
        e.cfg.layer[3] = { true, gk::Harmony, 0, 0.5f, 0.75f, 112 };
        e.cfg.human = 0.3f;
        auto full = render (e, 8, bpms[s]);
        int per[4] = { 0, 0, 0, 0 }, lo = 127, hi = 0;
        for (const auto& nt : full) { per[nt.ch]++; lo = std::min (lo, nt.pitch); hi = std::max (hi, nt.pitch); }
        std::printf ("  4 couches sur 8 mesures : %d / %d / %d / %d notes, registre %s - %s\n",
                     per[0], per[1], per[2], per[3], pitchName (lo).c_str(), pitchName (hi).c_str());
        writeMidi (outDir + "/" + files[s] + ".mid", full, bpms[s]);
    }

    // divisions et vitesses
    std::printf ("\n=== divisions (afrobeats, melodie seule, 4 mesures) ===\n");
    const char* divNames[6] = { "auto", "1/4", "1/8", "1/8T", "1/16", "1/16T" };
    for (int d = 0; d < 6; ++d)
        for (int sp = 0; sp < 3; ++sp)
        {
            gk::Engine e;
            gk::Chord chords[gk::kMaxProg];
            e.useProg = true;
            e.setProgression (chords, gk::parseProgression ("Lam Fa Do Sol", chords, gk::kMaxProg));
            e.cfg.division = d; e.cfg.speed = sp; e.cfg.human = 0.0f;
            e.cfg.layer[0] = { true, gk::Lead, 0, 0.6f, 1.0f, 1 };
            auto n = render (e, 4, 110);
            if (sp == 0) std::printf ("  %-6s", divNames[d]);
            std::printf (" %s=%3d notes", sp == 0 ? "moitie" : sp == 1 ? "normal" : "double", (int) n.size());
            if (sp == 2) std::printf ("\n");
        }

    // Audit des fausses notes : 4 couches, plusieurs progressions, 40 idees par style.
    //   hors  = note ni dans la gamme ni dans l'accord
    //   frotte = deux notes a un demi-ton (ou une 9e mineure) qui sonnent en meme temps
    std::printf ("\n=== audit fausses notes (4 couches, 40 idees x 5 progressions) ===\n");
    const char* auditProgs[5] = { "Am F C G", "Am Dm E Am", "Dm Bb F C", "C G Am F", "Cm Ab Eb Bb" };
    for (int s = 0; s < gk::kNumStyles; ++s)
    {
        long total = 0, outside = 0, clash = 0, pairs = 0, jumps = 0;
        long by[4][4] = {}; long outBy[4] = {}; int shown = 0;
        long longNotes = 0, longOff = 0;
        for (int pg = 0; pg < 5; ++pg)
            for (int seed = 1; seed <= 40; ++seed)
            {
                gk::Engine e;
                gk::Chord chords[gk::kMaxProg];
                const int n = gk::parseProgression (auditProgs[pg], chords, gk::kMaxProg);
                e.useProg = true;
                e.setProgression (chords, n);
                e.cfg.style = s; e.cfg.seed = seed * 37; e.cfg.human = 0.0f; e.cfg.swing = 0.0f;
                e.cfg.layer[0] = { true, gk::Lead, 0, 0.2f + 0.02f * (float) (seed % 40), 1.0f, seed };
                e.cfg.layer[1] = { true, gk::Answer, 0, 0.5f, 0.85f, seed + 11 };
                e.cfg.layer[2] = { true, gk::Arp, 0, 0.5f, 0.8f, seed + 23 };
                e.cfg.layer[3] = { true, gk::Harmony, 0, 0.5f, 0.75f, seed + 31 };
                const gk::Key k = e.currentKey();
                auto notes = render (e, 4, 110);
                int prevLead = -1;
                for (size_t i = 0; i < notes.size(); ++i)
                {
                    const auto& a = notes[i];
                    ++total;
                    const gk::Chord c = gk::resolveChord (chords[((int) (a.beat / 4.0 + 1.0e-3)) % n], k);
                    if (gk::scaleIndex (k, a.pitch) < 0 && ! gk::pcIn (c.mask, a.pitch))
                    {
                        ++outside; ++outBy[a.ch];
                        if (shown++ < 3)
                        {
                            char cn[16]; gk::chordName (c, cn, sizeof (cn));
                            std::printf ("    hors: %s sur %s (%s, tonalite %s %s, couche %d, duree %.2f)\n", pitchName (a.pitch).c_str(), cn,
                                         auditProgs[pg], gk::noteName (k.root), gk::kScales[k.scale].name, a.ch + 1, a.len);
                        }
                    }
                    // note tenue (croche pointee ou plus) qui n'est pas dans l'accord = ce que l'oreille entend comme faux
                    if (a.ch != 2 && a.len >= 0.7) { ++longNotes; if (! gk::pcIn (c.mask, a.pitch)) ++longOff; }
                    if (a.ch == 0) { if (prevLead >= 0 && std::abs (a.pitch - prevLead) > 12) ++jumps; prevLead = a.pitch; }
                    for (size_t j = i + 1; j < notes.size() && notes[j].beat < a.beat + a.len; ++j)
                    {
                        const auto& b = notes[j];
                        const double overlap = std::min (a.beat + a.len, b.beat + b.len) - b.beat;
                        if (overlap < 0.12) continue;
                        ++pairs;
                        const int iv = std::abs (a.pitch - b.pitch) % 12;
                        // demi-ton ou 9e mineure = ca frotte ; la 7e majeure (11) est une couleur normale d'accord
                        if (iv == 1) { ++clash; ++by[std::min (a.ch, b.ch)][std::max (a.ch, b.ch)]; }
                    }
                }
            }
        std::printf ("    frottements par couches : 1-1=%ld 1-2=%ld 1-3=%ld 1-4=%ld 2-2=%ld 2-3=%ld 2-4=%ld 3-3=%ld 3-4=%ld 4-4=%ld | hors par couche : %ld %ld %ld %ld\n",
                     by[0][0], by[0][1], by[0][2], by[0][3], by[1][1], by[1][2], by[1][3], by[2][2], by[2][3], by[3][3],
                     outBy[0], outBy[1], outBy[2], outBy[3]);
        std::printf ("    notes tenues hors accord : %ld / %ld (%.1f%%)\n", longOff, longNotes,
                     100.0 * (double) longOff / (double) std::max (1L, longNotes));
        std::printf ("  %-16s notes=%6ld  hors=%4ld (%.2f%%)  frotte=%5ld / %6ld paires (%.2f%%)  sauts>octave=%ld\n",
                     files[s], total, outside, 100.0 * (double) outside / (double) std::max (1L, total),
                     clash, pairs, 100.0 * (double) clash / (double) std::max (1L, pairs), jumps);
    }

    char nm[16];
    const char* tests[] = { "Lam", "Sib7", "F#m7", "Do", "Cmaj7", "Dm7b5", "Gsus4", "R\xC3\xA9m", "Ebmaj9" };
    std::printf ("\n=== lecture des accords ===\n  ");
    for (const char* t : tests) { gk::chordName (gk::parseChord (t), nm, sizeof (nm)); std::printf ("%s->%s  ", t, nm); }
    std::printf ("\n");
    return 0;
}
