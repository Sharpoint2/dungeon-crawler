#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <iomanip>

// Color codes
enum Color {
    BLACK = 30, RED = 31, GREEN = 32, YELLOW = 33,
    BLUE = 34, MAGENTA = 35, CYAN = 36, WHITE = 37,
    GRAY = 37, BRIGHT_BLACK = 90, BRIGHT_RED = 91, BRIGHT_GREEN = 92,
    BRIGHT_YELLOW = 93, BRIGHT_BLUE = 94, BRIGHT_MAGENTA = 95,
    BRIGHT_CYAN = 96, BRIGHT_WHITE = 97
};

// Global random generator
class Random {
private:
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist;
    
public:
    Random() {
        gen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    int getInt(int min, int max) {
        if(min > max) std::swap(min, max);
        std::uniform_int_distribution<int> d(min, max);
        return d(gen);
    }
    
    float getFloat(float min, float max) {
        if(min > max) std::swap(min, max);
        std::uniform_real_distribution<float> d(min, max);
        return d(gen);
    }
    
    bool getBool(float probability = 0.5f) {
        return getFloat(0.0f, 1.0f) < probability;
    }
} g_random;

// Terminal control
void setColor(int color) {
    std::cout << "\033[" << color << "m";
}

void resetColor() {
    std::cout << "\033[0m";
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void moveCursor(int x, int y) {
    std::cout << "\033[" << y << ";" << x << "H";
}

void setupTerminal() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void restoreTerminal() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

char getKeyPress() {
    char key = 0;
    if (std::cin.get(key)) {
        return key;
    }
    return 0;
}

void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Entity types
enum class TileType { WALL, FLOOR, DOOR, STAIRS_DOWN, EMPTY };

enum class EntityType { PLAYER, ENEMY, ITEM, STAIRS };

struct Stats {
    int hp, maxHp;
    int mp, maxMp;
    int strength, defense;
    int level, exp, expToNext;
    
    Stats() : hp(10), maxHp(10), mp(5), maxMp(5), 
              strength(1), defense(0), level(1), exp(0), expToNext(10) {}
};

class Entity {
public:
    std::string name;
    EntityType type;
    int x, y;
    char symbol;
    int color;
    Stats stats;
    bool destroyed;
    std::string description;
    
    Entity(const std::string& n, EntityType t, char s, int c) 
        : name(n), type(t), symbol(s), color(c), destroyed(false) {}
    
    virtual ~Entity() {}
    
    void takeDamage(int damage) {
        int actualDamage = std::max(1, damage - stats.defense);
        stats.hp -= actualDamage;
        if (stats.hp <= 0) {
            stats.hp = 0;
            destroyed = true;
        }
    }
    
    void gainExp(int amount) {
        stats.exp += amount;
        while (stats.exp >= stats.expToNext) {
            levelUp();
        }
    }
    
    void levelUp() {
        stats.level++;
        stats.exp -= stats.expToNext;
        stats.expToNext = stats.level * 10;
        stats.maxHp += g_random.getInt(3, 7);
        stats.hp = stats.maxHp;
        stats.strength += g_random.getInt(0, 2);
        stats.defense += g_random.getInt(0, 1);
    }
    
    virtual void update() {}
};

using EntityPtr = std::shared_ptr<Entity>;

// Map
const int MAP_WIDTH = 80;
const int MAP_HEIGHT = 24;

class Map {
public:
    struct Cell {
        TileType tile;
        bool explored;
        bool visible;
        
        Cell() : tile(TileType::WALL), explored(false), visible(false) {}
    };
    
    std::vector<std::vector<Cell>> cells;
    std::vector<EntityPtr> entities;
    int level;
    
    Map(int lvl) : level(lvl) {
        cells.resize(MAP_WIDTH, std::vector<Cell>(MAP_HEIGHT));
        generateDungeon();
    }
    
    void generateDungeon() {
        // Fill with walls
        for (int x = 0; x < MAP_WIDTH; x++) {
            for (int y = 0; y < MAP_HEIGHT; y++) {
                cells[x][y].tile = TileType::WALL;
            }
        }
        
        // Create rooms
        int numRooms = g_random.getInt(5, 10);
        std::vector<std::pair<int, int>> roomCenters;
        
        for (int i = 0; i < numRooms; i++) {
            int w = g_random.getInt(5, 12);
            int h = g_random.getInt(4, 8);
            int x = g_random.getInt(1, MAP_WIDTH - w - 1);
            int y = g_random.getInt(1, MAP_HEIGHT - h - 1);
            
            createRoom(x, y, w, h);
            roomCenters.push_back({x + w/2, y + h/2});
        }
        
        // Connect rooms
        for (size_t i = 1; i < roomCenters.size(); i++) {
            createTunnel(roomCenters[i-1].first, roomCenters[i-1].second,
                        roomCenters[i].first, roomCenters[i].second);
        }
        
        // Add stairs down in last room
        auto lastRoom = roomCenters.back();
        cells[lastRoom.first][lastRoom.second].tile = TileType::STAIRS_DOWN;
        
        // Place player in first room
        auto firstRoom = roomCenters.front();
        addEntity(createPlayer(firstRoom.first, firstRoom.second));
        
        // Spawn enemies
        for (int i = 1; i < numRooms; i++) {
            if (g_random.getBool(0.7f)) {
                auto room = roomCenters[i];
                addEntity(createRandomEnemy(room.first, room.second));
            }
        }
        
        // Spawn items
        for (int i = 0; i < numRooms / 2; i++) {
            auto room = roomCenters[g_random.getInt(1, roomCenters.size() - 1)];
            if (g_random.getBool(0.5f)) {
                addEntity(createRandomPotion(room.first, room.second));
            }
        }
    }
    
    void createRoom(int x, int y, int w, int h) {
        for (int ix = x; ix < x + w; ix++) {
            for (int iy = y; iy < y + h; iy++) {
                if (ix >= 0 && ix < MAP_WIDTH && iy >= 0 && iy < MAP_HEIGHT) {
                    cells[ix][iy].tile = TileType::FLOOR;
                }
            }
        }
    }
    
    void createTunnel(int x1, int y1, int x2, int y2) {
        int x = x1;
        int y = y1;
        
        while (x != x2 || y != y2) {
            if (x < x2) x++;
            else if (x > x2) x--;
            else if (y < y2) y++;
            else if (y > y2) y--;
            
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                if (cells[x][y].tile == TileType::WALL) {
                    cells[x][y].tile = TileType::FLOOR;
                }
            }
        }
    }
    
    bool isWalkable(int x, int y) {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return false;
        return cells[x][y].tile == TileType::FLOOR || 
               cells[x][y].tile == TileType::DOOR ||
               cells[x][y].tile == TileType::STAIRS_DOWN;
    }
    
    void addEntity(EntityPtr entity) {
        entities.push_back(entity);
    }
    
    void removeEntity(EntityPtr entity) {
        entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
    }
    
    std::vector<EntityPtr> getEntitiesAt(int x, int y) {
        std::vector<EntityPtr> result;
        for (auto& e : entities) {
            if (e->x == x && e->y == y) {
                result.push_back(e);
            }
        }
        return result;
    }
    
    void updateFOV(int px, int py, int radius) {
        // Reset visibility
        for (int x = 0; x < MAP_WIDTH; x++) {
            for (int y = 0; y < MAP_HEIGHT; y++) {
                cells[x][y].visible = false;
            }
        }
        
        // Simple FOV: everything within radius
        for (int x = px - radius; x <= px + radius; x++) {
            for (int y = py - radius; y <= py + radius; y++) {
                if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                    int dx = x - px;
                    int dy = y - py;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (dist <= radius) {
                        cells[x][y].visible = true;
                        cells[x][y].explored = true;
                    }
                }
            }
        }
    }
    
    EntityPtr createPlayer(int x, int y);
    EntityPtr createRandomEnemy(int x, int y);
    EntityPtr createRandomPotion(int x, int y);
};

// Create game entities
EntityPtr Map::createPlayer(int x, int y) {
    auto player = std::make_shared<Entity>("Player", EntityType::PLAYER, '@', Color::YELLOW);
    player->x = x;
    player->y = y;
    player->stats.hp = 30;
    player->stats.maxHp = 30;
    player->stats.mp = 10;
    player->stats.maxMp = 10;
    player->stats.strength = 3;
    player->stats.defense = 1;
    return player;
}

EntityPtr Map::createRandomEnemy(int x, int y) {
    const char* names[] = {"Rat", "Bat", "Goblin", "Orc", "Troll", "Dragon"};
    const char symbols[] = {'r', 'b', 'g', 'o', 'T', 'D'};
    const int colors[] = {Color::WHITE, Color::BRIGHT_BLACK, Color::GREEN, Color::RED, Color::RED, Color::RED};
    
    int tier = std::min(level / 3 + 1, 6);
    int idx = g_random.getInt(0, tier - 1);
    
    auto enemy = std::make_shared<Entity>(names[idx], EntityType::ENEMY, symbols[idx], colors[idx]);
    enemy->x = x;
    enemy->y = y;
    enemy->stats.hp = 5 + level * g_random.getInt(1, 4);
    enemy->stats.maxHp = enemy->stats.hp;
    enemy->stats.strength = 1 + idx;
    enemy->stats.defense = idx / 2;
    enemy->stats.level = idx + 1;
    
    return enemy;
}

EntityPtr Map::createRandomPotion(int x, int y) {
    const char* names[] = {"Healing Potion", "Mana Potion", "Strength Potion"};
    const char symbols[] = {'!', '!', '!'};
    const int colors[] = {Color::RED, Color::BLUE, Color::GREEN};
    
    int idx = g_random.getInt(0, 2);
    
    auto potion = std::make_shared<Entity>(names[idx], EntityType::ITEM, symbols[idx], colors[idx]);
    potion->x = x;
    potion->y = y;
    potion->description = (idx == 0) ? "Restores health" : (idx == 1) ? "Restores mana" : "Boosts strength";
    
    return potion;
}

// UI System
class UI {
public:
    std::vector<std::pair<std::string, int>> messages;
    
    UI() {
        messages.reserve(100);
    }
    
    void addMessage(const std::string& msg, int color = Color::WHITE) {
        messages.push_back({msg, color});
        if (messages.size() > 100) {
            messages.erase(messages.begin());
        }
    }
    
    void renderMessages() {
        int y = 26;
        int start = std::max(0, (int)messages.size() - 3);
        
        for (int i = start; i < (int)messages.size(); i++) {
            moveCursor(1, y++);
            setColor(messages[i].second);
            std::cout << messages[i].first;
            resetColor();
        }
    }
    
    void renderStatus(EntityPtr player) {
        moveCursor(1, 1);
        setColor(Color::BRIGHT_CYAN);
        std::cout << "╔════════════════════════════════════════════════════════════════════════════╗";
        
        moveCursor(1, 2);
        std::cout << "║ ";
        resetColor();
        
        // Player info
        setColor(Color::YELLOW);
        std::cout << "HP: " << player->stats.hp << "/" << player->stats.maxHp;
        resetColor();
        std::cout << " | ";
        setColor(Color::BLUE);
        std::cout << "MP: " << player->stats.mp << "/" << player->stats.maxMp;
        resetColor();
        std::cout << " | ";
        setColor(Color::GREEN);
        std::cout << "LVL: " << player->stats.level;
        resetColor();
        std::cout << " | ";
        setColor(Color::MAGENTA);
        std::cout << "EXP: " << player->stats.exp << "/" << player->stats.expToNext;
        resetColor();
        
        setColor(Color::BRIGHT_CYAN);
        std::cout << " ║";
        
        moveCursor(1, 3);
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝";
        resetColor();
    }
    
    void renderMap(Map& map, EntityPtr player) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            moveCursor(1, y + 4);
            for (int x = 0; x < MAP_WIDTH; x++) {
                const auto& cell = map.cells[x][y];
                
                if (!cell.explored) {
                    std::cout << ' ';
                    continue;
                }
                
                if (!cell.visible) {
                    setColor(Color::BRIGHT_BLACK);
                    if (cell.tile == TileType::WALL) std::cout << '#';
                    else std::cout << '.';
                    resetColor();
                    continue;
                }
                
                // Check for entities
                bool rendered = false;
                for (auto& e : map.entities) {
                    if (e->x == x && e->y == y && !e->destroyed) {
                        setColor(e->color);
                        std::cout << e->symbol;
                        resetColor();
                        rendered = true;
                        break;
                    }
                }
                
                if (!rendered) {
                    switch(cell.tile) {
                        case TileType::WALL:
                            setColor(Color::WHITE);
                            std::cout << '#';
                            break;
                        case TileType::FLOOR:
                            setColor(Color::BRIGHT_BLACK);
                            std::cout << '.';
                            break;
                        case TileType::STAIRS_DOWN:
                            setColor(Color::YELLOW);
                            std::cout << '>';
                            break;
                        default:
                            std::cout << ' ';
                    }
                    resetColor();
                }
            }
        }
    }
    
    void showMainMenu() {
        clearScreen();
        moveCursor(1, 10);
        setColor(Color::BRIGHT_CYAN);
        std::cout << "╔════════════════════════════════════════════════════════════════════════════╗";
        moveCursor(1, 11);
        std::cout << "║                                                                            ║";
        moveCursor(1, 12);
        std::cout << "║               DUNGEON CRAWLER - RogueLike Adventure                        ║";
        moveCursor(1, 13);
        std::cout << "║                                                                            ║";
        moveCursor(1, 14);
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝";
        resetColor();
        
        moveCursor(30, 16);
        std::cout << "1. New Game";
        moveCursor(30, 17);
        std::cout << "2. Help";
        moveCursor(30, 18);
        std::cout << "3. Quit";
        
        moveCursor(1, 24);
        std::cout << "Use 1-3 to select, WASD/Arrows to move, Space to rest";
    }
    
    void showHelp() {
        clearScreen();
        setColor(Color::BRIGHT_YELLOW);
        std::cout << "╔════════════════════════════════════════════════════════════════════════════╗";
        moveCursor(1, 2);
        std::cout << "║                             HELP SCREEN                                    ║";
        moveCursor(1, 3);
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝";
        resetColor();
        
        moveCursor(5, 5);
        std::cout << "Movement:";
        moveCursor(5, 6);
        std::cout << "  W/↑ - Move up";
        moveCursor(5, 7);
        std::cout << "  S/↓ - Move down";
        moveCursor(5, 8);
        std::cout << "  A/← - Move left";
        moveCursor(5, 9);
        std::cout << "  D/→ - Move right";
        
        moveCursor(5, 11);
        std::cout << "Actions:";
        moveCursor(5, 12);
        std::cout << "  Space - Rest (heal slowly)";
        moveCursor(5, 13);
        std::cout << "  G - Pick up items";
        moveCursor(5, 14);
        std::cout << "  > - Use stairs down";
        
        moveCursor(5, 16);
        std::cout << "Combat:";
        moveCursor(5, 17);
        std::cout << "  Move into enemies to attack";
        moveCursor(5, 18);
        std::cout << "  Different enemies have different strength";
        
        moveCursor(5, 20);
        std::cout << "Goal:";
        moveCursor(5, 21);
        std::cout << "  Descend all dungeon levels (25 floors)";
        moveCursor(5, 22);
        std::cout << "  Find the Amulet of Yendor on floor 25!";
        
        moveCursor(30, 24);
        std::cout << "Press any key to continue...";
    }
};

// Game class
class Game {
public:
    enum class State { MENU, PLAYING, HELP, VICTORY, DEFEAT };
    
    State state;
    std::unique_ptr<Map> map;
    EntityPtr player;
    UI ui;
    int dungeonLevel;
    bool amuletFound;
    
    Game() : state(State::MENU), dungeonLevel(1), amuletFound(false) {}
    
    void run() {
        setupTerminal();
        clearScreen();
        
        while (state != State::VICTORY && state != State::DEFEAT) {
            switch (state) {
                case State::MENU:
                    runMenu();
                    break;
                case State::PLAYING:
                    runGame();
                    break;
                case State::HELP:
                    runHelp();
                    break;
            }
        }
        
        runEndScreen();
        restoreTerminal();
    }
    
    void runMenu() {
        ui.showMainMenu();
        
        char key = 0;
        while (key == 0) {
            key = getKeyPress();
            if (key == 0) sleepMs(50);
        }
        
        if (key == '1') {
            startNewGame();
        } else if (key == '2') {
            state = State::HELP;
        } else if (key == '3' || key == 'q' || key == 'Q') {
            state = State::DEFEAT;
        }
    }
    
    void runHelp() {
        ui.showHelp();
        
        char key = 0;
        while (key == 0) {
            key = getKeyPress();
            if (key == 0) sleepMs(50);
        }
        state = State::MENU;
    }
    
    void startNewGame() {
        dungeonLevel = 1;
        amuletFound = false;
        map = std::make_unique<Map>(dungeonLevel);
        
        // Find player
        for (auto& e : map->entities) {
            if (e->type == EntityType::PLAYER) {
                player = e;
                break;
            }
        }
        
        state = State::PLAYING;
        ui.addMessage("Welcome to Dungeon Crawler!", Color::GREEN);
        ui.addMessage("Descend 25 levels and find the Amulet of Yendor!", Color::CYAN);
    }
    
    void runGame() {
        // Update FOV
        map->updateFOV(player->x, player->y, 10);
        
        // Render
        ui.renderStatus(player);
        ui.renderMap(*map, player);
        ui.renderMessages();
        
        // Handle input
        char key = getKeyPress();
        if (key != 0) {
            handleInput(key);
        } else {
            sleepMs(10);
        }
        
        // Check win/lose conditions
        if (player->destroyed) {
            state = State::DEFEAT;
        }
        
        if (amuletFound && dungeonLevel == 1) {
            state = State::VICTORY;
        }
    }
    
    void handleInput(char key) {
        int dx = 0, dy = 0;
        
        switch (key) {
            case 'w': case 'W': case 'k': dy = -1; break;
            case 's': case 'S': case 'j': dy = 1; break;
            case 'a': case 'A': case 'h': dx = -1; break;
            case 'd': case 'D': case 'l': dx = 1; break;
            case ' ': 
                player->stats.hp = std::min(player->stats.maxHp, player->stats.hp + 1);
                ui.addMessage("You rest for a moment.", Color::GREEN);
                takeTurn();
                return;
            case 'g': case 'G':
                pickupItem();
                return;
            case '>':
                useStairs();
                return;
            case '?':
                state = State::HELP;
                return;
            default: return;
        }
        
        if (dx != 0 || dy != 0) {
            int newX = player->x + dx;
            int newY = player->y + dy;
            
            if (map->isWalkable(newX, newY)) {
                // Check for enemies
                bool attacked = false;
                for (auto& e : map->entities) {
                    if (e->type == EntityType::ENEMY && e->x == newX && e->y == newY && !e->destroyed) {
                        attackEntity(player, e);
                        attacked = true;
                        break;
                    }
                }
                
                if (!attacked) {
                    player->x = newX;
                    player->y = newY;
                }
                
                takeTurn();
            }
        }
    }
    
    void attackEntity(EntityPtr attacker, EntityPtr target) {
        int damage = attacker->stats.strength + g_random.getInt(1, 4);
        target->takeDamage(damage);
        
        std::string msg = attacker->name + " hits " + target->name + " for " + std::to_string(damage) + " damage!";
        ui.addMessage(msg, Color::RED);
        
        if (target->destroyed) {
            ui.addMessage(target->name + " is slain!", Color::BRIGHT_RED);
            
            if (target->type == EntityType::ENEMY) {
                int xp = target->stats.level * 10;
                player->gainExp(xp);
                ui.addMessage("You gain " + std::to_string(xp) + " XP!", Color::GREEN);
                
                // Chance to drop gold
                if (g_random.getBool(0.3f)) {
                    int gold = g_random.getInt(1, 20) * dungeonLevel;
                    ui.addMessage("You find " + std::to_string(gold) + " gold pieces!", Color::YELLOW);
                }
            }
        }
    }
    
    void pickupItem() {
        auto items = map->getEntitiesAt(player->x, player->y);
        for (auto& item : items) {
            if (item->type == EntityType::ITEM) {
                if (item->name == "Healing Potion") {
                    player->stats.hp = std::min(player->stats.maxHp, player->stats.hp + g_random.getInt(5, 15));
                    ui.addMessage("You drink the healing potion!", Color::GREEN);
                } else if (item->name == "Mana Potion") {
                    player->stats.mp = std::min(player->stats.maxMp, player->stats.mp + g_random.getInt(3, 8));
                    ui.addMessage("You drink the mana potion!", Color::BLUE);
                } else if (item->name == "Strength Potion") {
                    player->stats.strength++;
                    ui.addMessage("You feel stronger!", Color::GREEN);
                }
                
                // Special: Amulet of Yendor on level 25
                if (dungeonLevel == 25 && g_random.getBool(0.3f)) {
                    if (!amuletFound) {
                        amuletFound = true;
                        ui.addMessage("You found the Amulet of Yendor! ESCAPE NOW!", Color::BRIGHT_YELLOW);
                    }
                }
                
                map->removeEntity(item);
                return;
            }
        }
        ui.addMessage("There is nothing here to pick up.", Color::YELLOW);
    }
    
    void useStairs() {
        const auto& cell = map->cells[player->x][player->y];
        if (cell.tile == TileType::STAIRS_DOWN) {
            dungeonLevel++;
            map = std::make_unique<Map>(dungeonLevel);
            
            // Find new player position
            for (auto& e : map->entities) {
                if (e->type == EntityType::PLAYER) {
                    player = e;
                    break;
                }
            }
            
            ui.addMessage("You descend to dungeon level " + std::to_string(dungeonLevel) + "!", Color::CYAN);
            
            // Special message
            if (dungeonLevel == 25 && !amuletFound) {
                ui.addMessage("This is the deepest level... the amulet must be here!", Color::MAGENTA);
            }
        } else {
            ui.addMessage("There are no stairs here.", Color::YELLOW);
        }
    }
    
    void takeTurn() {
        // Update enemies
        for (auto& e : map->entities) {
            if (e->type == EntityType::ENEMY && !e->destroyed) {
                updateEnemy(e);
            }
        }
        
        // Remove destroyed entities
        map->entities.erase(
            std::remove_if(map->entities.begin(), map->entities.end(), 
                          [](const EntityPtr& e) { return e->destroyed; }),
            map->entities.end());
    }
    
    void updateEnemy(EntityPtr enemy) {
        int dx = player->x - enemy->x;
        int dy = player->y - enemy->y;
        float dist = std::sqrt(dx*dx + dy*dy);
        
        if (dist < 10) {
            // Move towards player if adjacent
            if (dist <= 1.5f) {
                attackEntity(enemy, player);
            } else if (dist < 5) {
                // Move one step
                int moveX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
                int moveY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
                
                int newX = enemy->x + moveX;
                int newY = enemy->y + moveY;
                
                if (map->isWalkable(newX, newY)) {
                    bool blocked = false;
                    for (auto& e : map->entities) {
                        if (e->x == newX && e->y == newY && !e->destroyed) {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) {
                        enemy->x = newX;
                        enemy->y = newY;
                    }
                }
            }
        }
    }
    
    void runEndScreen() {
        clearScreen();
        
        if (state == State::VICTORY) {
            setColor(Color::BRIGHT_YELLOW);
            moveCursor(30, 10);
            std::cout << "╔════════════════════════════╗";
            moveCursor(30, 11);
            std::cout << "║   VICTORY! YOU ESCAPED!    ║";
            moveCursor(30, 12);
            std::cout << "╚════════════════════════════╝";
            resetColor();
            
            moveCursor(25, 14);
            std::cout << "You found the Amulet of Yendor and escaped the dungeon!";
        } else {
            setColor(Color::BRIGHT_RED);
            moveCursor(30, 10);
            std::cout << "╔════════════════════════════╗";
            moveCursor(30, 11);
            std::cout << "║        GAME OVER           ║";
            moveCursor(30, 12);
            std::cout << "╚════════════════════════════╝";
            resetColor();
            
            moveCursor(30, 14);
            std::cout << "You died on level " << dungeonLevel;
        }
        
        moveCursor(25, 20);
        std::cout << "Final Score: " << (player->stats.level * 100 + dungeonLevel * 50);
        
        moveCursor(30, 24);
        std::cout << "Press any key to exit...";
        
        while (getKeyPress() == 0) {
            sleepMs(50);
        }
    }
};

int main() {
    try {
        Game game;
        game.run();
        return 0;
    } catch (const std::exception& e) {
        restoreTerminal();
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}