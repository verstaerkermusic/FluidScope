# FluidScope – Kurzanleitung

**Version 0.4.11 · verstaerker · macOS Apple Silicon + Intel**

## 1. Loslegen

In der DAW FluidScope als Audio-Effekt auf die gewünschte Spur setzen und Audio abspielen. Für Stereo- und Phasenbilder beide Kanäle zuführen. In der eigenständigen App die Audio-Einstellungen öffnen und das passende Eingangsgerät wählen. Für Systemaudio einen Loopback-Eingang verwenden.

Die Grunddarstellung verändert das Audio nicht. Nur **Listen band** schaltet die Frequenzfilter zum Vorhören ein.

## 2. Wellenform stabilisieren

**Oscilloscope** wählen. Mit **Window / ms** den sichtbaren Zeitraum einstellen: wenige Millisekunden für einzelne Schwingungen, mehr Zeit für Hüllkurven und Transienten. **Gain** vergrößert ausschließlich die Wellenform-/XY-Anzeige.

Zum Start **Auto**, **Left** und **Rising** wählen. Den Trigger Level so setzen, dass das Signal die gewählte Flanke regelmäßig überschreitet. **Hysteresis** hilft gegen kleine Störbewegungen am Triggerpunkt; **Holdoff** kann Mehrfachtrigger in komplexen Perioden reduzieren. **Pre-trigger** zeigt einen Anteil vor dem Triggerpunkt.

- **Auto:** zeigt auch ohne passenden Trigger weiter ein Bild.
- **Normal:** wartet auf einen passenden Trigger.
- **Free:** freie Erfassung ohne Flankentrigger.
- **Freeze:** Bild anhalten; mit Unfreeze fortsetzen.
- **Single:** eine passende Erfassung abwarten und anhalten.
- **Persist / ms:** regelt das Nachleuchten.

Ein stabiles Bild hängt auch vom Signal ab: Bei Rauschen oder stark wechselnden Perioden gibt es nicht immer einen wiederkehrenden Triggerpunkt.

## 3. Lissajous: Phase und Bewegung

**XY / Lissajous** wählen. X ist immer der linke Eingang. **Y Source** wählt rechten Eingang, linken Eingang oder internen Sinus. **Y Delay** verschiebt nur die Darstellung des Y-Signals um bis zu 100 ms.

Für einen Kreis aus einem Monosinus **Left input** wählen und eine Viertelperiode verzögern:

`Delay in ms = 250 / Frequenz in Hz`

Bei 100 Hz sind das 2,5 ms. Ein Kreis entsteht bei passenden Pegeln und idealer Sinusform.

**Internal sine** bringt einen unabhängigen visuellen Sinus auf Y. **OSC / Hz**, **OSC Phase** und **OSC Level** bestimmen seine Frequenz, Phase und Amplitude. Der Sinus wird nicht als Audio ausgegeben.

**Follow input** koppelt seine Amplitude an die Hüllkurve des linken/X-Eingangs. Der Anstieg reagiert sofort, der Abfall hat eine Zeitkonstante von 120 ms. **OSC Ratio = 1,00** entspricht dem erfassten Hüllkurvenpegel; kleinere Werte verkleinern Y. Y Delay verschiebt Sinus und Hüllkurve gemeinsam.

**3D / Time** ergänzt Zeit als dritte Achse. **3D Time / s** bestimmt den sichtbaren Verlauf von 0,02 bis 2 Sekunden. Im 2D-Modus ist **Window / ms** zuständig. Bei langen 3D-Verläufen kann eine dichte Figur mehr Grafikleistung benötigen.

## 4. FFT lesbar einstellen

**FFT / Spectrum** wählen. Als Einstieg: **Log frequency**, **Music +3 dB/oct**, **1/24 octave**, **2D Release 180 ms**.

- **Frequency scale:** logarithmisch für musikalische Abstände, linear für gleichmäßige Frequenzabstände.
- **Analysis input:** Stereo power, linker oder rechter Kanal. Stereo power verhindert, dass gegenphasige Kanäle sich in der Analyse auslöschen.
- **Frequency smooth:** fasst benachbarte Frequenzen zusammen. Mehr Glättung beruhigt die Kurve und verringert feine Details.
- **2D Release:** längerer Wert = ruhigerer, langsamerer Abfall.
- **Weighting / Custom tilt:** kippt die visuelle Gewichtung in dB pro Oktave, mit Bezug auf 1 kHz.
- **FFT Gain / dB:** hebt die gesamte FFT-Anzeige an oder senkt sie ab; Doppelklick setzt auf 0 dB.
- **Floor / dB:** untere Grenze der Pegelanzeige.

Mit der Maus über der 2D-Kurve werden Frequenz, dargestellter Pegel und nächster Ton angezeigt. Für eine möglichst unveränderte Pegelanzeige **Flat**, **None** und **FFT Gain 0 dB** wählen. Gewichtung und Gain sind rein visuell. Das FFT-Fenster umfasst je nach Abtastrate ungefähr 170–186 ms; Glättung bringt weitere Reaktionszeit mit.

## 5. Spektrum über die Zeit

**3D / History** zeigt Frequenz, Pegel und Zeit. **History / s** wählt 2–12 Sekunden.

**3D Decay / ms** regelt den zusätzlichen visuellen Nachklang unabhängig vom 2D Release. 40 ms ist ein kurzer Startwert; 0 fügt keinen weiteren Nachklang hinzu. Bereits erfasste Spektren behalten ihre Form beim Weiterwandern.

**Rear Fade / %** blendet den ältesten Teil weich aus. 25 entspricht dem letzten Viertel, 0 schaltet den Fade ab. Diese Einstellung gilt auch für 3D-Lissajous.

**Wireframe** neben Trace Colours schaltet die 3D-FFT zwischen gefüllter Fläche und Liniennetz um. Der Schalter erscheint bei 3D / History und Quad; in der Vollbild-Kopfzeile ist er ebenfalls verfügbar. Wireframe lässt sich mit Clean Mode kombinieren und wird mit dem Projekt gespeichert.

## 6. 3D bedienen und groß anzeigen

- **Ziehen:** Perspektive horizontal und vertikal rundum drehen.
- **Mausrad/Trackpad:** Zoom zwischen 50 und 300 %.
- **Doppelklick:** Perspektive und Zoom zurücksetzen. In Quad vergrößert ein Doppelklick das Feld; dort zum Zurücksetzen zunächst in die Einzelansicht wechseln.
- **Shift-Ziehen im 3D-Spektrum:** Frequenzband an der vorderen Achse auswählen.

**Quad** zeigt vier Perspektiven gleichzeitig. **Full screen / F11** öffnet die große Darstellung auf dem Bildschirm des Plugins. Oben lässt sich die Ansicht wechseln. **F11**, **Return** oder die Schaltfläche kehren zu den Reglern zurück. **Escape** kehrt zurück und beendet außerdem das Band-Vorhören.

## 7. Frequenzbereiche vorhören

In FFT oder Quad einen Bereich aufziehen oder **Low / High** einstellen. Dann **Listen band** ausdrücklich einschalten. Erst jetzt wird das Audio gefiltert. Die FFT zeigt weiterhin den ursprünglichen Eingang.

Ausschalten, **Clear band** oder **Escape** führt nach einem kurzen Übergang zum Original zurück. Der Filter dämpft außerhalb des Bereichs steil, ungefähr 48 dB/Oktave je Flanke. Kick, Snare und Synth können dieselben Frequenzbereiche nutzen; dies ist keine Instrumententrennung.

Der Listen-Zustand wird im DAW-Projekt gespeichert. Auf die sichtbare **LISTENING**-Anzeige achten.

## 8. Clean Mode

**Clean** neben Appearance blendet Raster, Skalen, Achsen, Trigger-Markierungen und Plot-Hinweise aus. Die Einstellung gilt gemeinsam für alle Ansichten, Quad und Vollbild. In der Vollbild-Kopfzeile lässt sie sich ebenfalls umschalten. Die Regler und der Rückweg zum Plugin bleiben erreichbar. Die Messdaten und das Audio bleiben unverändert.

## 9. Farben, Linien und Leistung

**Appearance** bietet Mint, Classic green, Amber CRT, Monochrome, Paper, Ice, Neon und Custom. **L/R Colour** öffnet die freie Farbauswahl. **Line / px** regelt die Dicke, **Glow / %** das Leuchten. Einstellungen werden mit dem Projekt gespeichert.

Bei hoher Last zunächst Zeitfenster, 3D-Verlauf, Zoom und Fenstergröße prüfen. Quad berechnet mehrere Anzeigen gleichzeitig. Kurze Zeitfenster sind meist leichter zu zeichnen als lange, dichte Figuren. Die FPS-Anzeige hilft beim Vergleichen; sie misst keine physische Monitor-Ausgabe.
