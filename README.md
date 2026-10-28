# The Last Escape — C++ / Raylib

A graphical, top-down pixel-art-inspired mystery survival mini-game for a C++ OOP project.

## Current gameplay
- Four rooms that can be explored by walking through side doors
- Collectibles are assigned to particular rooms
- Collect three clues and the security keycard to unlock an ending
- Find hidden evidence for the secret ending
- Enemies chase the player in their rooms and can hurt the player
- Health, energy, inventory, flashlight toggle, sprinting, saving/loading and score records
- OOP-style structures for `Player`, `Enemy`, `Item`, and `Room`

## Install Raylib on macOS

```bash
brew install raylib pkg-config
```

## Open and run

1. Extract this ZIP file.
2. In VS Code, choose **File → Open Folder...** and select `the_last_escape_raylib`.
3. Choose **Terminal → New Terminal**.
4. Compile and run:

```bash
g++ main.cpp -std=c++17 -o the_last_escape $(pkg-config --cflags --libs raylib)
./the_last_escape
```

## Controls

- `1` — New game (title screen)
- `2` — Load saved game (title screen)
- `3` — How to play
- `4` — Records
- `5` — Quit
- `W A S D` or arrow keys — Move
- `E` — Pick up a nearby glowing item / try the exit
- `F` — Turn flashlight on/off after collecting it
- `Shift` — Sprint (uses energy)
- `I` — Inventory
- `P` — Save progress
- `ESC` — Return to title menu / leave inventory

## Objective
Explore all four rooms. Press `E` when the **PRESS E** prompt appears near a glowing object. Collect 3 clues and the keycard, then reach the exit in the final room. Collect the secret evidence as well to unlock the secret ending. Avoid the red creature.

Save files are created in the working directory as `last_escape_save.txt` and `last_escape_records.txt`.

This is a student mini-project starter with drawn pixel-style shapes (no external sprite assets required), not a commercial-quality finished game. It can be extended with sprite sheets, audio, and more elaborate puzzles.
