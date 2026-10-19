#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chrono>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;

struct ButtonState {
    bool held = false;
    bool pendingRelease = false;
    Clock::time_point pressedAt{};
};

static ButtonState g_state[2][4];
static bool g_injecting = false;

static bool assistActive() {
    auto mod = Mod::get();
    if (!mod->getSettingValue<bool>("enabled")) return false;
    auto pl = PlayLayer::get();
    if (!pl) return false;
    if (mod->getSettingValue<bool>("practice-only") && !pl->m_isPracticeMode) return false;
    return true;
}

class $modify(AssistGameLayer, GJBaseGameLayer) {
    struct Fields {
        CCLabelBMFont* indicator = nullptr;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_injecting || button < 0 || button > 3) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }
        auto& s = g_state[isPlayer1 ? 0 : 1][button];

        if (down) {
            s.held = true;
            s.pressedAt = Clock::now();
            if (s.pendingRelease) {
                s.pendingRelease = false;
                GJBaseGameLayer::handleButton(false, button, isPlayer1);
            }
            return GJBaseGameLayer::handleButton(true, button, isPlayer1);
        }

        s.held = false;
        if (assistActive()) {
            auto window = std::chrono::milliseconds(
                Mod::get()->getSettingValue<int64_t>("window-ms"));
            if (Clock::now() - s.pressedAt < window) {
                s.pendingRelease = true;
                return;
            }
        }
        GJBaseGameLayer::handleButton(false, button, isPlayer1);
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);
        auto window = std::chrono::milliseconds(
            Mod::get()->getSettingValue<int64_t>("window-ms"));
        bool stretching = false;
        for (int p = 0; p < 2; p++) {
            for (int b = 0; b < 4; b++) {
                auto& s = g_state[p][b];
                if (!s.pendingRelease) continue;
                if (Clock::now() - s.pressedAt >= window || !assistActive()) {
                    s.pendingRelease = false;
                    g_injecting = true;
                    GJBaseGameLayer::handleButton(false, b, p == 0);
                    g_injecting = false;
                } else {
                    stretching = true;
                }
            }
        }
        updateIndicator(stretching);
    }

    void updateIndicator(bool stretching) {
        if (!PlayLayer::get() || !Mod::get()->getSettingValue<bool>("show-indicator")) {
            if (m_fields->indicator) m_fields->indicator->setVisible(false);
            return;
        }
        if (!m_fields->indicator) {
            auto win = CCDirector::get()->getWinSize();
            auto label = CCLabelBMFont::create("Assist", "bigFont.fnt");
            label->setScale(0.35f);
            label->setAnchorPoint({1.f, 1.f});
            label->setPosition({win.width - 5.f, win.height - 5.f});
            label->setZOrder(1000);
            this->addChild(label);
            m_fields->indicator = label;
        }
        auto label = m_fields->indicator;
        label->setVisible(assistActive());
        label->setColor(stretching ? ccColor3B{80, 255, 120} : ccColor3B{255, 255, 255});
        label->setOpacity(stretching ? 255 : 120);
    }
};

class $modify(PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        for (auto& p : g_state) for (auto& s : p) s = ButtonState{};
    }
    void onQuit() {
        for (auto& p : g_state) for (auto& s : p) s = ButtonState{};
        PlayLayer::onQuit();
    }
};
