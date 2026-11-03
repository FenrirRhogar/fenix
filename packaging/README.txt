FENIX COLOUR SUITE  -  version @VERSION@
by fenrikos

Six mix-bus colour plugins:

  Fenix Bus       all five stages in one chain, with chain-wide presets
  Fenix Drive     harmonic saturation
  Fenix Shelf EQ  broad two-band shelving EQ
  Fenix HF Comp   high-frequency-only compressor
  Fenix Width     mid/side width and low-band space
  Fenix Iron      output transformer colour


REQUIREMENTS
------------
[linux]
- 64-bit Linux (x86_64), glibc @GLIBC@ or newer
- A VST3 or LV2 host: REAPER, Bitwig Studio, Ardour, Carla, Qtractor, ...
- X11 or XWayland for the plugin windows
[/linux]
[windows]
- Windows 10 or 11, 64-bit
- A VST3 host: REAPER, Ableton Live, FL Studio, Cubase, Studio One, Bitwig Studio, ...
[/windows]
[macos]
- macOS 10.15 or newer, Apple Silicon or Intel (universal binaries)
- An AU or VST3 host: Logic Pro, GarageBand, Ableton Live, REAPER, Cubase, ...
[/macos]


INSTALL
-------
[linux]
Option 1, the script. Open a terminal in this folder and run:

    ./install.sh             installs for your user (~/.vst3 and ~/.lv2)
    ./install.sh --system    installs for all users (/usr/lib/vst3, /usr/lib/lv2; asks for sudo)

Option 2, by hand. Copy every folder inside VST3/ into ~/.vst3/ and every folder
inside LV2/ into ~/.lv2/. Create those folders if they do not exist.

To remove the plugins, run ./uninstall.sh (add --system if you installed system-wide).
[/linux]
[windows]
Copy every "Fenix ... .vst3" folder inside VST3\ into:

    C:\Program Files\Common Files\VST3\

Windows asks for administrator permission to copy there. To remove the plugins, delete those folders.
[/windows]
[macos]
1. Copy the plugins:
     every "Fenix ... .component" in AU/    ->  /Library/Audio/Plug-Ins/Components/
     every "Fenix ... .vst3" in VST3/       ->  /Library/Audio/Plug-Ins/VST3/
   (In Finder: Go > Go to Folder..., then paste the path.)
@MAC_QUARANTINE@
To remove the plugins, delete those files again.
[/macos]

Then rescan plugins in your host. In REAPER: Options > Preferences > Plug-ins > "Re-scan".
[macos]
Logic Pro and GarageBand pick up new AU plugins after a restart.
[/macos]


USING THE PLUGINS
-----------------
- Click the preset name to see the list, the arrows step through it, and SAVE stores your own.
[linux]
  User presets live in ~/.config/fenrikos/Fenix/<plugin name>/ as .fenixpreset files.
[/linux]
[windows]
  User presets live in %APPDATA%\fenrikos\Fenix\<plugin name>\ as .fenixpreset files.
[/windows]
[macos]
  User presets live in ~/Library/fenrikos/Fenix/<plugin name>/ as .fenixpreset files.
[/macos]
- The switch at the top right bypasses the plugin; hosts also see it as the bypass parameter.
- In Fenix Bus, each stage has its own switch, and the tabs choose which stage's display is shown.
- Double-click any knob to reset it.


SUPPORT
-------
Reply to your Gumroad receipt email with your system, host and what happened.
Updates for version 1.x are free and appear in your Gumroad library.
