# MP4 audio sync notes

## Background

CM cut output may have audio sync drift after creating `filename.mp4-new.mp4`.
The current cut flow can split and concatenate MP4 chunks with MP4Box, then
extract audio, normalize it with sox, re-encode AAC, and add the new AAC track
back to the video-only MP4.

This means the output video and audio are produced through different paths. AAC
encoder delay, AAC decoder priming, MP4 edit lists, and track start timestamps
can affect the final A/V offset.

## Investigation Result

The sample files in this repository were not a strong reproduction of the
reported 0.5 second drift, but they showed an important metadata difference:

- Source `GR30_20140803_0030.mp4`
  - video `start_time`: `0.000000`
  - audio `start_time`: `0.050000`
  - source A/V offset: `+0.050` seconds
- Existing output `GR30_20140803_0030.mp4-new.mp4`
  - video `start_time`: `0.000000`
  - audio `start_time`: `0.000000`
  - output A/V offset before correction: `0.000` seconds

MP4Box was checked locally and `-delay <trackID>=<milliseconds>` sets the
track delay as an absolute value, not as an additive adjustment.

## Implemented Metadata-Based Correction

`cmcheckwave -S filename.mp4` was added.

It compares stream metadata for:

- original file: `filename.mp4`
- output file: `filename.mp4-new.mp4`

It calculates:

```text
original_offset = original_audio_start - original_video_start
output_offset   = output_audio_start - output_video_start
target_audio_start = output_video_start + original_offset
target_delay_ms = round(target_audio_start * 1000)
```

If `target_delay_ms` differs from the current output audio start, it runs:

```sh
MP4Box -quiet -noprog -delay <audioTrackID>=<target_delay_ms> filename.mp4-new.mp4
```

The existing cut command generation now appends this correction whenever
`filename.mp4-new.mp4` is created. This applies both to the normal AAC
re-encode path and the `-a` no-audio-reencode path.

## Limitations

The metadata-based method only corrects differences visible through stream
`start_time`. It will not detect all real lip-sync problems.

Negative delay was later verified with the `GR32_20260522_2300.mp4` sample:
`MP4Box -delay 2=-533` produced the same audio packet timing as the provided
`GR32_20260522_2300.mp4-new-fixed.mp4` reference.

Known weak cases:

- AAC encoder delay or decoder priming is audible but not represented in
  `start_time`.
- Players interpret edit lists or AAC priming metadata differently.
- The true drift appears only after several concat boundaries.
- The source file already has incorrect metadata but correct audible sync.
- The output waveform is shifted while stream start metadata still matches.

If reported drift remains around 0.5 seconds while metadata offsets match, the
next step should be waveform comparison.

## Waveform Comparison Option

A more robust correction can compare decoded PCM waveforms.

Suggested approach:

1. Build the same keep ranges used for the cut output.
2. Decode the source MP4 audio for those keep ranges into a reference PCM/WAV.
   This should match the actual post-cut timeline, not the full uncut source.
3. Decode `filename.mp4-new.mp4` audio into another PCM/WAV.
4. Compare the beginning and several interior windows using cross-correlation.
5. Estimate the delay in milliseconds.
6. Apply the correction with MP4Box `-delay` when the detected offset is stable.

Recommended safeguards:

- Use mono PCM for comparison to reduce noise.
- Compare multiple windows, for example near the beginning, middle, and after a
  concat boundary.
- Reject correction if windows disagree by more than a small threshold.
- Log both metadata offset and waveform offset.
- Keep a maximum automatic correction threshold, for example around 1000 ms, so
  a bad correlation does not damage the file.

Possible tools:

- `ffmpeg` to decode audio into signed 16-bit PCM WAV.
- A small C helper or script to compute cross-correlation over bounded windows.
- `ffmpeg` filters such as `axcorrelate` may help, but a purpose-built helper
  may be easier to make deterministic and parseable.

## Future Implementation Notes

The waveform path should probably be a separate option first, for example:

```sh
cmcheckwave -W filename.mp4
```

Then, after enough test files confirm reliability, `-S` could optionally run
metadata correction first and fall back to waveform correction when metadata
does not explain the observed drift.

Negative corrections are handled with MP4Box negative delay. A waveform
implementation should still keep explicit limits so a bad correlation does not
apply a large unintended shift.
