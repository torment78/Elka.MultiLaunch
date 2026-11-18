#pragma once
#include "Games.h"
#include <optional>

namespace elka {
class GameLauncher {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    GameLauncher(HWND owner, std::wstring directory, Log log);
    ~GameLauncher();
    bool Enabled() const;
    bool Enable(bool enabled);
    void Show();
    void Options(HWND owner);
    void Library(HWND owner);
    std::optional<Target> PickTarget(HWND owner);
};
}
