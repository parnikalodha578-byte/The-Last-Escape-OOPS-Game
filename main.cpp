#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

float Vector2Distance(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

constexpr int SCREEN_W = 960;
constexpr int SCREEN_H = 600;
constexpr int TILE = 32;
constexpr int MAP_W = 25;
constexpr int MAP_H = 15;
constexpr float PLAYER_SPEED = 155.0f;

enum class Screen { Title, Playing, Inventory, Help, Records, Ending };
enum class Ending { None, Escaped, Caught, Secret };

struct Player {
    Vector2 pos{80, 80};
    float w = 20, h = 24;
    int health = 100;
    int energy = 100;
    int keys = 0;
    int clues = 0;
    int batteries = 0;
    bool hasFlashlight = false;
    bool flashlightOn = false;
    bool hasKeycard = false;
    bool hasSecret = false;
    int score = 0;
};

struct Enemy {
    Vector2 pos{};
    float speed = 48.0f;
    bool active = true;
};

struct Item {
    Vector2 pos{};
    std::string type;
    bool taken = false;
    int room = 0;
};

struct Room {
    std::string name;
    std::vector<std::string> map;
};

static Color ink = {7, 12, 24, 255};
static Color panel = {13, 24, 42, 255};
static Color cyan = {65, 225, 255, 255};
static Color blue = {27, 92, 135, 255};
static Color red = {255, 70, 90, 255};
static Color yellow = {255, 216, 91, 255};
static Color green = {89, 255, 151, 255};
static Color muted = {147, 178, 200, 255};

// Small helper because Vector2Distance is not exposed by every Raylib build.
float Vector2Distance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

std::vector<Room> rooms;
std::vector<Item> items;
std::vector<Enemy> enemies;
Player player;
Screen screen = Screen::Title;
Ending ending = Ending::None;
int currentRoom = 0;
float gameTime = 0;
float messageTimer = 0;
std::string message = "Find clues, collect a keycard, and escape.";
int bestScore = 0;
int endingsFound = 0;
bool gameWon = false;

void SetMessage(const std::string& text, float seconds = 3.0f) {
    message = text;
    messageTimer = seconds;
}

std::vector<std::string> MakeMap(int roomId) {
    std::vector<std::string> m(MAP_H, std::string(MAP_W, '.'));
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) {
                // Leave a doorway in the left/right border so rooms can be traversed.
                bool sideDoor = (y == 7 && (x == 0 || x == MAP_W - 1));
                if (!sideDoor) m[y][x] = '#';
            }
        }
    }
    // Internal walls create room-like corridors, with doorways between sections.
    if (roomId == 0) {
        for (int y = 2; y < 12; ++y) if (y != 7) m[y][8] = '#';
        for (int x = 8; x < 22; ++x) if (x != 15) m[5][x] = '#';
        for (int y = 5; y < 13; ++y) if (y != 9) m[y][17] = '#';
        m[7][8] = '.'; m[5][15] = '.'; m[9][17] = '.';
    } else if (roomId == 1) {
        for (int x = 3; x < 22; ++x) if (x != 12) m[7][x] = '#';
        for (int y = 2; y < 7; ++y) if (y != 4) m[y][6] = '#';
        for (int y = 8; y < 13; ++y) if (y != 10) m[y][18] = '#';
    } else if (roomId == 2) {
        for (int y = 2; y < 12; ++y) if (y != 4 && y != 10) m[y][12] = '#';
        for (int x = 3; x < 10; ++x) if (x != 6) m[4][x] = '#';
        for (int x = 14; x < 22; ++x) if (x != 19) m[10][x] = '#';
    } else {
        for (int x = 3; x < 22; ++x) if (x != 12) m[4][x] = '#';
        for (int x = 3; x < 22; ++x) if (x != 7 && x != 18) m[10][x] = '#';
        for (int y = 5; y < 10; ++y) if (y != 7) m[y][7] = '#';
        for (int y = 5; y < 10; ++y) if (y != 8) m[y][18] = '#';
    }
    return m;
}

void BuildWorld() {
    rooms.clear(); items.clear(); enemies.clear();
    rooms.push_back({"LABORATORY", MakeMap(0)});
    rooms.push_back({"RESEARCH WING", MakeMap(1)});
    rooms.push_back({"SECURITY ARCHIVE", MakeMap(2)});
    rooms.push_back({"EXIT SECTOR", MakeMap(3)});

    // Items belong to a specific room, so exploration has a purpose.
    items.push_back({{5*TILE + 16.0f, 3*TILE + 16.0f}, "FLASHLIGHT", false, 0});
    items.push_back({{13*TILE + 16.0f, 3*TILE + 16.0f}, "CLUE", false, 0});
    items.push_back({{20*TILE + 16.0f, 8*TILE + 16.0f}, "KEYCARD", false, 1});
    items.push_back({{4*TILE + 16.0f, 11*TILE + 16.0f}, "CLUE", false, 1});
    items.push_back({{15*TILE + 16.0f, 12*TILE + 16.0f}, "BATTERY", false, 2});
    items.push_back({{10*TILE + 16.0f, 2*TILE + 16.0f}, "CLUE", false, 2});
    items.push_back({{21*TILE + 16.0f, 12*TILE + 16.0f}, "SECRET", false, 3});

    enemies.push_back({{20*TILE + 16.0f, 3*TILE + 16.0f}, 42.0f, true});
    enemies.push_back({{4*TILE + 16.0f, 10*TILE + 16.0f}, 35.0f, true});
}

bool IsWall(int roomId, Vector2 p) {
    const auto& map = rooms[roomId].map;
    int tx = (int)(p.x / TILE), ty = (int)(p.y / TILE);
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return true;
    return map[ty][tx] == '#';
}

bool CollidesAt(Vector2 pos) {
    Vector2 points[4] = {
        {pos.x - player.w/2, pos.y - player.h/2},
        {pos.x + player.w/2, pos.y - player.h/2},
        {pos.x - player.w/2, pos.y + player.h/2},
        {pos.x + player.w/2, pos.y + player.h/2}
    };
    for (auto p : points) if (IsWall(currentRoom, p)) return true;
    return false;
}

void StartGame() {
    player = Player{};
    player.pos = {2.5f*TILE, 2.5f*TILE};
    currentRoom = 0;
    gameTime = 0;
    ending = Ending::None;
    gameWon = false;
    BuildWorld();
    screen = Screen::Playing;
    SetMessage("You wake in the laboratory. Find 3 clues and the keycard.");
}

void SaveGame() {
    std::ofstream f("last_escape_save.txt");
    if (!f) { SetMessage("Could not write save file."); return; }
    f << currentRoom << '\n' << player.pos.x << '\n' << player.pos.y << '\n'
      << player.health << '\n' << player.energy << '\n' << player.keys << '\n'
      << player.clues << '\n' << player.batteries << '\n' << player.hasFlashlight << '\n'
      << player.hasKeycard << '\n' << player.hasSecret << '\n' << player.score << '\n'
      << gameTime << '\n';
    for (const auto& item : items) f << item.taken << ' ';
    f << '\n';
    for (const auto& enemy : enemies) f << enemy.active << ' ' << enemy.pos.x << ' ' << enemy.pos.y << '\n';
    SetMessage("Game saved to last_escape_save.txt");
}

bool LoadGame() {
    std::ifstream f("last_escape_save.txt");
    if (!f) return false;
    BuildWorld();
    f >> currentRoom >> player.pos.x >> player.pos.y >> player.health >> player.energy
      >> player.keys >> player.clues >> player.batteries >> player.hasFlashlight
      >> player.hasKeycard >> player.hasSecret >> player.score >> gameTime;
    for (auto& item : items) f >> item.taken;
    for (auto& enemy : enemies) f >> enemy.active >> enemy.pos.x >> enemy.pos.y;
    if (!f || currentRoom < 0 || currentRoom >= (int)rooms.size()) return false;
    screen = Screen::Playing;
    ending = Ending::None;
    SetMessage("Saved game loaded.");
    return true;
}

void SaveRecord() {
    std::ifstream in("last_escape_records.txt");
    int oldScore = 0, oldEndings = 0;
    if (in) in >> oldScore >> oldEndings;
    bestScore = std::max(oldScore, player.score);
    endingsFound = std::min(3, std::max(oldEndings, endingsFound));
    std::ofstream out("last_escape_records.txt");
    out << bestScore << ' ' << endingsFound;
}

void Finish(Ending e) {
    ending = e;
    screen = Screen::Ending;
    if (e == Ending::Escaped) {
        player.score += std::max(0, 500 - (int)gameTime) + player.clues * 100;
        endingsFound = std::min(3, endingsFound + 1);
    } else if (e == Ending::Secret) {
        player.score += 1000 + player.clues * 100;
        endingsFound = std::min(3, endingsFound + 1);
    }
    SaveRecord();
}

void DrawTextCentered(const char* text, int y, int size, Color color) {
    DrawText(text, SCREEN_W/2 - MeasureText(text, size)/2, y, size, color);
}

void DrawPixelCharacter(Vector2 p, Color mainColor, bool isEnemy) {
    int x = (int)p.x, y = (int)p.y;
    DrawRectangle(x - 8, y - 10, 16, 17, mainColor);
    DrawRectangle(x - 6, y - 18, 12, 10, isEnemy ? red : (Color){218, 190, 154, 255});
    DrawRectangle(x - 5, y - 8, 4, 3, isEnemy ? BLACK : cyan);
    DrawRectangle(x + 1, y - 8, 4, 3, isEnemy ? BLACK : cyan);
    DrawRectangle(x - 7, y + 7, 5, 8, DARKGRAY);
    DrawRectangle(x + 2, y + 7, 5, 8, DARKGRAY);
}

void DrawRoom() {
    const auto& map = rooms[currentRoom].map;
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            Rectangle r{(float)x*TILE, (float)y*TILE, (float)TILE, (float)TILE};
            if (map[y][x] == '#') {
                DrawRectangleRec(r, (Color){22, 39, 58, 255});
                DrawRectangle((int)r.x, (int)r.y, TILE, 3, (Color){39, 72, 96, 255});
                DrawRectangle((int)r.x + 3, (int)r.y + 5, 2, TILE - 8, (Color){12, 22, 35, 255});
            } else {
                DrawRectangleRec(r, ((x + y) % 2 == 0) ? (Color){12, 22, 35, 255} : (Color){14, 25, 39, 255});
                DrawRectangle((int)r.x, (int)r.y + TILE - 2, TILE, 2, (Color){18, 43, 59, 255});
            }
        }
    }
    // Room-specific exit/door markings.
    if (currentRoom == 3) {
        DrawRectangle(22*TILE + 4, 7*TILE + 4, 24, 24, green);
        DrawText("EXIT", 22*TILE + 2, 7*TILE - 10, 10, WHITE);
    }

    for (const auto& item : items) {
        if (item.taken || item.room != currentRoom) continue;
        // Items are distributed through the world; all can be discovered in any room for this compact prototype.
        Color c = yellow;
        if (item.type == "FLASHLIGHT") c = cyan;
        if (item.type == "CLUE") c = green;
        if (item.type == "KEYCARD") c = (Color){255, 125, 220, 255};
        if (item.type == "BATTERY") c = yellow;
        if (item.type == "SECRET") c = red;
        DrawRectangle((int)item.pos.x - 6, (int)item.pos.y - 6, 12, 12, c);
        DrawRectangle((int)item.pos.x - 3, (int)item.pos.y - 3, 6, 6, WHITE);
    }
    for (const auto& enemy : enemies) if (enemy.active) DrawPixelCharacter(enemy.pos, (Color){107, 24, 46, 255}, true);
    DrawPixelCharacter(player.pos, (Color){38, 130, 184, 255}, false);
    for (const auto& item : items) {
        if (!item.taken && item.room == currentRoom && Vector2Distance(player.pos, item.pos) < 42) {
            DrawRectangle((int)player.pos.x - 34, (int)player.pos.y - 40, 68, 18, (Color){3, 8, 16, 230});
            DrawText("PRESS E", (int)player.pos.x - 28, (int)player.pos.y - 37, 12, yellow);
        }
    }

    // A pixelated flashlight vignette; it is a simple gameplay visual rather than a real light mask.
    if (player.flashlightOn) {
        DrawCircleLines((int)player.pos.x, (int)player.pos.y, 105, (Color){62, 177, 208, 150});
        DrawCircleLines((int)player.pos.x, (int)player.pos.y, 115, (Color){62, 177, 208, 75});
        DrawCircleLines((int)player.pos.x, (int)player.pos.y, 125, (Color){62, 177, 208, 35});
    }
}

void DrawHUD() {
    DrawRectangle(0, 480, SCREEN_W, 120, (Color){5, 10, 21, 245});
    DrawLine(0, 480, SCREEN_W, 480, cyan);
    DrawText(rooms[currentRoom].name.c_str(), 20, 492, 20, cyan);
    DrawText("OBJECTIVE: find 3 clues + keycard, then reach the exit", 360, 496, 15, muted);
    DrawText(TextFormat("HEALTH %d", player.health), 20, 525, 17, red);
    DrawRectangle(110, 528, 150, 12, DARKGRAY);
    DrawRectangle(110, 528, (int)(150 * player.health / 100.0f), 12, red);
    DrawText(TextFormat("ENERGY %d", player.energy), 280, 525, 17, yellow);
    DrawRectangle(375, 528, 120, 12, DARKGRAY);
    DrawRectangle(375, 528, (int)(120 * player.energy / 100.0f), 12, yellow);
    DrawText(TextFormat("CLUES %d/3", player.clues), 520, 525, 17, green);
    DrawText(TextFormat("TIME %02d:%02d", (int)gameTime/60, (int)gameTime%60), 690, 525, 17, WHITE);
    DrawText("MOVE: WASD/ARROWS   E: PICK UP / USE   F: FLASHLIGHT   SHIFT: SPRINT   I: BAG   P: SAVE", 20, 563, 14, muted);
    if (messageTimer > 0) {
        DrawRectangle(18, 445, 924, 28, (Color){3, 8, 16, 235});
        DrawText(message.c_str(), 28, 451, 16, yellow);
    }
}

void Interact() {
    // Collect items when close enough.
    bool foundNearby = false;
    for (auto& item : items) {
        if (item.taken || item.room != currentRoom) continue;
        if (Vector2Distance(player.pos, item.pos) < 34) {
            foundNearby = true;
            item.taken = true;
            if (item.type == "FLASHLIGHT") { player.hasFlashlight = true; player.flashlightOn = true; SetMessage("FLASHLIGHT FOUND! Press F to toggle it."); }
            else if (item.type == "CLUE") { player.clues++; player.score += 100; SetMessage("Evidence collected! The facility was hiding something."); }
            else if (item.type == "KEYCARD") { player.hasKeycard = true; player.keys++; player.score += 150; SetMessage("Security keycard acquired."); }
            else if (item.type == "BATTERY") { player.batteries++; player.energy = std::min(100, player.energy + 35); SetMessage("Battery collected. Energy restored."); }
            else if (item.type == "SECRET") { player.hasSecret = true; player.score += 250; SetMessage("Secret evidence found. This could change everything."); }
        }
    }
    if (!foundNearby) {
        SetMessage("Nothing close enough to use. Look for glowing items and press E nearby.", 2.0f);
    }
    // Exit gate.
    Vector2 exitPos{22*TILE + 16.0f, 7*TILE + 16.0f};
    if (currentRoom == 3 && Vector2Distance(player.pos, exitPos) < 45) {
        if (player.hasSecret && player.clues >= 3) Finish(Ending::Secret);
        else if (player.hasKeycard && player.clues >= 3) Finish(Ending::Escaped);
        else if (!player.hasKeycard) SetMessage("The exit is locked. Find the security keycard.");
        else SetMessage("You need all 3 clues to understand what happened.");
    }

}

void UpdateGame(float dt) {
    gameTime += dt;
    if (messageTimer > 0) messageTimer -= dt;
    if (IsKeyPressed(KEY_I)) { screen = Screen::Inventory; return; }
    if (IsKeyPressed(KEY_P)) { SaveGame(); }
    if (IsKeyPressed(KEY_F)) {
        if (player.hasFlashlight) { player.flashlightOn = !player.flashlightOn; SetMessage(player.flashlightOn ? "Flashlight ON." : "Flashlight OFF."); }
        else SetMessage("Find the flashlight first.");
    }
    if (IsKeyPressed(KEY_ESCAPE)) { screen = Screen::Title; return; }

    Vector2 move{0, 0};
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) move.x -= 1;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) move.x += 1;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) move.y -= 1;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) move.y += 1;
    if (move.x != 0 || move.y != 0) {
        float len = sqrtf(move.x*move.x + move.y*move.y);
        move.x /= len; move.y /= len;
        float speed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? PLAYER_SPEED * 1.65f : PLAYER_SPEED;
        Vector2 next = {player.pos.x + move.x * speed * dt, player.pos.y + move.y * speed * dt};
        if (!CollidesAt({next.x, player.pos.y})) player.pos.x = next.x;
        if (!CollidesAt({player.pos.x, next.y})) player.pos.y = next.y;
        if ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && player.energy > 0)
            player.energy = std::max(0, player.energy - (int)(dt * 12.0f));
        else if (player.energy < 100)
            player.energy = std::min(100, player.energy + (int)(dt * 3.0f));
    }
    if (IsKeyPressed(KEY_E)) Interact();

    // Walk through side doorways to travel between rooms; the security wing needs a keycard.
    if (player.pos.x > (MAP_W - 1)*TILE && currentRoom < 3) {
        if (currentRoom == 1 && !player.hasKeycard) {
            player.pos.x = (MAP_W - 2)*TILE;
            SetMessage("SECURITY LOCK: find the keycard before entering the archive.");
        } else {
            currentRoom++;
            player.pos.x = 1.5f*TILE;
            player.pos.y = 7.5f*TILE;
            SetMessage("Entered " + rooms[currentRoom].name + ". Search carefully.");
        }
    } else if (player.pos.x < TILE && currentRoom > 0) {
        currentRoom--;
        player.pos.x = (MAP_W - 1.5f)*TILE;
        player.pos.y = 7.5f*TILE;
        SetMessage("Returned to " + rooms[currentRoom].name + ".");
    }
    if (currentRoom == 3 && player.pos.x > (MAP_W - 1)*TILE) player.pos.x = (MAP_W - 1)*TILE;

    for (int i = 0; i < (int)enemies.size(); ++i) {
        // Each enemy belongs to its own room and becomes active again when you return.
        enemies[i].active = (currentRoom == i);
    }
    for (auto& enemy : enemies) {
        if (!enemy.active) continue;
        float dist = Vector2Distance(enemy.pos, player.pos);
        if (dist < 230 && dist > 1) {
            Vector2 d{(player.pos.x - enemy.pos.x)/dist, (player.pos.y - enemy.pos.y)/dist};
            Vector2 next{enemy.pos.x + d.x*enemy.speed*dt, enemy.pos.y + d.y*enemy.speed*dt};
            if (!IsWall(currentRoom, next)) enemy.pos = next;
        }
        if (dist < 22) {
            player.health -= (int)(20 * dt);
            if (player.health <= 0) { player.health = 0; Finish(Ending::Caught); return; }
            if (messageTimer <= 0) SetMessage("An entity caught your scent! Keep moving.", 1.2f);
        }
    }
    if (gameTime > 600) { SetMessage("Time is running out!", 1.0f); }
    if (gameTime > 720) { Finish(Ending::Caught); }
}

void DrawTitle() {
    ClearBackground(ink);
    for (int i = 0; i < 24; ++i) {
        int x = (i*79 + 23) % SCREEN_W, y = (i*47 + 15) % SCREEN_H;
        DrawRectangle(x, y, 2, 2, (i%3==0) ? cyan : blue);
    }
    DrawRectangle(75, 65, 810, 465, panel);
    DrawRectangleLines(75, 65, 810, 465, cyan);
    DrawTextCentered("THE LAST ESCAPE", 105, 46, WHITE);
    DrawTextCentered("A PIXEL-ART MYSTERY SURVIVAL GAME", 163, 19, cyan);
    DrawTextCentered("You woke up somewhere you were never meant to be.", 218, 18, muted);
    DrawTextCentered("Find the evidence. Avoid what hunts you. Get out.", 247, 18, muted);
    const char* options[] = {"[1] NEW GAME", "[2] LOAD GAME", "[3] HOW TO PLAY", "[4] MY RECORDS", "[5] QUIT"};
    for (int i = 0; i < 5; ++i) {
        Color c = (i == 0) ? yellow : WHITE;
        DrawTextCentered(options[i], 310 + i*34, 21, c);
    }
    DrawTextCentered("MOVE: WASD/ARROWS   E: INTERACT   F: FLASHLIGHT   SHIFT: SPRINT", 490, 15, green);
}

void DrawInventory() {
    ClearBackground(ink);
    DrawRectangle(100, 55, 760, 490, panel);
    DrawRectangleLines(100, 55, 760, 490, cyan);
    DrawTextCentered("INVENTORY", 80, 32, cyan);
    DrawText(TextFormat("Health: %d / 100", player.health), 150, 155, 22, red);
    DrawText(TextFormat("Energy: %d / 100", player.energy), 150, 195, 22, yellow);
    DrawText(TextFormat("Evidence clues: %d / 3", player.clues), 150, 235, 22, green);
    DrawText(TextFormat("Keycard: %s", player.hasKeycard ? "YES" : "NO"), 150, 275, 22, WHITE);
    DrawText(TextFormat("Flashlight: %s", player.hasFlashlight ? (player.flashlightOn ? "ON" : "OFF") : "NO"), 150, 315, 22, WHITE);
    DrawText(TextFormat("Batteries: %d", player.batteries), 150, 355, 22, WHITE);
    DrawText(TextFormat("Secret evidence: %s", player.hasSecret ? "FOUND" : "NOT FOUND"), 150, 395, 22, player.hasSecret ? red : muted);
    DrawTextCentered("Press I or ESC to return", 485, 18, muted);
}

void DrawHelp() {
    ClearBackground(ink);
    DrawRectangle(90, 45, 780, 510, panel);
    DrawRectangleLines(90, 45, 780, 510, cyan);
    DrawTextCentered("HOW TO PLAY", 72, 32, cyan);
    const char* lines[] = {
        "MISSION: Escape the research facility and uncover its secret.",
        "",
        "W / A / S / D or ARROW KEYS  - Move around",
        "E                          - Collect nearby items / use exit",
        "I                          - Open inventory",
        "F                          - Toggle flashlight after finding it",
        "SHIFT                      - Sprint (uses energy)",
        "P                          - Save your progress",
        "ESC                        - Return to title menu",
        "",
        "Collect 3 clues and a security keycard to unlock the normal ending.",
        "Find the hidden evidence too to unlock the secret ending.",
        "Avoid the red entities. If your health reaches zero, you are caught.",
        "You have 12 minutes before the facility locks down.",
        "",
        "TIP: Explore every room. Bright objects can be collected."
    };
    for (int i = 0; i < 16; ++i) DrawText(lines[i], 135, 125 + i*25, 15, (i==0) ? yellow : WHITE);
    DrawTextCentered("Press ESC to return", 510, 17, muted);
}

void DrawRecords() {
    std::ifstream f("last_escape_records.txt");
    if (f) f >> bestScore >> endingsFound;
    ClearBackground(ink);
    DrawRectangle(150, 100, 660, 390, panel);
    DrawRectangleLines(150, 100, 660, 390, cyan);
    DrawTextCentered("MY RECORDS", 130, 34, cyan);
    DrawText(TextFormat("Best score: %d", bestScore), 220, 230, 25, yellow);
    DrawText(TextFormat("Endings discovered: %d / 3", endingsFound, 3), 220, 280, 25, green);
    DrawText("Records are stored in last_escape_records.txt", 220, 340, 17, muted);
    DrawTextCentered("Press ESC to return", 435, 18, WHITE);
}

void DrawEnding() {
    ClearBackground(ink);
    Color c = ending == Ending::Caught ? red : (ending == Ending::Secret ? green : cyan);
    DrawRectangle(100, 80, 760, 430, panel);
    DrawRectangleLines(100, 80, 760, 430, c);
    if (ending == Ending::Escaped) {
        DrawTextCentered("ESCAPE SUCCESSFUL", 130, 34, green);
        DrawTextCentered("You made it outside with the evidence.", 215, 22, WHITE);
        DrawTextCentered("But someone knew you were coming.", 255, 20, muted);
    } else if (ending == Ending::Secret) {
        DrawTextCentered("THE SECRET ENDING", 130, 34, green);
        DrawTextCentered("The evidence reveals the truth:", 210, 22, WHITE);
        DrawTextCentered("the facility was studying the people who entered it.", 250, 19, yellow);
        DrawTextCentered("You escaped with proof. The story is not over.", 290, 19, muted);
    } else {
        DrawTextCentered("YOU WERE CAUGHT", 130, 34, red);
        DrawTextCentered("The lights go out. Something is standing behind you.", 220, 20, WHITE);
        DrawTextCentered("Try again, and do not stop moving.", 265, 20, muted);
    }
    DrawTextCentered(TextFormat("Final score: %d", player.score), 350, 23, yellow);
    DrawTextCentered("Press ENTER to return to the title screen", 435, 18, cyan);
}

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "The Last Escape - Mystery Survival");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    StartGame();
    screen = Screen::Title;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (screen == Screen::Title) {
            if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) StartGame();
            if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
                if (!LoadGame()) SetMessage("No save file found yet.");
            }
            if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) screen = Screen::Help;
            if (IsKeyPressed(KEY_FOUR) || IsKeyPressed(KEY_KP_4)) screen = Screen::Records;
            if (IsKeyPressed(KEY_FIVE) || IsKeyPressed(KEY_KP_5)) break;
        } else if (screen == Screen::Playing) {
            UpdateGame(dt);
        } else if (screen == Screen::Inventory) {
            if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ESCAPE)) screen = Screen::Playing;
        } else if (screen == Screen::Help || screen == Screen::Records) {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) screen = Screen::Title;
        } else if (screen == Screen::Ending) {
            if (IsKeyPressed(KEY_ENTER)) screen = Screen::Title;
        }

        BeginDrawing();
        ClearBackground(ink);
        if (screen == Screen::Title) DrawTitle();
        else if (screen == Screen::Playing) { DrawRoom(); DrawHUD(); }
        else if (screen == Screen::Inventory) DrawInventory();
        else if (screen == Screen::Help) DrawHelp();
        else if (screen == Screen::Records) DrawRecords();
        else if (screen == Screen::Ending) DrawEnding();
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
