<div align="center">

# ♟️ Chess

**A lightweight, native Windows chess game—with a clickable board, drag-and-drop moves, and a built-in computer opponent.**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17.html)
[![Windows](https://img.shields.io/badge/Platform-Windows-0078D4?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![GitHub stars](https://img.shields.io/github/stars/SrOdranoel/cpp-console-chess?style=social)](https://github.com/SrOdranoel/cpp-console-chess/stargazers)

[Get the source](https://github.com/SrOdranoel/cpp-console-chess) · [Report a bug](https://github.com/SrOdranoel/cpp-console-chess/issues) · [Request a feature](https://github.com/SrOdranoel/cpp-console-chess/issues)

</div>

## About

Enjoy a game of chess in a clean, responsive desktop interface. Play against the computer as either color, challenge a friend on the same PC, or try different AI search depths. The board is drawn with native Windows graphics, and the chess engine is included in the project—no third-party packages to install.

## Features

- **Three ways to play:** White vs. computer, Black vs. computer, or two-player local chess.
- **Mouse-first controls:** Drag a piece or click it and its destination. Legal moves and captures are highlighted.
- **A chess-playing AI:** Choose Easy, Medium, or Hard search depth.
- **Core chess rules:** Legal moves, check and checkmate, castling, en passant, and queen/rook/bishop/knight promotion.
- **Game helpers:** Move history, undo, board flip, and draw detection for stalemate, the fifty-move rule, and insufficient material.
- **Native Windows app:** Win32 + GDI+, written in C++17, with no external runtime dependencies.

## Quick start

### Option 1: Build from source

1. Install **Visual Studio 2019 Build Tools** with the C++ build tools and Windows SDK. The supplied build script expects the default Build Tools installation path:
   `C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat`
2. Clone this repository or download the source ZIP from GitHub.
3. Run `build.bat` from the project folder.
4. Launch `chess.exe`.

The script compiles with MSVC in C++17 mode and places intermediate build files in `build/`.

### Option 2: Download the source

Use GitHub's **Code → Download ZIP**, extract the archive, then follow the build steps above. A prebuilt release is not currently provided.

## How to play

1. Start the app and choose a game mode. In a game against the computer, choose a difficulty from the menu first if you want to change it.
2. Select a piece by clicking or dragging it. Available moves appear on the board.
3. Click or release on a highlighted destination. For a pawn promotion, select the piece you want.
4. Use **Undo** to take back a turn, **Flip board** to change the view, or **Main menu** to start another game.

The AI difficulty setting changes its search depth: Easy = 1, Medium = 2, Hard = 3. Deeper searches can take longer to choose a move.

## Tech stack

- C++17 and the standard library
- Win32 API for the application window and mouse input
- GDI+ for the board and piece rendering
- MSVC build script: `build.bat`

## Project files

- `main.cpp` — chess rules, AI, and graphical interface
- `build.bat` — MSVC build script
- `.gitignore` — excludes generated build artifacts

## Limitations

This is a small, locally played chess game. It does not currently include online multiplayer, save/load, chess clocks, or threefold-repetition detection. The AI is intentionally lightweight rather than a tournament-strength engine.

## Contributing

Bug reports, ideas, and pull requests are welcome. Please include the Windows version and steps to reproduce when reporting a problem.

If you enjoy the project, a ⭐ on GitHub helps others find it!
