# Dungeon Crawler (C++)

A terminal-based roguelike dungeon crawler written in modern C++.

## Features

- Procedural dungeon generation
- Turn-based combat
- 25 dungeon levels
- 6 enemy types with distinct difficulty
- Potion and progression systems
- Fog of war / field of view
- Roguelike permadeath rules
- Win condition with the Amulet of Yendor

## Requirements

- C++17-compatible compiler
- CMake 3.10+
- Linux/Unix terminal (uses `termios`)

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Run

From the `build` directory:

```bash
./dungeon_crawler
```

When a terminal emulator is available, `dungeon_crawler` opens the game in a new window and then exits. If no compatible terminal emulator is installed, it falls back to the current terminal.

The actual game binary is built as `dungeon_crawler_game`.

## Controls

- `WASD` or arrow keys: Move
- `HJKL`: Alternative movement
- `Space`: Rest
- `G`: Pick up/use item
- `>`: Descend stairs
- `?`: Help

## Objective

Descend to level 25, find the Amulet of Yendor, and return to level 1 alive.

## Project Structure

- `src/dungeon_crawler.cpp`: Main game implementation
- `CMakeLists.txt`: Build configuration

## License

This project is provided as a programming example.
