#include "sdl_backend.h"
#include "font8x8.h"
#include <SDL.h>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <algorithm>

// Screen dimensions in characters
const int SCREEN_COLS = 80;
const int SCREEN_ROWS = 45;
const int CHAR_W = 8;
const int CHAR_H = 8;
const int BASE_SCALE = 2;
const int WINDOW_W = SCREEN_COLS * CHAR_W * BASE_SCALE;
const int WINDOW_H = SCREEN_ROWS * CHAR_H * BASE_SCALE;

struct Cell {
    char ch;
    int fgColor;
};

static std::vector<std::vector<Cell>> screen;
static int cursorX = 0;
static int cursorY = 0;
static int currentColor = 37; // WHITE

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* gameTexture = nullptr;
static bool initialized = false;
static bool isFullscreen = false;

// ANSI color code to RGB mapping
static void ansiToRGB(int code, uint8_t& r, uint8_t& g, uint8_t& b) {
    switch (code) {
        case 30: r = 0;   g = 0;   b = 0;   break; // BLACK
        case 31: r = 170; g = 0;   b = 0;   break; // RED
        case 32: r = 0;   g = 170; b = 0;   break; // GREEN
        case 33: r = 170; g = 170; b = 0;   break; // YELLOW
        case 34: r = 0;   g = 0;   b = 170; break; // BLUE
        case 35: r = 170; g = 0;   b = 170; break; // MAGENTA
        case 36: r = 0;   g = 170; b = 170; break; // CYAN
        case 37: r = 192; g = 192; b = 192; break; // WHITE / GRAY
        case 90: r = 85;  g = 85;  b = 85;  break; // BRIGHT_BLACK
        case 91: r = 255; g = 85;  b = 85;  break; // BRIGHT_RED
        case 92: r = 85;  g = 255; b = 85;  break; // BRIGHT_GREEN
        case 93: r = 255; g = 255; b = 85;  break; // BRIGHT_YELLOW
        case 94: r = 85;  g = 85;  b = 255; break; // BRIGHT_BLUE
        case 95: r = 255; g = 85;  b = 255; break; // BRIGHT_MAGENTA
        case 96: r = 85;  g = 255; b = 255; break; // BRIGHT_CYAN
        case 97: r = 255; g = 255; b = 255; break; // BRIGHT_WHITE
        default: r = 192; g = 192; b = 192; break;
    }
}

static void drawChar(char ch, int x, int y, int color) {
    uint8_t r, g, b;
    ansiToRGB(color, r, g, b);
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    unsigned char uc = static_cast<unsigned char>(ch);
    const unsigned char* font = FONT_8X8[uc];

    int px = x * CHAR_W;
    int py = y * CHAR_H;

    for (int row = 0; row < CHAR_H; ++row) {
        unsigned char byte = font[row];
        for (int col = 0; col < CHAR_W; ++col) {
            if (byte & (1 << (7 - col))) {
                SDL_RenderDrawPoint(renderer, px + col, py + row);
            }
        }
    }
}

bool setupTerminal() {
    if (initialized) return true;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(
        "Dungeon Crawler",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_W,
        WINDOW_H,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        window = nullptr;
        return false;
    }

    gameTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_COLS * CHAR_W, SCREEN_ROWS * CHAR_H);
    if (!gameTexture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        window = nullptr;
        renderer = nullptr;
        return false;
    }
    SDL_SetTextureScaleMode(gameTexture, SDL_ScaleModeNearest);

    // Set a sensible default window size based on display
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        int maxScaleW = dm.w / (SCREEN_COLS * CHAR_W);
        int maxScaleH = dm.h / (SCREEN_ROWS * CHAR_H);
        int scale = std::min(maxScaleW, maxScaleH);
        if (scale < 1) scale = 1;
        if (scale > 4) scale = 4;
        int defaultScale = std::max(2, scale - 1);
        SDL_SetWindowSize(window, SCREEN_COLS * CHAR_W * defaultScale, SCREEN_ROWS * CHAR_H * defaultScale);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    screen.resize(SCREEN_ROWS, std::vector<Cell>(SCREEN_COLS, {' ', 37}));
    clearScreen();
    initialized = true;

    // Start in fullscreen by default
    isFullscreen = true;
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    return true;
}

void restoreTerminal() {
    if (!initialized) return;
    SDL_DestroyTexture(gameTexture);
    gameTexture = nullptr;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    initialized = false;
    window = nullptr;
    renderer = nullptr;
}

void clearScreen() {
    for (int y = 0; y < SCREEN_ROWS; ++y) {
        for (int x = 0; x < SCREEN_COLS; ++x) {
            screen[y][x] = {' ', 37};
        }
    }
    cursorX = 0;
    cursorY = 0;
}

void moveCursor(int x, int y) {
    cursorX = x - 1;
    cursorY = y - 1;
    if (cursorX < 0) cursorX = 0;
    if (cursorY < 0) cursorY = 0;
    if (cursorX >= SCREEN_COLS) cursorX = SCREEN_COLS - 1;
    if (cursorY >= SCREEN_ROWS) cursorY = SCREEN_ROWS - 1;
}

void setColor(int color) {
    currentColor = color;
}

void resetColor() {
    currentColor = 37;
}

static unsigned char unicodeToCP437(char32_t codepoint) {
    // Map common Unicode box drawing and symbol characters to CP437
    switch (codepoint) {
        case U'↑': return 0x18;
        case U'↓': return 0x19;
        case U'→': return 0x1A;
        case U'←': return 0x1B;
        case U'═': return 0xCD;
        case U'║': return 0xBA;
        case U'╔': return 0xC9;
        case U'╗': return 0xBB;
        case U'╚': return 0xC8;
        case U'╝': return 0xBC;
        default:
            if (codepoint < 256) return static_cast<unsigned char>(codepoint);
            return 0x3F; // '?'
    }
}

static char32_t decodeUTF8(const std::string& s, size_t& i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
        return c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        char32_t cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(s[i + 1]) & 0x3F);
        i += 1;
        return cp;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        char32_t cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(s[i + 2]) & 0x3F);
        i += 2;
        return cp;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
        char32_t cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(s[i + 3]) & 0x3F);
        i += 3;
        return cp;
    }
    return c;
}

void termPrintString(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char32_t cp = decodeUTF8(s, i);
        unsigned char ch = unicodeToCP437(cp);
        if (cursorX >= 0 && cursorX < SCREEN_COLS && cursorY >= 0 && cursorY < SCREEN_ROWS) {
            screen[cursorY][cursorX] = {static_cast<char>(ch), currentColor};
        }
        cursorX++;
        if (cursorX >= SCREEN_COLS) {
            cursorX = 0;
            cursorY++;
            if (cursorY >= SCREEN_ROWS) cursorY = SCREEN_ROWS - 1;
        }
    }
}

void refreshScreen() {
    if (!renderer || !gameTexture) return;

    // 1. Render to texture at native resolution
    SDL_SetRenderTarget(renderer, gameTexture);
    SDL_SetRenderDrawColor(renderer, 16, 16, 32, 255);
    SDL_RenderClear(renderer);

    for (int y = 0; y < SCREEN_ROWS; ++y) {
        for (int x = 0; x < SCREEN_COLS; ++x) {
            const Cell& cell = screen[y][x];
            if (cell.ch != ' ') {
                drawChar(cell.ch, x, y, cell.fgColor);
            }
        }
    }

    // 2. Render texture to window
    SDL_SetRenderTarget(renderer, nullptr);

    int winW = 0, winH = 0;
    SDL_GetWindowSize(window, &winW, &winH);

    SDL_SetRenderDrawColor(renderer, 16, 16, 32, 255);
    SDL_RenderClear(renderer);

    // Preserve aspect ratio uniformly in both windowed and fullscreen
    float scaleX = static_cast<float>(winW) / (SCREEN_COLS * CHAR_W);
    float scaleY = static_cast<float>(winH) / (SCREEN_ROWS * CHAR_H);
    float renderScale = std::min(scaleX, scaleY);
    if (renderScale < 1.0f) renderScale = 1.0f;

    int w = static_cast<int>(SCREEN_COLS * CHAR_W * renderScale);
    int h = static_cast<int>(SCREEN_ROWS * CHAR_H * renderScale);
    int offsetX = (winW - w) / 2;
    int offsetY = (winH - h) / 2;

    SDL_Rect dstRect = { offsetX, offsetY, w, h };

    SDL_RenderCopy(renderer, gameTexture, nullptr, &dstRect);
    SDL_RenderPresent(renderer);
}

char getKeyPress() {
    if (!initialized) return 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return 'q'; // Treat window close as quit
        }
        if (event.type == SDL_KEYDOWN) {
            SDL_Keycode key = event.key.keysym.sym;

            // Zoom levels
            if (key == SDLK_F1) {
                SDL_SetWindowSize(window, SCREEN_COLS * CHAR_W * 1, SCREEN_ROWS * CHAR_H * 1);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                continue;
            }
            if (key == SDLK_F2) {
                SDL_SetWindowSize(window, SCREEN_COLS * CHAR_W * 2, SCREEN_ROWS * CHAR_H * 2);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                continue;
            }
            if (key == SDLK_F3) {
                SDL_SetWindowSize(window, SCREEN_COLS * CHAR_W * 3, SCREEN_ROWS * CHAR_H * 3);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                continue;
            }
            if (key == SDLK_F4) {
                SDL_SetWindowSize(window, SCREEN_COLS * CHAR_W * 4, SCREEN_ROWS * CHAR_H * 4);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                continue;
            }

            // Fullscreen toggle
            if (key == SDLK_F11) {
                isFullscreen = !isFullscreen;
                SDL_SetWindowFullscreen(window, isFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                continue;
            }

            // Map arrow keys to WASD
            switch (key) {
                case SDLK_UP:    return 'w';
                case SDLK_DOWN:  return 's';
                case SDLK_LEFT:  return 'a';
                case SDLK_RIGHT: return 'd';
                case SDLK_KP_8:  return 'w';
                case SDLK_KP_2:  return 's';
                case SDLK_KP_4:  return 'a';
                case SDLK_KP_6:  return 'd';
                default: break;
            }

            // Map regular keys
            if (key >= SDLK_a && key <= SDLK_z) {
                return static_cast<char>(key);
            }
            if (key >= SDLK_0 && key <= SDLK_9) {
                return static_cast<char>(key);
            }
            if (key == SDLK_SPACE) {
                return ' ';
            }
            if (key == SDLK_GREATER) {
                return '>';
            }
            if (key == SDLK_QUESTION) {
                return '?';
            }
            if (key == SDLK_PERIOD) {
                return '.';
            }
            if (key == SDLK_ESCAPE) {
                return 'q';
            }
            if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                return '\r';
            }
        }
    }
    return 0;
}
