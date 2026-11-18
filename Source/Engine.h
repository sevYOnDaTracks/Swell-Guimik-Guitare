// Swell Guimik - moteur de generation de melodies guitare (C++17 pur, sans JUCE).
// Tout est deterministe : meme graine + memes reglages = meme phrase a chaque boucle.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace gk
{

constexpr int kTPQ = 24;        // ticks par noire
constexpr int kTPB = 96;        // ticks par mesure (4/4)
constexpr int kMaxBars = 8;
constexpr int kMaxVBars = 16;   // mesures "virtuelles" (vitesse double)
constexpr int kMaxEv = 400;
constexpr int kLayers = 4;
constexpr int kMaxProg = 16;
constexpr int kNumStyles = 5;
constexpr int kNumScales = 9;

//==============================================================================
inline uint32_t mix (uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x85EBCA6Bu + (a << 6) + (a >> 2));
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9E3779B9u) { next(); next(); next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return float (next() & 0xFFFFFF) / float (0x1000000); }
    int range (int lo, int hi) { return lo + int (next() % uint32_t (hi - lo + 1)); }
    bool chance (float p) { return uni() < p; }
};

inline int imod (long long a, int m) { long long r = a % m; return int (r < 0 ? r + m : r); }
inline long long idiv (long long a, int m) { long long q = a / m; if ((a % m) != 0 && a < 0) --q; return q; }
inline bool pcIn (uint16_t mask, int pc) { return ((mask >> imod (pc, 12)) & 1) != 0; }
inline uint16_t pcBit (int pc) { return uint16_t (1u << imod (pc, 12)); }
inline uint16_t rotMask (uint16_t rel, int root)
{
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i) if ((rel >> i) & 1) m |= pcBit (root + i);
    return m;
}

//==============================================================================
// Gammes
struct ScaleDef
{
    const char* name;
    int steps[7];
    uint16_t pent;      // masque relatif a la tonique (0 = gamme complete)
    bool minorFamily;
};

static const ScaleDef kScales[kNumScales] = {
    { "Mineur naturel",       { 0, 2, 3, 5, 7, 8, 10 }, 0,    true  },
    { "Majeur",               { 0, 2, 4, 5, 7, 9, 11 }, 0,    false },
    { "Mineur harmonique",    { 0, 2, 3, 5, 7, 8, 11 }, 0,    true  },
    { "Mineur m\xC3\xA9lodique", { 0, 2, 3, 5, 7, 9, 11 }, 0, true  },
    { "Pentatonique mineure", { 0, 2, 3, 5, 7, 8, 10 }, 1193, true  },
    { "Pentatonique majeure", { 0, 2, 4, 5, 7, 9, 11 }, 661,  false },
    { "Blues",                { 0, 2, 3, 5, 7, 8, 10 }, 1257, true  },
    { "Dorien",               { 0, 2, 3, 5, 7, 9, 10 }, 0,    true  },
    { "Phrygien",             { 0, 1, 3, 5, 7, 8, 10 }, 0,    true  },
};

struct Key { int root = 9; int scale = 0; };

inline int scaleIndex (const Key& k, int pc)
{
    for (int i = 0; i < 7; ++i)
        if (imod (k.root + kScales[k.scale].steps[i], 12) == imod (pc, 12)) return i;
    return -1;
}

//==============================================================================
// Accords
struct Chord
{
    bool valid = false;
    int root = 0, third = -1, fifth = 7, seventh = 0;
    bool ninth = false;
    uint16_t mask = 0;

    bool same (const Chord& o) const
    {
        return valid == o.valid && root == o.root && third == o.third && fifth == o.fifth
            && seventh == o.seventh && ninth == o.ninth && mask == o.mask;
    }
};

inline Chord makeChord (int root, int third, int fifth, int seventh, bool ninth)
{
    Chord c; c.valid = true; c.root = imod (root, 12); c.third = third; c.fifth = fifth;
    c.seventh = seventh; c.ninth = ninth;
    c.mask = uint16_t (pcBit (root) | pcBit (root + fifth));
    if (third > 0) c.mask |= pcBit (root + third);
    if (seventh > 0) c.mask |= pcBit (root + seventh);
    if (ninth) c.mask |= pcBit (root + 2);
    return c;
}

// Analyse un ensemble de notes tenues (classes de hauteur + basse).
inline Chord analyseChord (uint16_t pcs, int bassPc)
{
    Chord c;
    int n = 0;
    for (int i = 0; i < 12; ++i) n += (pcs >> i) & 1;
    if (n == 0) return c;
    c.valid = true;
    if (n == 1) { c.root = imod (bassPc, 12); c.third = -1; c.fifth = 7; c.mask = pcs; return c; }

    float best = -100.0f; int bestRoot = imod (bassPc, 12);
    for (int r = 0; r < 12; ++r)
    {
        if (! pcIn (pcs, r)) continue;
        auto H = [&] (int iv) { return pcIn (pcs, r + iv); };
        float s = 0.0f;
        if (H (7)) s += 2.0f;
        if (H (4) || H (3)) s += 2.0f;
        if (H (10) || H (11)) s += 1.0f;
        if (r == imod (bassPc, 12)) s += 1.5f;
        if (H (3) && H (6) && ! H (7)) s += 1.0f;
        if (! H (3) && ! H (4) && (H (5) || H (2)) && H (7)) s += 1.0f;
        if (H (1)) s -= 2.0f;
        if (H (3) && H (4)) s -= 1.0f;
        if (H (6) && H (7)) s -= 0.5f;
        if (s > best) { best = s; bestRoot = r; }
    }
    const int r = bestRoot;
    auto H = [&] (int iv) { return pcIn (pcs, r + iv); };
    c.root = r;
    c.third = H (4) ? 4 : H (3) ? 3 : H (5) ? 5 : H (2) ? 2 : -1;
    c.fifth = H (7) ? 7 : H (6) ? 6 : H (8) ? 8 : 7;
    c.seventh = H (10) ? 10 : H (11) ? 11 : (H (9) && c.fifth == 6 && c.third == 3) ? 9 : 0;
    c.ninth = H (2) && c.third != 2;
    c.mask = uint16_t (pcs | pcBit (r + c.fifth));
    return c;
}

// Complete un accord sans tierce (note seule, power chord) a partir de la gamme.
inline Chord resolveChord (Chord c, const Key& k)
{
    if (! c.valid || c.third > 0) return c;
    int cnt = 0;
    for (int i = 0; i < 12; ++i) cnt += (c.mask >> i) & 1;
    const int idx = scaleIndex (k, c.root);
    int t = 4, f = c.fifth;
    if (idx >= 0)
    {
        const int* S = kScales[k.scale].steps;
        t = imod (S[(idx + 2) % 7] - S[idx], 12);
        if (cnt == 1) f = imod (S[(idx + 4) % 7] - S[idx], 12);
    }
    c.third = t; c.fifth = f;
    c.mask |= uint16_t (pcBit (c.root + t) | pcBit (c.root + f));
    return c;
}

//==============================================================================
// Lecture d'une progression ecrite : "Am F C G", "Lam Fa Do Sol", "Dm7 | G7 | Cmaj7"
namespace detail
{
    inline bool sw (const char* s, const char* pre) { return std::strncmp (s, pre, std::strlen (pre)) == 0; }
    inline bool swi (const char* s, const char* pre)
    {
        for (; *pre; ++s, ++pre)
            if (std::tolower ((unsigned char) *s) != std::tolower ((unsigned char) *pre)) return false;
        return true;
    }
}

inline Chord parseChord (const char* t)
{
    using namespace detail;
    Chord bad;
    int root = -1;
    const char* p = t;
    // noms francais d'abord
    if (swi (p, "sol")) { root = 7; p += 3; }
    else if (swi (p, "do")) { root = 0; p += 2; }
    else if (swi (p, "r\xC3\xA9") || swi (p, "r\xC3\x89")) { root = 2; p += 3; }
    else if (swi (p, "re")) { root = 2; p += 2; }
    else if (swi (p, "mi")) { root = 4; p += 2; }
    else if (swi (p, "fa")) { root = 5; p += 2; }
    else if (swi (p, "la")) { root = 9; p += 2; }
    else if (swi (p, "si")) { root = 11; p += 2; }
    else
    {
        static const int letters[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        const char ch = (char) std::toupper ((unsigned char) *p);
        if (ch < 'A' || ch > 'G') return bad;
        root = letters[ch - 'A']; ++p;
    }
    if (*p == '#') { ++root; ++p; }
    else if (sw (p, "\xE2\x99\xAF")) { ++root; p += 3; }
    else if (sw (p, "\xE2\x99\xAD")) { --root; p += 3; }
    else if (*p == 'b' && ! swi (p, "b5")) { --root; ++p; }
    while (*p == ' ') ++p;

    if (swi (p, "maj9") || sw (p, "M9")) return makeChord (root, 4, 7, 11, true);
    if (swi (p, "maj7") || sw (p, "M7")) return makeChord (root, 4, 7, 11, false);
    if (swi (p, "maj")) return makeChord (root, 4, 7, 0, false);
    if (swi (p, "m7b5")) return makeChord (root, 3, 6, 10, false);
    if (swi (p, "dim7")) return makeChord (root, 3, 6, 9, false);
    if (swi (p, "dim")) return makeChord (root, 3, 6, 0, false);
    if (swi (p, "aug") || *p == '+') return makeChord (root, 4, 8, 0, false);
    if (swi (p, "sus2")) return makeChord (root, 2, 7, 0, false);
    if (swi (p, "sus")) return makeChord (root, 5, 7, 0, false);
    if (swi (p, "madd9")) return makeChord (root, 3, 7, 0, true);
    if (swi (p, "min9") || sw (p, "m9")) return makeChord (root, 3, 7, 10, true);
    if (swi (p, "min7") || sw (p, "m7") || sw (p, "-7")) return makeChord (root, 3, 7, 10, false);
    if (swi (p, "min") || *p == 'm' || *p == '-') return makeChord (root, 3, 7, 0, false);
    if (swi (p, "add9")) return makeChord (root, 4, 7, 0, true);
    if (*p == '9') return makeChord (root, 4, 7, 10, true);
    if (*p == '7') return makeChord (root, 4, 7, 10, false);
    if (*p == '5') { Chord c = makeChord (root, -1, 7, 0, false); return c; }
    return makeChord (root, 4, 7, 0, false);
}

inline int parseProgression (const char* text, Chord* out, int maxOut)
{
    int n = 0;
    const char* p = text;
    while (*p && n < maxOut)
    {
        while (*p == ' ' || *p == '\t' || *p == '|' || *p == ',' || *p == ';' || *p == '\n' || *p == '\r') ++p;
        if (! *p) break;
        char tok[24]; int len = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '|' && *p != ',' && *p != ';' && *p != '\n' && *p != '\r')
        {
            if (len < 23) tok[len++] = *p;
            ++p;
        }
        tok[len] = 0;
        if (tok[0] == '%' && n > 0) { out[n] = out[n - 1]; ++n; continue; }
        const Chord c = parseChord (tok);
        if (c.valid) out[n++] = c;
    }
    return n;
}

inline const char* noteName (int pc)
{
    static const char* names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[imod (pc, 12)];
}

inline void chordName (const Chord& c, char* buf, size_t size)
{
    if (! c.valid) { std::snprintf (buf, size, "-"); return; }
    const char* q = "";
    if (c.third == 3 && c.fifth == 6) q = c.seventh == 10 ? "m7b5" : c.seventh == 9 ? "dim7" : "dim";
    else if (c.third == 3) q = c.seventh == 10 ? (c.ninth ? "m9" : "m7") : c.seventh == 11 ? "mMaj7" : (c.ninth ? "madd9" : "m");
    else if (c.third == 4 && c.fifth == 8) q = "aug";
    else if (c.third == 4) q = c.seventh == 11 ? (c.ninth ? "maj9" : "maj7") : c.seventh == 10 ? (c.ninth ? "9" : "7") : (c.ninth ? "add9" : "");
    else if (c.third == 5) q = "sus4";
    else if (c.third == 2) q = "sus2";
    else q = "5";
    std::snprintf (buf, size, "%s%s", noteName (c.root), q);
}

//==============================================================================
// Detection automatique de la tonalite a partir des accords joues
struct KeyDetector
{
    float pcW[12] {}, rootW[12] {};
    int firstRoot = -1;

    void reset() { for (int i = 0; i < 12; ++i) { pcW[i] = 0; rootW[i] = 0; } firstRoot = -1; }

    void add (const Chord& c)
    {
        if (! c.valid) return;
        for (int i = 0; i < 12; ++i) { pcW[i] *= 0.92f; rootW[i] *= 0.92f; }
        for (int i = 0; i < 12; ++i) if ((c.mask >> i) & 1) pcW[i] += 1.0f;
        rootW[c.root] += 1.0f;
        if (firstRoot < 0) firstRoot = c.root;
    }

    Key estimate() const
    {
        Key best; float bs = -1.0e9f;
        for (int r = 0; r < 12; ++r)
            for (int sc = 0; sc < 2; ++sc)
            {
                Key k { r, sc };
                float s = 0.0f;
                for (int pc = 0; pc < 12; ++pc)
                    s += (scaleIndex (k, pc) >= 0 ? 1.0f : -1.5f) * pcW[pc];
                s += 0.6f * rootW[r] + 0.25f * rootW[(r + 7) % 12];
                if (firstRoot == r) s += 0.5f;
                if (sc == 0) s += 0.1f;
                if (s > bs) { bs = s; best = k; }
            }
        return best;
    }
};

//==============================================================================
// Styles. Chaque style a 6 phrases ("licks") ecrites a la main, de la plus aeree a la plus dense :
//   r1/d1 = mesure question, r2/d2 = mesure reponse, r3/d3 = mesure de fin de phrase.
// Rythme sur 16 doubles-croches :
//   X note forte   x note   h hammer-on / pull-off (note liee)   s slide
//   d double-corde en tierce   D double-corde en sixte   g note etouffee
//   - tenue   . silence   t_ roulement en triolet (3 notes)
// Degres : un nombre par note.
//   pent     -> indices de pentatonique (5 par octave), sinon degres de la gamme (7 par octave)
//   chordRel -> comptes depuis la fondamentale de l'accord (la ligne suit les accords),
//               sinon depuis la tonique : le gimmick reste le meme et s'adapte juste aux accords.
struct Lick { const char* r1; const char* d1; const char* r2; const char* d2; const char* r3; const char* d3; };

struct StyleDef
{
    const char* name;
    Lick licks[6];
    const char* arp[3];
    const char* bass[2];
    bool pent, chordRel;
    float gate;
    bool ring;
    int base;       // hauteur visee pour la tonique (ou la fondamentale)
};

static const StyleDef kStyles[kNumStyles] = {
    { "Afrobeats",
      { { "X..x..x.....x.x.", "7 6 5 4 5",           "X..x..X-....x.x.", "6 5 4 2 3",         "X..x..X---......", "6 4 5" },
        { "..x.x.X..x.h.x..", "4 5 7 5 6 5",         "..x.x.X..x.X---.", "4 5 7 6 5",         "..x.x.X..h.x.X--", "4 5 7 6 5 5" },
        { "d..d..d...d.d...", "7 6 5 6 7",           "d..d..d.....x.x.", "8 7 6 4 5",         "d..d..D---......", "8 7 5" },
        { "X.hx..x.X.hx..x.", "5 6 5 4 5 6 5 7",     "X.hx..x.X..x.x..", "7 8 7 6 5 4 5",     "X.hx..x.X-------", "7 8 7 6 5" },
        { "x.xhx.x.X..x.hx.", "4 5 6 5 4 5 7 6 5",   "x.xhx.x.X.x.X...", "4 5 6 5 4 5 6 7",   "x.xhx.x.X.x.X---", "4 5 6 5 4 6 5 5" },
        { "X.hxh.x.X.hxh.x.", "7 8 7 6 5 7 8 7 6 5", "X.hxh.x.d..d..d.", "6 7 6 5 4 5 6 7",   "X.hxh.x.d..d.X--", "6 7 6 5 4 6 5 5" } },
      { "0..3.4.5..3.4.3.", "0.3.4.3.0.3.4.3.", "0.34.5.40.34.5.4" },
      { "0..0..0...0.0...", "0.....0...0....." },
      true, false, 0.60f, false, 60 },

    { "Rumba / soukous",
      { { "X..x..x.X..x..x.", "9 7 4 9 7 4",                     "X..x..x.X.x.x...", "8 7 4 6 7 9",             "X..x..x.X-------", "8 7 6 7" },
        { "X.x.x.x.X.x.x.x.", "4 7 9 7 4 7 9 7",                 "X.x.x.x.X.x.x.x.", "4 7 9 7 8 7 6 4",         "X.x.x.x.X.x.X---", "4 7 9 7 6 5 4" },
        { "d.d.d..d.d.d.d..", "9 8 7 8 9 8 7",                   "d.d.d..d.d.d.D--", "9 8 7 6 7 8 9",           "d.d.d..d.d.D----", "9 8 7 6 8 7" },
        { "X.xhx.x.X.xhx.x.", "9 7 8 7 4 9 7 8 7 4",             "X.xhx.x.X.x.x.x.", "9 7 8 7 4 5 6 7 9",       "X.xhx.x.X.x.x.x.", "9 7 8 7 4 7 6 5 4" },
        { "X.xxX.xxX.xxX.xx", "9 4 7 9 4 7 8 4 7 8 4 7",         "X.xxX.xxX.x.x.x.", "9 4 7 9 4 7 8 7 6 4",     "X.xxX.xxX.x.X---", "9 4 7 9 4 7 8 9 7" },
        { "XxhxXxhxXxhxXxhx", "7 9 8 7 4 7 6 4 7 9 8 7 4 7 6 4", "XxhxXxhxX.x.x.x.", "7 9 8 7 4 7 6 4 2 4 7 9", "XxhxXxhxX.x.X---", "7 9 8 7 4 7 6 4 5 6 7" } },
      { "0.34.5.40.34.5.4", "0.3.4.5.4.3.4.5.", "0345435403454354" },
      { "0.....0.0.....0.", "0..0..0...0.0..." },
      false, true, 0.80f, false, 60 },

    { "Rap m\xC3\xA9lodique",
      { { "X---x-x-X-----x-", "4 2 1 0 -1",              "X---x-x-X-------", "2 1 0 1",             "X---x-x-X-------", "2 1 -1 0" },
        { "X-x-x-h-X---..x-", "4 7 6 5 4 2",             "X-x-x-h-X-------", "1 4 3 2 1",           "X-x-x-h-X-------", "1 4 3 2 0" },
        { "X-x-x-x-X-x-h-x-", "0 2 4 7 8 7 6 4",         "X-x-x-x-X---x-h-", "0 2 4 7 9 8 7",       "X-x-x-x-X-------", "0 2 4 6 4" },
        { "X-xhx-x-X-xhx-..", "7 8 9 8 7 4 5 6 5",       "X-xhx-x-X---x-x-", "7 8 9 8 7 4 2 1",     "X-xhx-x-X-------", "7 8 9 8 6 7" },
        { "X-x-t_x-X-x-t_x-", "4 2 1 2 4 2 4 2 1 2 4 5", "X-x-t_x-X---x---", "4 2 1 2 4 2 1 0",     "X-x-t_x-X---x---", "4 2 1 2 4 2 -1 0" },
        { "XxhxX-x-XxhxX-x-", "4 5 6 5 4 2 4 5 6 5 4 7", "XxhxX-x-X-x-x-x-", "4 5 6 5 4 2 1 2 1 0", "XxhxX-x-X-------", "4 5 6 5 4 2 0" } },
      { "0-3-4-3-5-3-4-3-", "0.3.4.5.4.3.4.5.", "0-345-430-345-43" },
      { "0-------0-......", "0-----0-..0-...." },
      false, false, 0.90f, false, 60 },

    { "Trap guitare",
      { { "X-----x-X-----..", "0 4 5",                 "X-----x-X-------", "5 4 2",         "X-----x-X-------", "5 4 0" },
        { "X--x--x-X--x--..", "7 4 5 4 2",             "X--x--x-X-------", "7 4 5 4",       "X--x--x-X---x---", "7 4 5 4 0" },
        { "X-x-..X-x-..X-x-", "0 0 2 1 0 -1",          "X-x-..X-x-..X---", "0 0 2 1 0",     "X-x-..X-x-..X---", "0 0 5 4 0" },
        { "X-xhX---X-xhX---", "4 5 6 5 4 2 3 2",       "X-xhX---x-x-X---", "4 5 6 5 4 2 0", "X-xhX---X-------", "4 5 6 5 4" },
        { "X-x-x-x-X-x-x-x-", "0 4 7 4 5 4 7 4",       "X-x-x-x-X-x-X---", "0 4 7 4 5 4 2", "X-x-x-x-X-x-X---", "0 4 7 4 2 1 0" },
        { "X-xxX-x-t_x-X-x-", "7 6 5 4 2 4 5 4 2 0 2", "X-xxX-x-X-------", "7 6 5 4 2 0",   "X-xxX-x-X---x-X-", "7 6 5 4 2 0 -1 0" } },
      { "0-3-4-3-0-3-4-3-", "0--3--4-0--3--4-", "0-3-5-3-4-3-5-3-" },
      { "0-----0-....0-..", "0--0--0-.......0" },
      false, false, 0.75f, false, 52 },

    { "M\xC3\xA9lancolique",
      { { "X-------x---x---", "4 2 1",             "X-----------x---", "0 -1",      "X---------------", "0" },
        { "X---x---X-----x-", "7 6 4 2",           "X---x---X-------", "3 2 1",     "X---x---X-------", "3 2 0" },
        { "X-----x-x---X---", "2 1 2 4",           "X-----x-x---X---", "5 4 2 1",   "X-----x-x---X---", "5 4 2 0" },
        { "X---s---X-x-x---", "4 7 6 5 4",         "X---x-x-X-------", "2 4 3 2",   "X---x-x-X-------", "2 1 -1 0" },
        { "X-x-x---X-x-h-x-", "0 2 4 7 8 7 4",     "X-x-x---X-------", "0 2 3 2",   "X-x-x---X-------", "0 2 1 0" },
        { "X-x-x-x-X-x-xhx-", "4 2 0 2 4 7 8 9 8", "X-x-x-x-X-------", "7 6 4 2 1", "X-x-x-x-X---x---", "7 6 4 2 1 0" } },
      { "0-3-4-5-4-3-4-3-", "0---3-4-5---4-3-", "0-4-3-5-4-3-5-4-" },
      { "0---------------", "0-----------0---" },
      false, false, 1.00f, true, 65 },
};

//==============================================================================
enum Role { Lead = 0, Answer, Harmony, Arp, Bass, kNumRoles };
enum : uint8_t { fChord = 1, fDouble = 2, fSlide = 4, fHammer = 8, fGhost = 16, fArp = 32, fSixth = 64, fHarm = 128 };

struct Ev { int16_t tick; int16_t len; int8_t deg; uint8_t vel; uint8_t flags; };

struct Phrase
{
    int count = 0, bars = 4;
    int degMin = 0, degMax = 0;     // etendue de la phrase, pour la placer d'un bloc sur le manche
    std::array<Ev, kMaxEv> ev {};
    std::array<int16_t, kMaxBars * kTPB> at {};

    void clear (int b) { count = 0; bars = b; degMin = 127; degMax = -128; at.fill (-1); }

    int add (int tick, int len, int deg, int vel, int flags)
    {
        if (count >= kMaxEv || tick < 0 || tick >= bars * kTPB || at[(size_t) tick] >= 0) return -1;
        if (! (flags & fArp)) { degMin = std::min (degMin, deg); degMax = std::max (degMax, deg); }
        ev[(size_t) count] = { (int16_t) tick, (int16_t) len, (int8_t) deg,
                               (uint8_t) std::clamp (vel, 1, 127), (uint8_t) flags };
        at[(size_t) tick] = (int16_t) count;
        return count++;
    }
};

struct BarNotes
{
    static constexpr int cap = 48;
    int n = 0;
    int tick[cap] {}, len[cap] {};
    int8_t deg[cap] {};
    bool acc[cap] {};
    char sym[cap] {};

    void push (int t, int l, bool a, char s, int d = 0)
    {
        if (n >= cap) return;
        tick[n] = t; len[n] = l; acc[n] = a; sym[n] = s; deg[n] = (int8_t) d; ++n;
    }
};

inline void parseRhythm (const char* t, BarNotes& o)
{
    o.n = 0;
    int L = (int) std::strlen (t);
    if (L > 16) L = 16;
    for (int i = 0; i < L; ++i)
    {
        const char ch = t[i];
        const bool onset = ch == 'X' || ch == 'x' || ch == 'h' || ch == 's' || ch == 'd' || ch == 'D' || ch == 'g'
                        || (ch >= '0' && ch <= '9');
        if (onset)
        {
            int steps = 1;
            while (i + steps < L && t[i + steps] == '-') ++steps;
            o.push (i * 6, steps * 6, ch == 'X' || ch == 'd' || ch == 'D' || ch == '0', ch);
            i += steps - 1;
        }
        else if (ch == 't')
        {
            for (int k = 0; k < 3; ++k) o.push (i * 6 + k * 4, 4, false, 'x');
            ++i;
        }
    }
}

inline void parseLick (const char* rhythm, const char* degs, BarNotes& o)
{
    parseRhythm (rhythm, o);
    const char* p = degs;
    int last = 0;
    for (int i = 0; i < o.n; ++i)
    {
        char* end = nullptr;
        const long v = std::strtol (p, &end, 10);
        if (end != p) { last = (int) v; p = end; }
        o.deg[i] = (int8_t) last;
    }
}

// Ramene les notes sur la division choisie (1/4, 1/8, triolets...).
inline void quantize (BarNotes& b, int grid, Rng& r, float density)
{
    if (grid <= 0) return;
    BarNotes q;
    for (int i = 0; i < b.n; ++i)
    {
        int t = ((b.tick[i] + grid / 2) / grid) * grid;
        if (t >= kTPB) continue;
        if (q.n > 0 && q.tick[q.n - 1] == t) { q.acc[q.n - 1] = q.acc[q.n - 1] || b.acc[i]; continue; }
        const bool triplet = (grid == 8 || grid == 4);
        if (triplet && b.acc[i] && q.n > 0 && t - grid > q.tick[q.n - 1] && r.chance (0.2f + 0.45f * density))
            q.push (t - grid, grid, false, 'x', b.deg[i] - 1);   // levee en triolet avant l'accent
        q.push (t, std::max (b.len[i], grid), b.acc[i], b.sym[i], b.deg[i]);
    }
    for (int i = 0; i < q.n; ++i)
    {
        const int gap = (i + 1 < q.n ? q.tick[i + 1] : kTPB) - q.tick[i];
        q.len[i] = std::min (std::max (q.len[i], grid), gap);
    }
    b = q;
}

//==============================================================================
struct LayerCfg
{
    bool on = false;
    int role = Lead;
    int octave = 0;
    float density = 0.5f;
    float vel = 1.0f;
    int seed = 1;
};

struct Config
{
    int style = 0;
    int keyRoot = -1;       // -1 = auto
    int scale = 0;
    int seed = 1;
    int bars = 4;
    int division = 0;       // 0 auto, 1 = 1/4, 2 = 1/8, 3 = 1/8T, 4 = 1/16, 5 = 1/16T
    int speed = 1;          // 0 moitie, 1 normale, 2 double
    float swing = 0.0f, human = 0.3f;
    float artHammer = 0.5f, artSlide = 0.5f, artDouble = 0.5f, artGhost = 0.5f;
    LayerCfg layer[kLayers];
};

struct Realized { int n = 0; int pitch[2] { 0, 0 }; int grace = -1; };

//==============================================================================
class Engine
{
public:
    struct Out { int sample; uint8_t status, d1, d2; };

    Config cfg;
    Chord live;
    bool useProg = false;
    Chord prog[kMaxProg];
    int progLen = 0;
    Key autoKey { 9, 0 }, progKey { 9, 0 };
    Phrase phrase[kLayers];
    std::array<Out, 1024> out {};
    int outCount = 0;

    Engine() { for (auto& p : pend) p.used = false; std::memset (sounding, 0, sizeof (sounding)); rebuild(); }

    void setSampleRate (double s) { sr = s > 1000.0 ? s : 44100.0; }

    void setLive (const Chord& c)
    {
        int cnt = 0;
        for (int i = 0; i < 12; ++i) cnt += (c.mask >> i) & 1;
        // changement d'accord joue en direct : on coupe ce qui resonne de l'accord precedent
        if (c.valid && live.valid && cnt >= 2 && (c.root != live.root || c.third != live.third)) cutRinging = true;
        live = c;
    }

    // La tonalite ne se calcule que sur un accord complet (3 notes ou plus) et reste stable :
    // l'appelant ne confirme l'accord qu'une fois qu'il est tenu, pas pendant qu'on le plaque.
    void confirmForKey (const Chord& c)
    {
        int cnt = 0;
        for (int i = 0; i < 12; ++i) cnt += (c.mask >> i) & 1;
        if (c.valid && cnt >= 3) { det.add (c); autoKey = det.estimate(); }
    }

    void setProgression (const Chord* chords, int n)
    {
        progLen = std::clamp (n, 0, kMaxProg);
        KeyDetector d;
        for (int i = 0; i < progLen; ++i) { prog[i] = chords[i]; d.add (chords[i]); }
        if (progLen > 0) progKey = d.estimate();
    }

    Key currentKey() const
    {
        const Key d = useProg ? progKey : autoKey;
        if (cfg.keyRoot >= 0) return Key { cfg.keyRoot, cfg.scale };
        const bool wantMinor = kScales[cfg.scale].minorFamily;
        const bool isMinor = kScales[d.scale].minorFamily;
        return Key { d.root, wantMinor == isMinor ? cfg.scale : d.scale };
    }

    Chord chordAtBar (long long bar) const
    {
        if (! useProg) return live;
        if (progLen <= 0) return Chord {};
        return prog[imod (bar, progLen)];
    }

    // Reconstruit les phrases si un reglage qui les concerne a change.
    void update()
    {
        cfg.style = std::clamp (cfg.style, 0, kNumStyles - 1);
        cfg.scale = std::clamp (cfg.scale, 0, kNumScales - 1);
        cfg.bars = std::clamp (cfg.bars, 1, kMaxBars);
        cfg.division = std::clamp (cfg.division, 0, 5);
        cfg.speed = std::clamp (cfg.speed, 0, 2);
        bool dirty = cfg.style != built.style || cfg.seed != built.seed || cfg.bars != built.bars
                  || cfg.division != built.division || cfg.speed != built.speed
                  || cfg.artHammer != built.artHammer || cfg.artSlide != built.artSlide
                  || cfg.artDouble != built.artDouble || cfg.artGhost != built.artGhost;
        for (int l = 0; l < kLayers && ! dirty; ++l)
            dirty = cfg.layer[l].role != built.layer[l].role || cfg.layer[l].density != built.layer[l].density
                 || cfg.layer[l].seed != built.layer[l].seed || cfg.layer[l].on != built.layer[l].on;
        if (dirty) rebuild();
    }

    void rebuild()
    {
        built = cfg;
        const int bars = std::clamp (cfg.bars, 1, kMaxBars);
        vBars = cfg.speed == 0 ? std::max (1, bars / 2) : cfg.speed == 2 ? bars * 2 : bars;
        num = cfg.speed == 0 ? 2 : 1;
        den = cfg.speed == 2 ? 2 : 1;
        static const int grids[6] = { 0, 24, 12, 8, 6, 4 };
        grid = grids[std::clamp (cfg.division, 0, 5)];
        std::memset (leadOcc, 0, sizeof (leadOcc));
        for (auto& p : phrase) p.clear (bars);

        int leadIdx = -1;
        for (int l = 0; l < kLayers; ++l) if (cfg.layer[l].role == Lead && cfg.layer[l].on) { leadIdx = l; break; }
        if (leadIdx < 0)
            for (int l = 0; l < kLayers; ++l) if (cfg.layer[l].role == Lead) { leadIdx = l; break; }

        if (leadIdx >= 0) buildLead (leadIdx, true);
        for (int l = 0; l < kLayers; ++l)
        {
            if (l == leadIdx) continue;
            switch (cfg.layer[l].role)
            {
                case Lead:    buildLead (l, false); break;
                case Answer:  buildAnswer (l); break;
                case Harmony:
                    if (leadIdx >= 0) phrase[l] = phrase[leadIdx]; else buildLead (l, false);
                    markHarmony (l);
                    break;
                case Arp:     buildArp (l, false); break;
                default:      buildArp (l, true); break;
            }
        }
    }

    //==========================================================================
    // Transforme un evenement abstrait (degre) en hauteurs MIDI pour l'accord courant.
    Realized realize (const Ev& e, const Chord& c, const Key& key, int l) const
    {
        Realized r;
        const StyleDef& st = kStyles[cfg.style];
        const int role = cfg.layer[l].role;

        if (e.flags & fArp)
        {
            const int third = c.third > 0 ? c.third : 7;
            int v[6] = { 0, c.fifth, 12, 12 + third, 12 + c.fifth, 24 };
            if (c.seventh > 0) v[2] = c.seventh;
            if (c.ninth) v[5] = 26;
            const int center = (role == Bass ? 40 : 45) + 12 * cfg.layer[l].octave;
            int rootPitch = nearestPc (center, c.root);
            if (rootPitch < 40 && cfg.layer[l].octave >= 0) rootPitch += 12;
            int p = rootPitch + v[std::clamp ((int) e.deg, 0, 5)];
            while (p > 88) p -= 12;
            while (p < 28) p += 12;
            r.pitch[r.n++] = p;
            return r;
        }

        int base = st.base + 12 * cfg.layer[l].octave;
        if (role == Answer && base < 62) base += 12;
        // Registre : on decale la phrase ENTIERE d'une octave si elle depasse du manche.
        // (Replier note par note cassait la melodie avec des sauts d'octave au milieu d'une phrase.)
        const Phrase& ph = phrase[l];
        if (ph.degMax >= ph.degMin)
        {
            const int per = (st.pent && ! st.chordRel) ? 5 : 7;
            const int hi = (ph.degMax * 12 + per - 1) / per + ((e.flags & fHarm) ? 4 : 0) + 6;
            const int lo = (int) idiv ((long long) ph.degMin * 12, per) - 6;
            while (base + hi > 88 && base + lo - 12 >= 40) base -= 12;
            while (base + lo < 40 && base + hi + 12 <= 88) base += 12;
        }
        const bool strong = (e.flags & fChord) != 0;
        int main;
        if (st.chordRel)
        {
            main = pitchForDeg (c, key, nearestPc (base, c.root), e.deg, strong, e.len);
        }
        else
        {
            const int tonic = nearestPc (base, key.root);
            if (st.pent)
            {
                static const int pm[5] = { 0, 3, 5, 7, 10 }, pM[5] = { 0, 2, 4, 7, 9 };
                const int* P = kScales[key.scale].minorFamily ? pm : pM;
                const int oct = (int) idiv (e.deg, 5);
                main = tonic + P[e.deg - oct * 5] + 12 * oct;
            }
            else
            {
                const int* S = kScales[key.scale].steps;
                const int oct = (int) idiv (e.deg, 7);
                main = tonic + S[e.deg - oct * 7] + 12 * oct;
            }
            // Regle de securite : une note forte ou tenue (noire ou plus) se pose TOUJOURS sur une note
            // de l'accord. Seules les notes de passage restent des notes de la gamme.
            const bool anchor = strong || e.len >= 20;
            main = anchor ? chordToneNear (main, c.mask, 0) : adapt (main, c, key, false, e.len);
        }
        const bool anchor = strong || e.len >= 20;
        // tierces / sixtes : la note ajoutee suit la meme regle, au-dessus de la melodie
        if (e.flags & fHarm)
        {
            const int h = scaleShift (main, key, 2);
            main = anchor ? chordToneNear (h, c.mask, main + 3) : adapt (h, c, key, false, e.len);
        }
        main = fold (main);
        r.pitch[r.n++] = main;

        if (e.flags & fDouble)
        {
            // double-corde : toujours une note de l'accord, une tierce ou une sixte sous la melodie
            const int want = scaleShift (main, key, (e.flags & fSixth) ? -5 : -2);
            const int d2 = fold (chordToneNear (want, c.mask, 0));
            if (d2 < main - 2 && main - d2 <= 10) r.pitch[r.n++] = d2;
        }
        if (e.flags & fSlide)
        {
            const int g = scaleShift (main, key, -1);       // le slide part toujours d'une note de la gamme
            if (g < main && main - g <= 3) r.grace = g;
        }
        return r;
    }

    //==========================================================================
    // Rendu temps reel. Les echeances "due" sont en echantillons depuis le debut du bloc.
    void renderSpan (double tickA, double ticksPerSample, int offset, int numSamples, bool generate)
    {
        if (numSamples <= 0) return;
        if (cutRinging)
        {
            cutRinging = false;
            const int soon = offset + int (0.03 * sr);
            for (auto& p : pend) if (p.used && p.isOff && p.due > soon) p.due = soon;
        }
        const double tickB = tickA + numSamples * ticksPerSample;
        if (generate)
        {
            long long k0 = (long long) std::ceil (tickA - 1.0e-9);
            if (std::fabs (tickA - expectedNext) < 0.25) k0 = lastTick + 1;
            const long long kEnd = (long long) std::ceil (tickB - 1.0e-9);
            const double spt = 1.0 / ticksPerSample;
            for (long long k = k0; k < kEnd; ++k)
            {
                int s = (int) ((double (k) - tickA) * spt);
                s = std::clamp (s, 0, numSamples - 1);
                fireTick (k, offset + s, spt);
            }
            lastTick = kEnd - 1;
            expectedNext = tickB;
        }
        else
        {
            expectedNext = -1.0e18;
        }
        flush (offset + numSamples);
    }

    void endBlock (int numSamples)
    {
        for (auto& p : pend) if (p.used) p.due -= numSamples;
    }

    void flushAll (int sample)
    {
        for (auto& p : pend) p.used = false;
        for (int ch = 0; ch < kLayers; ++ch)
            for (int n = 0; n < 128; ++n)
                if (sounding[ch][n] > 0) { emit (sample, uint8_t (0x80 | ch), (uint8_t) n, 0); sounding[ch][n] = 0; }
        expectedNext = -1.0e18;
    }

private:
    struct Pend { int due; uint8_t ch, pitch, vel; bool isOff, used; };

    Config built;
    KeyDetector det;
    double sr = 44100.0;
    std::array<Pend, 512> pend {};
    uint8_t sounding[kLayers][128];
    long long lastTick = -1;
    double expectedNext = -1.0e18;
    bool cutRinging = false;
    int vBars = 4, num = 1, den = 1, grid = 0;
    bool leadOcc[kMaxVBars][kTPB];

    static int nearestPc (int target, int pc)
    {
        int diff = imod (pc - target, 12);
        if (diff > 6) diff -= 12;
        return target + diff;
    }

    static int fold (int p)
    {
        while (p < 40) p += 12;
        while (p > 88) p -= 12;
        return p;
    }

    static int snapTo (int p, uint16_t mask)
    {
        if (mask == 0) return p;
        for (int d = 1; d <= 6; ++d)
        {
            if (pcIn (mask, p - d)) return p - d;
            if (pcIn (mask, p + d)) return p + d;
        }
        return p;
    }

    // Note de l'accord la plus proche de p (au-dessus de minPitch si demande).
    static int chordToneNear (int p, uint16_t mask, int minPitch)
    {
        if (mask == 0) return p;
        for (int d = 0; d <= 6; ++d)
        {
            if (pcIn (mask, p - d) && p - d >= minPitch) return p - d;
            if (pcIn (mask, p + d) && p + d >= minPitch) return p + d;
        }
        for (int q = std::max (p, minPitch); q < minPitch + 12; ++q) if (pcIn (mask, q)) return q;
        return p;
    }

    // Deplace une note de n degres dans la gamme (tierce = 2, sixte en dessous = -5).
    static int scaleShift (int p, const Key& key, int n)
    {
        int idx = scaleIndex (key, p);
        for (int d = 1; idx < 0 && d <= 2; ++d)
        {
            if (scaleIndex (key, p - d) >= 0) { p -= d; idx = scaleIndex (key, p); }
            else if (scaleIndex (key, p + d) >= 0) { p += d; idx = scaleIndex (key, p); }
        }
        if (idx < 0) return p + (n > 0 ? 4 : -3);
        const int* S = kScales[key.scale].steps;
        const int k = idx + n;
        const int oct = (int) idiv (k, 7);
        return p + S[k - oct * 7] + 12 * oct - S[idx];
    }

    // Le gimmick reste dans la tonalite ; on ne corrige que ce qui frotte avec l'accord.
    static int adapt (int p, const Chord& c, const Key& key, bool strong, int len)
    {
        if (pcIn (c.mask, p)) return p;
        const bool up = pcIn (c.mask, p + 1), down = pcIn (c.mask, p - 1);
        if (up && scaleIndex (key, p + 1) < 0) return p + 1;        // note de l'accord hors gamme (ex. sol# sur Mi majeur)
        // note a eviter (demi-ton au-dessus d'une note de l'accord) : seules les notes de passage
        // tres courtes ont le droit de frotter, tout le reste se pose sur la note de l'accord
        if (down)
        {
            if (strong || len > 6) return p - 1;
            // note de passage tres courte : on la monte d'un degre pour garder le mouvement sans frotter
            const int q = scaleShift (p, key, 1);
            return (pcIn (c.mask, q) || ! pcIn (c.mask, q - 1)) ? q : p - 1;
        }
        if (strong && len >= 36) return snapTo (p, c.mask);         // longue note forte : on se pose sur l'accord
        return p;
    }

    // Mode "suit les accords" (rumba / soukous) : degres comptes depuis la fondamentale.
    static int pitchForDeg (const Chord& c, const Key& key, int rootPitch, int deg, bool snap, int len)
    {
        static const int locrian[7] = { 0, 1, 3, 5, 6, 8, 10 };
        const int* sc = kScales[key.scale].steps;
        int idx = scaleIndex (key, c.root);
        if (idx < 0)
        {
            sc = c.third == 3 ? (c.fifth == 6 ? locrian : kScales[0].steps) : kScales[1].steps;
            idx = 0;
        }
        const int k = idx + deg;
        const int oct = (int) idiv (k, 7);
        int semis = sc[k - oct * 7] + 12 * oct - sc[idx];
        const int dOct = (int) idiv (deg, 7);
        const int dm = deg - dOct * 7;
        if (dm == 2 && (c.third == 3 || c.third == 4)) semis = 12 * dOct + c.third;
        if (dm == 4) semis = 12 * dOct + c.fifth;
        if (dm == 6 && c.seventh > 0) semis = 12 * dOct + c.seventh;
        int p = rootPitch + semis;
        if (! pcIn (c.mask, p))
        {
            if (snap || len >= 18) p = snapTo (p, c.mask);
            else if (len >= 12 && pcIn (c.mask, p - 1)) p -= 1;
        }
        return p;
    }

    //==========================================================================
    uint32_t layerSeed (int l) const
    {
        return mix (mix ((uint32_t) cfg.seed, (uint32_t) cfg.layer[l].seed * 131u),
                    (uint32_t) (cfg.layer[l].role * 7 + cfg.style * 1009 + 13));
    }

    // 0 = question, 1 = reponse, 2 = deuxieme question (phrases de 8 mesures), 3 = fin de phrase
    static int kindOf (int b, int V)
    {
        if (V <= 1) return 0;
        if (b % 2 == 0) return (V >= 8 && b % 8 == 4) ? 2 : 0;
        return (b % 4 == 3 || b == V - 1) ? 3 : 1;
    }

    int addScaled (Phrase& p, int vtick, int len, int deg, int vel, int flags) const
    {
        return p.add (vtick * num / den, std::max (2, len * num / den), deg, vel, flags);
    }

    void emitBar (Phrase& p, int vbar, const BarNotes& b, const StyleDef& st, Rng r, bool recordOcc)
    {
        auto keep = [&r] (float a) { return r.chance (std::min (1.0f, a * 2.0f)); };
        auto extra = [&r] (float a, float k) { return a > 0.5f && r.chance ((a - 0.5f) * 2.0f * k); };
        int lastIdx = -1, lastTickV = 0;
        for (int i = 0; i < b.n; ++i)
        {
            const int vt = vbar * kTPB + b.tick[i];
            if (recordOcc && vbar < kMaxVBars) leadOcc[vbar][b.tick[i]] = true;
            const char sym = b.sym[i];
            const bool strong = b.acc[i];
            int flags = strong ? fChord : 0;
            int L = b.len[i] > 6 ? std::max (3, int ((float) b.len[i] * st.gate)) : std::max (2, int ((float) b.len[i] * st.gate + 0.5f));
            // les notes resonnent, sauf vers une note voisine (deux notes conjointes qui se chevauchent, ca frotte)
            const bool nextIsStep = i + 1 < b.n && std::abs (b.deg[i + 1] - b.deg[i]) == 1;
            if (st.ring) L = nextIsStep ? b.len[i] : std::min (48, b.len[i] * 2);
            int vel = strong ? r.range (98, 112) : r.range (78, 94);
            const bool stepwise = i > 0 && std::abs (b.deg[i] - b.deg[i - 1]) == 1 && b.tick[i] - b.tick[i - 1] <= 8;

            if ((sym == 'g' && keep (cfg.artGhost)) || (sym == 'x' && ! strong && b.len[i] <= 6 && extra (cfg.artGhost, 0.3f)))
            {
                flags |= fGhost; vel = r.range (38, 52); L = 2;
            }
            else
            {
                if ((sym == 'd' || sym == 'D') ? keep (cfg.artDouble) : (strong && extra (cfg.artDouble, 0.4f)))
                {
                    flags |= fDouble;
                    if (sym == 'D' || (sym != 'd' && r.chance (0.3f))) flags |= fSixth;
                }
                if ((sym == 's' && keep (cfg.artSlide)) || (strong && b.len[i] >= 12 && extra (cfg.artSlide, 0.35f)))
                    flags |= fSlide;
                else if (i > 0 && ! (flags & fDouble)
                         && ((sym == 'h' && keep (cfg.artHammer)) || (sym == 'x' && stepwise && extra (cfg.artHammer, 0.5f))))
                {
                    // hammer-on (montant) ou pull-off (descendant) : note liee, attaque plus douce
                    flags |= fHammer; vel = int ((float) vel * 0.7f);
                    if (lastIdx >= 0) p.ev[(size_t) lastIdx].len = (int16_t) (((vt - lastTickV) * num / den) + 2);
                }
            }
            const int idx = addScaled (p, vt, L, b.deg[i], vel, flags);
            if (idx >= 0) { lastIdx = idx; lastTickV = vt; }
        }
    }

    static void shiftBar (BarNotes& b, int s) { for (int i = 0; i < b.n; ++i) b.deg[i] = (int8_t) (b.deg[i] + s); }

    // Choisit et assemble les phrases ecrites : question d'un lick, reponse d'un lick voisin, decalage eventuel.
    void pickLicks (Rng& r, const StyleDef& st, float dens, BarNotes m[4]) const
    {
        const int ci = std::clamp (int (dens * 5.999f), 0, 5);
        const int a = std::clamp (ci + r.range (-1, 1), 0, 5);
        const int b = r.chance (0.5f) ? a : std::clamp (ci + r.range (-2, 2), 0, 5);
        static const int pentShift[6] = { 0, 0, 0, 1, -1, 2 };
        static const int diatShift[6] = { 0, 0, 0, 2, -2, 0 };
        const int pickS = r.range (0, 5);
        const int s = st.chordRel ? 0 : (st.pent ? pentShift : diatShift)[pickS];
        const Lick& la = st.licks[a]; const Lick& lb = st.licks[b]; const Lick& lc = st.licks[(a + 3) % 6];
        parseLick (la.r1, la.d1, m[0]);
        parseLick (lb.r2, lb.d2, m[1]);
        parseLick (lc.r1, lc.d1, m[2]);
        parseLick (lb.r3, lb.d3, m[3]);
        for (int k = 0; k < 4; ++k) { shiftBar (m[k], s); quantize (m[k], grid, r, dens); }
    }

    void buildLead (int l, bool recordOcc)
    {
        const StyleDef& st = kStyles[cfg.style];
        const float dens = std::clamp (cfg.layer[l].density, 0.0f, 1.0f);
        const uint32_t seed = layerSeed (l);
        Rng r (seed);
        BarNotes m[4];
        pickLicks (r, st, dens, m);
        for (int b = 0; b < vBars; ++b)
        {
            const int kd = kindOf (b, vBars);
            emitBar (phrase[l], b, m[kd], st, Rng (mix (seed, (uint32_t) kd * 77u + 5u)), recordOcc);
        }
    }

    // Reponse : une autre phrase du meme style, jouee seulement dans les trous de la melodie principale.
    void buildAnswer (int l)
    {
        const StyleDef& st = kStyles[cfg.style];
        const float dens = std::clamp (cfg.layer[l].density, 0.0f, 1.0f);
        const uint32_t seed = layerSeed (l);
        Rng r (seed);
        BarNotes m[4];
        pickLicks (r, st, dens, m);
        const int g = grid > 0 ? grid : 6;
        const int limit = 60 - int (dens * 24.0f);
        const int want = 2 + int (dens * 3.5f);
        for (int b = 0; b < vBars; ++b)
        {
            const int kd = kindOf (b, vBars);
            // la reponse commente les fins de phrase ; a forte densite elle repond aussi aux questions
            if ((kd == 0 || kd == 2) && dens < 0.6f) continue;
            const BarNotes& src = m[kd == 0 ? 1 : kd == 1 ? 0 : kd];
            if (src.n == 0) continue;
            const bool* occ = leadOcc[std::min (b, kMaxVBars - 1)];
            // emplacements vraiment libres : aucune note de la melodie a moins d'une double-croche
            int slots[32]; int ns = 0;
            for (int pass = 0; pass < 2 && ns < 2; ++pass)
            {
                ns = 0;
                for (int t = 0; t < kTPB && ns < 32; t += g)
                {
                    if (pass == 0 && t < limit) continue;
                    bool busy = false;
                    for (int k = std::max (0, t - 5); k <= std::min (kTPB - 1, t + 5); ++k) busy = busy || occ[k];
                    if (! busy) slots[ns++] = t;
                }
            }
            const int n = std::min (std::min (ns, want), src.n);
            BarNotes f;
            for (int j = 0; j < n; ++j)
            {
                const int t = slots[ns - n + j];                // les derniers trous : la reponse amene la mesure suivante
                const int si = src.n - n + j;                   // et reprend la fin de la phrase ecrite
                int room = kTPB - t;
                for (int k = t + 1; k < kTPB; ++k) if (occ[k]) { room = k - t; break; }
                if (j + 1 < n) room = std::min (room, slots[ns - n + j + 1] - t);
                f.push (t, std::max (3, std::min (12, room)), j == n - 1, 'x', src.deg[si]);
            }
            emitBar (phrase[l], b, f, st, Rng (mix (seed, (uint32_t) kd * 91u + 3u)), false);
        }
    }

    void markHarmony (int l)
    {
        Phrase& p = phrase[l];
        for (int i = 0; i < p.count; ++i)
        {
            Ev& e = p.ev[(size_t) i];
            e.flags = (uint8_t) ((e.flags & ~(fDouble | fSixth | fSlide)) | fHarm);
            e.vel = (uint8_t) std::max (1, int ((float) e.vel * 0.8f));
        }
    }

    void buildArp (int l, bool bass)
    {
        const StyleDef& st = kStyles[cfg.style];
        const float dens = std::clamp (cfg.layer[l].density, 0.0f, 1.0f);
        const uint32_t seed = layerSeed (l);
        const int nPat = bass ? 2 : 3;
        const char* const* pats = bass ? st.bass : st.arp;
        const int ia = std::clamp (int (dens * (float) nPat), 0, nPat - 1);
        const int ib = (ia + 1) % nPat;
        BarNotes m[2];
        Rng r (seed);
        parseRhythm (pats[ia], m[0]); quantize (m[0], grid, r, dens);
        parseRhythm (pats[ib], m[1]); quantize (m[1], grid, r, dens);
        Rng rv (mix (seed, 17u));
        int vels[2][BarNotes::cap];
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < m[k].n; ++i)
                vels[k][i] = m[k].sym[i] == '0' ? rv.range (92, 104) : rv.range (68, 86);
        for (int b = 0; b < vBars; ++b)
        {
            const int k = (vBars > 1 && b == vBars - 1) ? 1 : 0;
            for (int i = 0; i < m[k].n; ++i)
            {
                const int d = (m[k].sym[i] >= '0' && m[k].sym[i] <= '9') ? m[k].sym[i] - '0' : 0;
                const int L = bass ? std::max (4, int ((float) m[k].len[i] * 0.9f)) : std::clamp (m[k].len[i] * 3, 18, 48);
                addScaled (phrase[l], b * kTPB + m[k].tick[i], L, d, vels[k][i], fArp);
            }
        }
    }

    //==========================================================================
    void fireTick (long long k, int sampleAbs, double spt)
    {
        const int total = std::clamp (cfg.bars, 1, kMaxBars) * kTPB;
        const int pt = imod (k, total);
        const long long bar = idiv (k, kTPB);
        Chord c = chordAtBar (bar);
        if (! c.valid) return;
        const Key key = currentKey();
        c = resolveChord (c, key);
        const double swingS = (imod (k, 12) == 6) ? cfg.swing * 2.0 * spt : 0.0;

        for (int l = 0; l < kLayers; ++l)
        {
            if (! cfg.layer[l].on) continue;
            const int idx = phrase[l].at[(size_t) pt];
            if (idx < 0) continue;
            const Ev& e = phrase[l].ev[(size_t) idx];
            const Realized r = realize (e, c, key, l);
            const uint32_t h = mix ((uint32_t) k * 31u + (uint32_t) l, (uint32_t) cfg.seed * 7919u);
            const double hum = cfg.human * (double (h & 0xFF) / 255.0) * 0.012 * sr;
            const int velJ = int (cfg.human * float (int ((h >> 8) & 0x1F) - 16) * 0.6f);
            int vel = std::clamp (int ((float) e.vel * cfg.layer[l].vel) + velJ, 1, 127);
            int due = sampleAbs + int (swingS + hum);
            int len = std::max (int (e.len * spt), int (0.02 * sr));
            // une note ne deborde jamais sur l'accord suivant : c'est ce qui faisait frotter les couches
            if (useProg && ! chordAtBar (bar + 1).same (chordAtBar (bar)))
            {
                const int room = int (double (kTPB - imod (k, kTPB)) * spt) - int (swingS + hum) - int (0.004 * sr);
                len = std::max (int (0.02 * sr), std::min (len, room));
            }
            if (r.grace >= 0)
            {
                const int g = std::max (int (1.5 * spt), int (0.012 * sr));
                note (l, r.grace, std::max (1, int ((float) vel * 0.75f)), due, g + 8);
                due += g; len = std::max (len - g, int (0.02 * sr)); vel = std::max (1, int ((float) vel * 0.9f));
            }
            note (l, r.pitch[0], vel, due, len);
            if (r.n > 1) note (l, r.pitch[1], std::max (1, int ((float) vel * 0.9f)), due + int (0.006 * sr), len);
        }
    }

    void push (int due, int ch, int pitch, int vel, bool isOff)
    {
        for (auto& p : pend)
            if (! p.used) { p = { due, (uint8_t) ch, (uint8_t) pitch, (uint8_t) vel, isOff, true }; return; }
    }

    void note (int ch, int pitch, int vel, int due, int len)
    {
        pitch = std::clamp (pitch, 0, 127);
        for (auto& p : pend)
            if (p.used && p.isOff && p.ch == ch && p.pitch == pitch && p.due > due) p.due = due;
        push (due, ch, pitch, vel, false);
        push (due + std::max (1, len), ch, pitch, 0, true);
    }

    void emit (int sample, uint8_t status, uint8_t d1, uint8_t d2)
    {
        if (outCount < (int) out.size()) out[(size_t) outCount++] = { std::max (0, sample), status, d1, d2 };
    }

    void flush (int limit)
    {
        for (int pass = 0; pass < 2; ++pass)
            for (auto& p : pend)
            {
                if (! p.used || p.due >= limit || p.isOff != (pass == 0)) continue;
                if (p.isOff)
                {
                    if (sounding[p.ch][p.pitch] > 0)
                    {
                        emit (std::min (p.due, limit - 1), uint8_t (0x80 | p.ch), p.pitch, 0);
                        sounding[p.ch][p.pitch] = 0;
                    }
                }
                else
                {
                    if (sounding[p.ch][p.pitch] > 0) emit (std::min (p.due, limit - 1), uint8_t (0x80 | p.ch), p.pitch, 0);
                    emit (std::min (p.due, limit - 1), uint8_t (0x90 | p.ch), p.pitch, p.vel);
                    sounding[p.ch][p.pitch] = 1;
                }
                p.used = false;
            }
    }
};

} // namespace gk
