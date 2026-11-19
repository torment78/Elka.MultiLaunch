#pragma once
#include "Platform.h"
#include <array>
#include <filesystem>

namespace elka::games {
enum class Store { Steam, EA, BattleNet, Other };
enum class Direction { Down, Up, Left, Right };
constexpr size_t StoreCount = 4;
struct Game {
    Store store = Store::Steam;
    std::wstring id, name, folder;
    Target target;
    std::wstring icon;
    int iconIndex = 0;
    std::wstring Key() const;
};
struct Library {
    std::vector<Game> games;
    std::array<std::wstring, StoreCount> clients;
    std::vector<std::wstring> notes;
};
struct State {
    bool enabled = false, topmost = false, locked = false;
    Direction direction = Direction::Down;
    std::array<Direction, StoreCount> groupDirections{};
    std::wstring screen;
    int x = -1, y = -1;
    std::set<std::wstring> favorites;
    std::vector<Game> custom;
    bool toolbarVisible = true;
    std::array<bool, StoreCount> drawerToolbarVisible{true,true,true,true};
    std::array<COLORREF, StoreCount> colors{RGB(100,181,246),RGB(255,144,102),RGB(103,152,255),RGB(77,218,191)};
};
const wchar_t* StoreName(Store store);
const wchar_t* DirectionName(Direction direction);
bool Allowed(Direction primary, Direction secondary);
struct Placement { RECT bounds{}; int columns = 1, rows = 1, capacity = 1; std::vector<POINT> cells; };
Placement LayoutDrawer(RECT widget, RECT tile, RECT work, int count, Direction primary, Direction secondary, UINT dpi, bool controls = true);
std::wstring ColorText(COLORREF color);
bool ParseColor(const std::wstring& text, COLORREF& color);
bool ParseIconLocation(const std::wstring& text, std::wstring& path, int& index);
std::wstring FindGameIcon(const Game& game);
Library Discover();
void DiscoverSteam(const std::filesystem::path& root, Library& library);
std::vector<std::wstring> BattleBuildProducts(const std::string& text);
void DiscoverBattleFolder(const std::filesystem::path& folder, Library& library);
std::vector<std::wstring> EaContentIds(const std::string& xml);
struct BattleInstall { std::wstring id, product, path; };
std::vector<BattleInstall> ReadBattleDatabase(const std::string& data);
std::string Encode(const State& state);
bool Decode(const std::string& text, State& state, std::wstring& error);
bool Load(const std::wstring& directory, State& state, std::wstring& error);
bool Save(const std::wstring& directory, const State& state, std::wstring& error);
}
