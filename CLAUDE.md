# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Role

`Media` is the native C++ media layer for **Visual Music**. It is a git submodule (separate repo:
`yousernaym/Media`) that builds `media.dll`, which the app calls via P/Invoke. It provides:

- **Video export** via FFmpeg (H.264/x264; supports spherical/360 and stereo output).
- **Audio playback** via Windows Media Foundation.

FFmpeg is supplied through vcpkg (`ffmpeg[x264]:x64-windows`) — see the repo-root [README.md](../../README.md).

## Build & output

- Project: [Media/Media.vcxproj](Media/Media.vcxproj) (`DynamicLibrary`). Standalone solution: [Media.sln](Media.sln).
- Built as part of the repo-root `VisualMusic.sln`. `OutDir` is `$(SolutionDir)$(Platform)\$(Configuration)\`,
  so when built from the root solution `media.dll` lands in the repo-root `x64\<Config>\`, and VisualMusic's
  post-build copies it into the app output.

## Native interface (P/Invoke surface)

The exact exported C functions the app binds to are declared in
[../../VisualMusic/Media.cs](../../VisualMusic/Media.cs) — keep that file and the C++ exports in sync
(`cdecl`, `media.dll`). Grouped:

- **General:** `initMF`, `closeMF`, `openAudioFile`, `getAudioFilePath`.
- **Encoding:** `beginVideoEnc(outputFile, audioFile, VideoFormat, audioOffsetSeconds, spherical, sphericalStereo, videoCodec, crf)`,
  `writeFrame(uint[] frameBuffer)`, `endVideoEnc`.
- **Playback:** `startPlayback` / `startPlaybackAtTime`, `stopPlayback`, `pausePlayback`, `playbackIsRunning`,
  `getPlaybackPos`, `getAudioLength`, `closeAudioFile`.

`VideoFormat` (width, height, fps) is marshaled `Sequential, Pack = 8`; match the C++ struct layout exactly.

See [../../CLAUDE.md](../../CLAUDE.md) for the repo-wide picture and
[../../VisualMusic/CLAUDE.md](../../VisualMusic/CLAUDE.md) for the export/playback flow.
