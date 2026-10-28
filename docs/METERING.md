# Input meters

Enable **Meters** to show the sidebar in any display mode or fullscreen. Its visibility is independent of **Clean** and is saved with the project.

The meters analyse the original plugin input, before band audition. Waveform gain, FFT gain, weighting, the internal visual oscillator and its envelope follower do not affect these readings. Mono input is counted once for loudness; stereo channels contribute their separate powers, including when they are out of phase.

## Readings

- **Peak / dBFS:** maximum absolute sample value in the most recent 5-ms analysis interval, separately for L/R. The bars follow display refresh with immediate attack, a brief 15-ms transient hold and a time-based 48-dB/s falling motion; the white marks retain each channel's highest sample peak until reset.
- **RMS / 400 ms:** unweighted RMS over the last 400 ms, displayed in dBFS. A full-scale sine reads approximately -3.01 dBFS RMS. The amber marks show RMS within the peak bars.
- **True Peak / dBTP:** a four-times-oversampled, 32-tap-per-phase FIR estimate of reconstructed peaks, including peaks between samples. **Max** retains the highest estimate until reset.
- **M:** K-weighted momentary loudness over 400 ms.
- **S:** K-weighted short-term loudness over 3 seconds, shown as the large LUFS reading.
- **I:** integrated loudness since reset, with overlapping 400-ms blocks, an absolute -70-LUFS gate and a relative gate 10 LU below the absolute-gated average.

Peak/RMS/True Peak values update every 5 ms. RMS uses a continuously sliding 400-ms window; the amber marker has a short visual interpolation. LUFS values continue to update every 100 ms with their original integration windows and gating.

M and I become available after 400 ms; S after 3 seconds. Unavailable readings are displayed as `-inf`. When the host stops supplying audio, live readings go idle; retained maxima and integrated loudness stay available. Freeze affects the visual capture, while meters continue measuring incoming audio.

## Warning, colours and reset

**Settings** adjusts the True Peak warning threshold, default **-1 dBTP**. If the retained maximum exceeds this threshold, the True Peak field stays red until **Reset**, unless the threshold is raised above that maximum. This is an indication; no limiter or gain change is applied.

Short-term LUFS gradually changes from green at **-14 LUFS**, through yellow, to deep red at **-8 LUFS**. Both endpoints are adjustable and saved. These colours are a personal loudness guide, not a delivery standard or a judgement about the music.

**Reset** clears peak holds, True Peak maximum and all loudness measurement windows. **Reset on playback start**, off by default, starts a fresh measurement on a stopped-to-playing transition reported by the host. A transport loop does not reset the measurement. Sample-rate or mono/stereo layout changes also start a fresh measurement. Measurement results themselves are session-local and are not restored from a saved project.

## Implementation and validation

Analysis runs on a dedicated worker. The audio callback only writes to a preallocated nonblocking queue; the plugin does not wait for the GUI or allocate meter memory in that callback. Integrated loudness uses bounded 0.01-LU histogram bins retaining exact power sums; quantisation is limited to the relative-gate selection. A queue overflow invalidates the current measurement and shows **Input gap / Reset meters**. Use Reset after the overload; do not treat a measurement with an input gap as a complete programme measurement. Very fast offline renders may exceed the real-time analysis queue.

The algorithm follows the mono/stereo signal model in [ITU-R BS.1770](https://www.itu.int/rec/R-REC-BS.1770) and the time windows in [EBU Tech 3341](https://tech.ebu.ch/publications/tech3341). Automated tests cover tone calibration at 44.1/48/96/192 kHz, mono/stereo, RMS, intersample peaks, gating, silence, reset and worker publication. Independent FFmpeg `ebur128` checks cover calibration and a gated level-transition signal. This is not a certified broadcast meter; True Peak is an oversampled estimate.
