using System;
using System.IO;
using System.Runtime.InteropServices;
using VisualMusic;
using Xunit;
using VmMedia = VisualMusic.Media;

namespace VisualMusic.MediaTests
{
    [Collection("MediaSequential")]
    public class MediaIntegrationTests
    {
        static string FindMediaDll()
        {
            string beside = Path.Combine(AppContext.BaseDirectory, "media.dll");
            if (File.Exists(beside)) return beside;
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir != null; dir = dir.Parent)
            {
                foreach (var path in new[]
                {
                    Path.Combine(dir.FullName, "x64", "Debug", "media.dll"),
                    Path.Combine(dir.FullName, "x64", "Release", "media.dll"),
                })
                {
                    if (File.Exists(path)) return path;
                }
            }
            return null;
        }

        static void EnsureNativeLoaded()
        {
            string dll = FindMediaDll();
            if (dll == null)
                throw new FileNotFoundException("media.dll not found. Build Media (x64) before Integration tests.");
            string dir = Path.GetDirectoryName(dll);
            if (!string.Equals(dir, AppContext.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase))
            {
                foreach (var f in Directory.GetFiles(dir, "*.dll"))
                {
                    string dest = Path.Combine(AppContext.BaseDirectory, Path.GetFileName(f));
                    if (!File.Exists(dest))
                        File.Copy(f, dest, overwrite: false);
                }
            }
            NativeLibrary.Load(Path.Combine(AppContext.BaseDirectory, "media.dll"));
        }

        [Fact]
        [Trait("Category", "Integration")]
        public void Playback_lifecycle()
        {
            EnsureNativeLoaded();
            string wav = TestFiles.PathTo("silence.wav");
            Assert.True(VmMedia.InitMF());
            try
            {
                Assert.True(VmMedia.OpenAudioFile(wav));
                Assert.True(VmMedia.GetAudioLength() > 0);
                Assert.True(VmMedia.StartPlayback());
                VmMedia.PausePlayback();
                VmMedia.StopPlayback();
                Assert.True(VmMedia.CloseAudioFile());
            }
            finally
            {
                VmMedia.CloseMF();
            }
        }

        [Fact]
        [Trait("Category", "Integration")]
        public void Encode_smoke_writes_mkv()
        {
            EnsureNativeLoaded();
            string outPath = Path.Combine(Path.GetTempPath(), "vm_enc_" + Guid.NewGuid().ToString("N") + ".mkv");
            Assert.True(VmMedia.InitMF());
            try
            {
                var fmt = new VideoFormat(64, 64, 10f);
                Assert.True(VmMedia.BeginVideoEnc(
                    outPath,
                    audioFile: null,
                    fmt,
                    audioOffsetSeconds: 0,
                    spherical: false,
                    sphericalStereo: false,
                    AVCodecID.AV_CODEC_ID_H264,
                    crf: "28"));

                uint[] frame = new uint[64 * 64];
                for (int i = 0; i < frame.Length; i++)
                    frame[i] = 0xFF0080FF; // BGRA-ish solid
                for (int f = 0; f < 5; f++)
                    Assert.True(VmMedia.WriteFrame(frame));
                VmMedia.EndVideoEnc();

                Assert.True(File.Exists(outPath));
                Assert.True(new FileInfo(outPath).Length > 0);
            }
            finally
            {
                VmMedia.CloseMF();
                try { File.Delete(outPath); } catch { }
            }
        }
    }

    [CollectionDefinition("MediaSequential", DisableParallelization = true)]
    public class MediaSequentialCollection { }
}
