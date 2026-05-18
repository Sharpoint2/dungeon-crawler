#pragma once

#include <string>
#include <sstream>

// SDL2 backend that emulates a terminal screen for the dungeon crawler game.

void setupTerminal();
void restoreTerminal();
char getKeyPress();
void setColor(int color);
void resetColor();
void clearScreen();
void moveCursor(int x, int y);
void refreshScreen();
void termPrintString(const std::string& s);

// Variadic template for convenient printing
template<typename... Args>
void termPrint(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    termPrintString(oss.str());
}
