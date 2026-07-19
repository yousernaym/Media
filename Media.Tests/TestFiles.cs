using System;
using System.IO;

namespace VisualMusic.MediaTests
{
    static class TestFiles
    {
        public static string FindRoot()
        {
            for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir != null; dir = dir.Parent)
            {
                string candidate = Path.Combine(dir.FullName, "test-files");
                if (Directory.Exists(candidate) && File.Exists(Path.Combine(candidate, "silence.wav")))
                    return candidate;
            }
            throw new DirectoryNotFoundException("Could not locate test-files/silence.wav.");
        }

        public static string PathTo(string relative) => Path.Combine(FindRoot(), relative);
    }
}
