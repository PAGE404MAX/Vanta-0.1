# Vanta Beta 0.1 — Windows installer project

This project contains VST3 source and an Inno Setup installer script. **No compiled .exe or .vst3 is included:** the project and installer have not been built or tested in this environment. The synth is a basic prototype with JUCE's generic editor; the HTML UI is separate and not wired up. Preset compatibility and the additional UI controls do not yet function in the plugin.

## Build the installer with GitHub Actions
1. Create a GitHub repository and upload the *contents* of this folder, including the hidden `.github` folder.
2. Open the Actions tab, enable workflows if prompted, and run **Build Windows VST3 and Installer**.
3. When the run succeeds, download the `Vanta-Beta-0.1-Windows` artifact. It contains the setup wizard and plugin bundle.
4. Run `Vanta-Beta-0.1-Setup.exe` on Windows. It requests admin rights and installs to the system VST3 folder. Rescan plugins in FL Studio.

## Build locally on Windows
Install Visual Studio 2022 C++ tools, CMake, Git and Inno Setup 6. With internet access, run:
```
cmake -S . -B build -A x64
cmake --build build --config Release --target VantaBeta_VST3 --parallel
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
```
The setup executable will be in `installer-output` if the build succeeds. Review JUCE licensing before distribution. Unsigned executables may trigger Windows SmartScreen. Use low volume when testing the unverified synth.
