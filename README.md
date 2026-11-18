# Swell Guimik

Générateur de mélodies de guitare par Swell Audio. Tu donnes ta progression d'accords, tu choisis un style, et le plugin joue des mélos et des gimmicks de guitare qui collent aux accords, avec des couches qu'on empile.

Formats : VST3 et application standalone (Windows). Le plugin sort du MIDI (un canal par couche) et embarque un son de guitare électrique clean pour écouter sans routage.

## Fonctions

- **Styles** : Afrobeats, Rumba / soukous, Rap mélodique, Trap guitare, Mélancolique. Chaque style a ses phrases écrites à la main, avec hammer-ons, pull-offs, slides et doubles-cordes.
- **Accords** : joués en MIDI (clip ou clavier) ou écrits (`Am F C G` ou `Lam Fa Do Sol`).
- **Tonalité** : détectée automatiquement à partir des accords, ou forcée. Neuf gammes.
- **Division et vitesse** : 1/4, 1/8, 1/16, triolets ; moitié, normale, double.
- **4 couches** : mélodie principale, réponse, tierces / sixtes, arpège picking, basses.
- **Jeu de guitare** : dosage des hammer-ons / pull-offs, slides, doubles-cordes et notes étouffées.

## Compiler

Prérequis : CMake 3.22 ou plus et Visual Studio 2022 (ou Build Tools) avec le module C++. JUCE 8 est téléchargé automatiquement à la première configuration.

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release --target SwellGuimik_VST3 SwellGuimik_Standalone
```

Résultats dans `build/SwellGuimik_artefacts/Release/` (`VST3/Swell Guimik.vst3` et `Standalone/Swell Guimik.exe`). Pour l'utiliser dans un DAW, copie le dossier `.vst3` dans `C:\Program Files\Common Files\VST3`.

## Tester le moteur

Le moteur de génération (`Source/Engine.h`) est du C++ pur, sans JUCE. Le script suivant le compile seul, régénère les démos MIDI du dossier `demos/` et lance un audit des fausses notes sur 1 000 générations par style :

```bash
Tests\build_test.bat
```

## Organisation

| Dossier | Contenu |
|---|---|
| `Source/Engine.h` | Moteur : lecture des accords, tonalité, phrases par style, couches |
| `Source/GuitarSampler.*` | Son de guitare intégré |
| `Source/PluginProcessor.*` | Plugin JUCE : MIDI, synchro au tempo de l'hôte, paramètres |
| `Source/PluginEditor.*` | Interface |
| `Tests/` | Test hors-ligne et audit du moteur |
| `demos/` | Un fichier MIDI de démonstration par style |
| `Assets/` | Logo, police et samples |

## Crédits et licences des ressources

- Samples de guitare : [FreePats, FSBS Electric Guitar Clean](https://freepats.zenvoid.org/ElectricGuitar/clean-electric-guitar.html), licence CC0 (voir `Assets/guitar/LICENSE.txt`).
- Police Bebas Neue : SIL Open Font License.
- Framework audio : [JUCE](https://juce.com), soumis à sa propre licence.
