# AGENTS.md

This file provides guidance for AI coding agents when working with code in this repository.

## Role

`Media` is the native C++ media layer for **Visual Music**. It is a git submodule (separate repo:
`yousernaym/Media`) that builds `media.dll`, which the app calls via P/Invoke. It provides:

- **Video export** via FFmpeg (H.264/x264; supports spherical/360 and stereo output).
- **Audio playback** via Windows Media Foundation.

FFmpeg is supplied through vcpkg in **manifest mode**: the dependency (`ffmpeg` with the `x264` feature) is
declared in [vcpkg.json](vcpkg.json) at this submodule's root, with versions pinned via `builtin-baseline`.
The x64 build auto-restores it into a local `vcpkg_installed/` (enabled by `VcpkgEnableManifest` in
[Media/Media.vcxproj](Media/Media.vcxproj)); `VcpkgAutoLink` then links the libs and applocal copies the DLLs
next to `media.dll`. See the repo-root [README.md](../../README.md).

## Build & output

- Project: [Media/Media.vcxproj](Media/Media.vcxproj) (`DynamicLibrary`). Standalone solution: [Media.sln](Media.sln).
- Built as part of the repo-root `VisualMusic.sln`. `OutDir` is `$(SolutionDir)$(Platform)\$(Configuration)\`,
  so when built from the root solution `media.dll` lands in the repo-root `x64\<Config>\`, and VisualMusic's
  post-build copies it into the app output.

## Updating FFmpeg

The FFmpeg version is whatever the `builtin-baseline` in [vcpkg.json](vcpkg.json) resolves to (no
`vcpkg install`/`vcpkg upgrade` in manifest mode). To move to a newer FFmpeg:

1. Update the vcpkg checkout to a newer snapshot and re-bootstrap (from the vcpkg root, e.g. `D:\dev\vcpkg`):
   `git fetch origin && git checkout <newer-release-tag-or-origin/master> && .\bootstrap-vcpkg.bat`.
2. Bump this manifest's baseline to the new snapshot: `vcpkg x-update-baseline --x-manifest-root=<this dir>`
   (or set `builtin-baseline` by hand to `git -C <vcpkg root> rev-parse HEAD`). MidMix's manifest is
   separate, so bumping here moves FFmpeg only.
3. Rebuild `VisualMusic.sln` — the new version installs automatically (compiles from source once, then cached).
4. **Test a real video export.** A newer FFmpeg can remove/deprecate APIs and break [Media/encoding.cpp](Media/encoding.cpp)
   (e.g. the 4.4 → 8.1 jump dropped `avcodec_encode_video2` and the `channels`/`channel_layout` fields).

To pin an exact version instead of the baseline default, add an `overrides` entry to [vcpkg.json](vcpkg.json),
e.g. `"overrides": [ { "name": "ffmpeg", "version": "8.1.1" } ]`; the version must exist at/after the baseline
(browse `<vcpkg root>\versions\f-\ffmpeg.json`).

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

See [../../AGENTS.md](../../AGENTS.md) for the repo-wide picture and
[../../VisualMusic/AGENTS.md](../../VisualMusic/AGENTS.md) for the export/playback flow.

## Testing

There is no C# test project in this repo. P/Invoke playback/encode smoke tests live in Visual Music’s
`VisualMusic.Tests` (`Category=Integration`) next to [../../VisualMusic/Media.cs](../../VisualMusic/Media.cs).
Fixture: [`test-files/silence.wav`](test-files/silence.wav). See [../../AGENTS.md](../../AGENTS.md).
