# FluidScope – Quick start

**0.4.5 · verstaerker · macOS Apple Silicon**

1. Insert the VST3 on an audio track, or launch the standalone app and choose your input device. Desktop playback requires a suitable loopback input.
2. Start in **Stereo** with Auto / Left / Rising. Set Window to the time span you want to see. Adjust trigger level, hysteresis and holdoff for a repeating waveform. Gain is display-only.
3. **Freeze** holds the picture. **Single** waits for a triggered capture, then holds it. Persist controls afterglow.
4. In **XY / Lissajous**, X is left. Choose right, delayed left or an internal sine for Y. A quarter-period delay is `250 / frequency_Hz` milliseconds; 100 Hz needs 2.5 ms.
5. **Internal sine** is visual-only. Follow input tracks the left/X peak envelope with immediate attack and a 120-ms release time constant. OSC Ratio 1.00 matches that envelope. Y Delay shifts both carrier and envelope.
6. Enable **3D / Time** for a time axis; 3D Time spans 0.02–2 seconds. In 2D, use Window / ms instead.
7. For a readable FFT, start with Log frequency, Music +3 dB/oct, 1/24 octave smoothing and 180-ms 2D Release. Flat / None / FFT Gain 0 dB gives a less altered amplitude view. Weighting and gain change the display only.
8. **3D / History** shows 2–12 seconds of spectra. Its separate 3D Decay controls added visual release; 0 adds no tail. Rear Fade 25% fades the oldest quarter of the history or XY trajectory.
9. Drag a 3D view to rotate; use the wheel/trackpad for 50–300% zoom. Double-click resets its camera and zoom. In Quad, double-click expands a panel instead. Shift-drag the 3D spectrum's front axis to select a band.
10. **Full screen / F11** opens the presentation. F11, Return or the button returns to controls; Escape also disables band audition.

## Auditioning a band

Select a frequency range, then explicitly enable **Listen band**. This applies steep filters to the audio; selection alone does not. Turn it off, use Clear band or press Escape to return to the original signal after a short transition. The FFT continues to show the original input.

This isolates frequency ranges, not individual instruments. The enabled listen state is saved in the project and visibly marked LISTENING.

## Appearance and performance

Choose a palette or your own colours, line width and glow. Settings persist with the project. Long time spans, dense XY figures, high sample rates, large windows and zoom can increase load. Quad renders several views together. FFT windowing and smoothing also introduce response time.
