# FluidScope

<img src="assets/FluidScope.png" alt="FluidScope app icon" width="128">

### Mein Traum-Oszilloskop. Wellenformen in Bewegung.

Ein Audio-Oszilloskop von **verstaerker**, entwickelt für eine flüssige Darstellung, stabiles Triggering und einen gut lesbaren Blick in den Klang. Wellenform, Lissajous und Spektralanalyse treffen auf räumliche Zeitverläufe, eigene Farben und eine große Vollbildansicht.

**macOS Apple Silicon + Intel · VST3 + eigenständige App · Version 0.4.9 · GPLv3 oder später**

[English](docs/README.en.md) · [Kurzanleitung](docs/KURZANLEITUNG.md) · [GUI-Galerie](docs/GALLERY.md) · [Downloads](https://github.com/verstaerkermusic/FluidScope/releases)

![FluidScope: Wellenform, Lissajous, FFT und 3D-Verlauf in der Quad-Ansicht](docs/images/quad.png)

## Was mir wichtig war

Ich wollte ein Oszilloskop, bei dem man Veränderungen wirklich verfolgen kann: modulierte Wellenformen, Transienten, Phasenbeziehungen und die Bewegung eines Spektrums über die Zeit. Eine Darstellung, die flüssig läuft, gut aussieht und etwas über den Klang verrät.

Daraus ist FluidScope geworden.

## Fünf Ansichten, ein Signal

| Ansicht | Was du siehst |
| --- | --- |
| **Stereo** | Zwei Wellenformen mit stabilem Trigger, Hysterese, Holdoff und einstellbarem Nachleuchten. |
| **XY / Lissajous** | Linker Kanal gegen rechten Kanal, verzögerten linken Kanal oder internen Sinus. Optional mit Zeit als dritter Achse. |
| **FFT / Spectrum** | Logarithmische oder lineare Frequenzachse, wählbare Gewichtung, Glättung und Spitzenkurve. |
| **3D / History** | Frequenz und Pegel über 2–12 Sekunden. Freie Rundum-Perspektiven und Zoom machen zeitliche Zusammenhänge sichtbar. |
| **Quad** | Wellenform, XY, FFT und 3D-Verlauf gleichzeitig. |

## Kleine Regler, großer Unterschied

- **Triggering:** steigende/fallende Flanke, Auto/Normal/Free, Pre-Trigger, Freeze und Single. Überlappende Erfassung vermeidet Pausen durch lange Zeitfenster.
- **Lissajous gestalten:** Y-Delay bis 100 ms; interner Sinus mit Frequenz, Phase und Pegel. „Follow input“ lässt seine Amplitude der Hüllkurve des linken Eingangs folgen.
- **Lesbare FFT:** getrennte Regler für 2D Release und 3D Decay, Frequenzglättung und visuelle Gewichtung. FFT Gain verändert nur die Anzeige.
- **Bereiche vorhören:** Frequenzband auswählen und „Listen band“ einschalten. So lässt sich hören, welche Klanganteile dort liegen.
- **3D erkunden:** Ziehen dreht die Perspektive; Mausrad/Trackpad zoomt von 50 bis 300 %. Der hintere Teil des Verlaufs kann weich ausblenden.
- **Wireframe:** zeigt die 3D-FFT als Liniennetz statt als gefüllte Fläche, auch in Quad und Vollbild.
- **Clean Mode:** blendet Raster, Skalen, Achsen und Hinweistexte in allen Ansichten aus; die Regler bleiben bedienbar.
- **Eigener Look:** klassische Grün- und Amber-Töne, Schwarzweiß und moderne Paletten, freie Farben, Liniendicke und Glow.
- **Groß anzeigen:** Vollbild auf dem Bildschirm des Plugins, auch über schwebenden DAW-Fenstern.

![3D-Lissajous mit Zeit als dritter Achse und Envelope-Follower](docs/images/xy3d.png)

## Installation

1. Das macOS-Universal-Paket unter [Releases](https://github.com/verstaerkermusic/FluidScope/releases) herunterladen und entpacken.
2. `FluidScope.vst3` nach `~/Library/Audio/Plug-Ins/VST3/` kopieren. Den Ordner bei Bedarf erstellen.
3. Die DAW vollständig neu starten; falls nötig Plugins neu scannen. Hersteller: **verstaerker**.
4. Für den Betrieb ohne DAW `FluidScope.app` in einen Ordner deiner Wahl kopieren und starten.

Benötigt **macOS 12 oder neuer und einen Apple-Silicon- oder Intel-Mac**. Die VST3-Version benötigt einen passenden VST3-Host. AU und Windows-Builds sind nicht enthalten. Die 3D-Ansichten benötigen OpenGL 3.2 oder neuer. Intel wurde hier per Crossbuild und Tests unter Rosetta geprüft; ein nativer Intel-Hardwaretest steht noch aus. Die Downloads sind ad-hoc signiert und nicht notarisiert; macOS kann eine zusätzliche Freigabe verlangen.

## Gut zu wissen

Ohne Band-Vorhören bleibt das Audio nach beendetem Umschalt-Fade unverändert. „Listen band“ schaltet dagegen hörbare Filter ein und wird mit dem Projekt gespeichert. Das Vorhören isoliert einen Frequenzbereich; Instrumente mit überlappenden Frequenzen lassen sich damit nicht vollständig trennen.

Die App analysiert die gewählten Geräte-Eingänge. Für Desktop-Audio wird ein geeigneter Loopback-Eingang benötigt; direkte macOS-Systemaudio-Aufnahme ist nicht eingebaut.

FluidScope ist auf flüssige Updates ausgelegt. Die erreichbare Bildrate und Last hängen unter anderem von Abtastrate, Zeitfenster, Fenstergröße, Zoom, Signal und DAW ab. Sehr dichte Lissajous-Figuren und lange 3D-Verläufe bleiben aufwendiger. FFT-Fenster und Glättung bringen eine Reaktionszeit mit sich.

## Quellcode und Lizenz

FluidScope wird unter **GNU GPLv3 oder später** veröffentlicht. Der vollständige Quellcode und das passende JUCE-7.0.10-Snapshot liegen im Repository und im Quellcode-Download des Releases. [Bauanleitung](BUILDING.md) · [Lizenz](LICENSE) · [Hinweise und Herkunft](NOTICE.md).

Die ursprünglichen Oscilloscope- und XYScope-Ideen von [FigBug/slPlugins](https://github.com/FigBug/slPlugins) waren der Ausgangspunkt. Danke an Roland Rabien und die JUCE-Entwickler. Die ursprünglichen BSD-Hinweise stehen in [UPSTREAM-LICENSE.md](UPSTREAM-LICENSE.md).

## Rückmeldungen

Bei Problemen bitte macOS-Version, Mac-Modell, DAW, Abtastrate, Ansicht und die betreffenden Reglerwerte angeben. Bei Darstellungsproblemen sind ein Screenshot und die FPS-Anzeige hilfreich. [Issue erstellen](https://github.com/verstaerkermusic/FluidScope/issues).
