FENIX COLOUR SUITE  -  version @VERSION@
by fenrikos

Six mix-bus colour plugins for Linux, in VST3 and LV2 formats:

  Fenix Bus       all five stages in one chain, with chain-wide presets
  Fenix Drive     harmonic saturation
  Fenix Shelf EQ  broad two-band shelving EQ
  Fenix HF Comp   high-frequency-only compressor
  Fenix Width     mid/side width and low-band space
  Fenix Iron      output transformer colour


REQUIREMENTS
------------
- 64-bit Linux (x86_64), glibc @GLIBC@ or newer
- A VST3 or LV2 host: REAPER, Bitwig Studio, Ardour, Carla, Qtractor, ...
- X11 or XWayland for the plugin windows


INSTALL
-------
Option 1, the script. Open a terminal in this folder and run:

    ./install.sh             installs for your user (~/.vst3 and ~/.lv2)
    ./install.sh --system    installs for all users (/usr/lib/vst3, /usr/lib/lv2; asks for sudo)

Option 2, by hand. Copy every folder inside VST3/ into ~/.vst3/ and every folder
inside LV2/ into ~/.lv2/. Create those folders if they do not exist.

Then rescan plugins in your host. In REAPER: Options > Preferences > Plug-ins > VST >
"Re-scan".

To remove the plugins, run ./uninstall.sh (add --system if you installed system-wide).


USING THE PLUGINS
-----------------
- Click the preset name to see the list, the arrows step through it, and SAVE stores your own.
  User presets live in ~/.config/fenrikos/Fenix/<plugin name>/ as .fenixpreset files.
- The switch at the top right bypasses the plugin; hosts also see it as the bypass parameter.
- In Fenix Bus, each stage has its own switch, and the tabs choose which stage's display is shown.
- Double-click any knob to reset it.


SUPPORT
-------
Reply to your Gumroad receipt email with the host, distro and what happened.
Updates for version 1.x are free and appear in your Gumroad library.
