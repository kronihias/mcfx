/*
  ==============================================================================

   Locate the per-plugin scanner executable embedded in a plugin's bundle.

   Each hosting plugin embeds its own scanner binary at build time
   (see mcfx_anything/CMakeLists.txt or mcfx_graph/CMakeLists.txt).
   At runtime the plugin needs to find that binary regardless of its
   bundle layout (.vst3 / .au / .app / standalone executable).

   Pass the per-plugin executable basename (without ".exe" on Windows —
   the suffix is added internally for that platform).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

inline juce::File findScannerExecutable (const juce::String& exeBaseName)
{
    auto thisExe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

   #if JUCE_WINDOWS
    const juce::String scannerName = exeBaseName + ".exe";
   #else
    const juce::String scannerName = exeBaseName;
   #endif

   #if JUCE_MAC
    // macOS bundle: <bundle>/Contents/MacOS/<binary>
    // Scanner at:   <bundle>/Contents/Helpers/<scannerName>
    auto helpers = thisExe.getParentDirectory()     // MacOS/
                          .getParentDirectory()     // Contents/
                          .getChildFile ("Helpers")
                          .getChildFile (scannerName);

    if (helpers.existsAsFile())
        return helpers;
   #endif

    // Alongside the executable (Windows, Linux, dev builds)
    auto alongside = thisExe.getParentDirectory()
                            .getChildFile (scannerName);

    if (alongside.existsAsFile())
        return alongside;

   #if JUCE_WINDOWS
    // Windows VST3 bundle: plugin.vst3/Contents/x86_64-win/plugin.vst3
    // Try two levels up from the DLL, then into a Helpers dir
    auto winHelpers = thisExe.getParentDirectory()      // x86_64-win/
                             .getParentDirectory()      // Contents/
                             .getChildFile ("Helpers")
                             .getChildFile (scannerName);

    if (winHelpers.existsAsFile())
        return winHelpers;
   #endif

    return {};
}
