# Vanta Beta 0.3 — native UI test

Build on GitHub Actions or on Windows using CMake and Inno Setup. This source has not been compiled or tested here. The native, flat red/black VANTA interface includes Main, OSC, Effects, Modulation, Settings and a demo preset browser. Only level, attack, release and sine/saw waveform affect the basic 16-voice engine. All other controls, settings, partials and preset names are visual mockups: they do not change sound or load preset files, and their state is not saved. External preset formats are not supported.

Upload the CONTENTS of this directory including .github to the repository, replacing Beta 0.2 files. Run the workflow and download the Vanta-Beta-0.3-Windows artifact. If build fails, share the error. The installer is unsigned. Back up FL Studio projects, uninstall the older beta before installing the new version to avoid duplicates, then rescan plugins. Use low volume. Review JUCE licensing before distributing.
