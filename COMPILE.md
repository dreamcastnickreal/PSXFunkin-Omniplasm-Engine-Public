# Compiling PSXFunkin: Omniplasm Engine

## Setting up the Development Environment
First off, you'll need a terminal one way or another.

### Windows
On Windows, you basically have two choices:
- MSYS2 (i'll just provide my pre-setup msys2, since i am bad at explaining shit to you guys. it already has mkpsxiso compiled and other optimizations ABSOLUTELY NESSESSARY for omniplasm ngine)

### MSYS2
Well, here you go: https://drive.google.com/file/d/1yHJosGwlBV0evd4maSpxu5lGNC08SViv/view?usp=sharing

## Copying PsyQ files
First, go to the [mips](/mips/) folder of the repo, and create a new folder named `psyq`.

Then, download the converted PsyQ library from http://psx.arthus.net/sdk/Psy-Q/psyq-4_7-converted-light.zip. Just extract the contents of this into the new `psyq` folder.

## Compiling PSXFunkin
First, make sure to `cd` to the repo directory where all the makefiles are. You're gonna want to run a few commands from here, You'll need to either get a PSX license file and save it as licensea.dat in the same directory as funkin.xml (you can get them at http://www.psxdev.net/downloads.html 's `PsyQ SDK`), or remove the referencing line `<license file="licensea.dat"/>` from funkin.xml. Without the license file, the game may fail on a bunch of emulators due to bios checks (unless you use fast boot, I believe?)

TIP: For any make, try appending `-jX` to the end of it, where X is the number of CPU cores you have times two. This will try to put as much of your CPU as it can to doing whatever it needs to do and makes it go way quicker.

`make -f Makefile.assets` this will compile all the assets.

`make -f Makefile.assets small` this will compile optimized code versions of all the assets.

`make -f Makefile.assets clean` this gives y/n prompts to remove compiled assets incase things go wrong.

You can read more about the asset formats in [FORMATS.md](/FORMATS.md)

If everything went well, you should have a `funkin_discX.bin` and a `funkin_discX.cue` (X being the disc number.) in the same directory.
