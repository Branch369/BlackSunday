BLACK SUNDAY
A dust-storm survival game for the Macintosh Plus.

Harrow, Oklahoma, April 1935. The storm has not stopped in nine days.
Somewhere past the edge of town the trains still run. Find the depot.

The town is endless and different every game. Nothing is stored: each
cell is computed from a seed and its coordinates, so memory use stays
flat however far you walk.


STATUS - READ THIS FIRST

  Version 2. The first version was built with Retro68 (GCC 16.1) on
  GitHub and played in the Infinite Mac emulator as a Mac Plus.

  Tested:      the game logic (src/game.c, src/tiles.c), on a modern
               machine: strict C89 compile, seeds checked for a
               walkable route to the depot, random-input fuzzing with
               memory checking, a simple bot that wins about a third
               of its games, and a check that the new partial screen
               redraw always matches a full redraw.
  NOT tested:  this version of src/main_mac.c. The compass, the bars,
               the damage flash and the faster drawing are new and have
               not been compiled or run on a Mac yet.

  If the map ever draws as garbage, set DIRECT_BLIT to 0 near the top
  of src/main_mac.c. That returns to the slower, safer drawing path.


HOW TO PLAY

  Arrow keys or W A S D   walk; walk into a thing to hit it
  Space                   wait a turn
  L                       lantern on/off (burns oil)
  F, then a direction     fire the shotgun
  R                       breathe through a clean rag (clears lungs)
  B                       bandage
  Cmd-N / Cmd-Q           new game / quit

  Outside, your lungs fill. At 100% you start dying. Under a roof they
  clear. Houses and stores hold oil, rags, shells and bandages, and
  sometimes something else.

  The lantern lets you see farther. It also lets you be seen.
  The shotgun is loud.

  Husks       slow. They follow light and noise and can open doors.
  Hounds      fast. They live in the wind and will not come indoors.
  Scarecrows  only move when you are not looking at them. You face the
              way you last tried to walk. Turning toward a wall or a
              fence costs no time.

  THE SCREEN

  The strip under the map shows health as five blocks, lungs as a
  meter, and what you are carrying. When it turns black you are in
  trouble: lungs at 75% or more, or health at 2 or less. The whole map
  flashes when something hurts you.

  The compass in the corner is how you find the depot. Every twenty
  turns a train whistle blows and one mark on the compass flashes: that
  is the way to go. The mark stays lit until the next whistle. North
  is the top of the screen.


BUILDING WITH RETRO68 (on a PC)

  Retro68 is an open-source GCC cross-compiler for 68k Macs:
  https://github.com/autc04/Retro68

  With the project's Docker image, from this folder:

    docker run --rm -v "$(pwd)":/root -i ghcr.io/autc04/retro68 /bin/bash <<"EOF"
    rm -rf build && mkdir build && cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
    make
    EOF

  That leaves build/BlackSunday.dsk (a floppy image), BlackSunday.bin
  (MacBinary) and BlackSunday.APPL. Mini vMac will take the .dsk
  directly. Check the Retro68 README if the image name or paths have
  changed.


BUILDING IN THE CLOUD (nothing to install)

  The project includes .github/workflows/build.yml. Put these files in
  a GitHub repository and GitHub builds the game on every push. Open
  the repository's Actions tab, click the newest run, and download the
  "BlackSunday" artifact: a zip holding BlackSunday.dsk and
  BlackSunday.bin.

  To play in a browser, open infinitemac.org, start a Mac Plus with
  System 6, and drag the file onto the page. It appears inside the
  emulated Mac.

  This workflow has not been run yet. If the build step goes red, the
  compiler error in its log says what to fix.


BUILDING WITH THINK C (on the Mac or in an emulator)

  Use the copies in the thinkc folder: they have classic Mac line
  endings. They must arrive as TEXT files.

  1. New project. Add main_mac.c, game.c, tiles.c and the MacTraps
     library.
  2. Leave "MacHeaders" turned on (the default).
  3. Build Application.

  THINK C's 16-bit int is accounted for: the code never uses plain int.


WHAT IT NEEDS

  Macintosh Plus or later, System 6 or 7, any amount of RAM the Plus
  can hold. The program keeps a few kilobytes of state and one
  864-byte tile bitmap. It has no resources and uses no C library.

  test/main_term.c is the test harness for a modern machine. It is not
  part of the Mac build.
