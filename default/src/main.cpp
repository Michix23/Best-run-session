#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>
#include <map>
#include <sstream>

using namespace geode::prelude;

struct Run {
    int percent;
    int attempt;
};

// level key -> best runs of this launch (resets when GD closes)
static std::map<std::string, std::vector<Run>> s_runs;

static std::string levelKey(GJGameLevel* lvl) {
    // id + name so local/editor levels (id 0) don't all share one list
    return std::to_string(lvl->m_levelID.value()) + ":" + std::string(lvl->m_levelName);
}

static bool shouldIgnore(PlayLayer* pl) {
    return pl->m_isPracticeMode || pl->m_isTestMode || pl->m_startPosObject;
}

static void recordRun(PlayLayer* pl, int percent) {
    if (percent <= 0) return;

    auto& runs = s_runs[levelKey(pl->m_level)];
    int attempt = pl->m_attempts;

    // skip if this attempt was already recorded (in case destroyPlayer fires twice)
    for (auto& r : runs) if (r.attempt == attempt) return;

    int oldBest = runs.empty() ? 0 : runs.front().percent;

    runs.push_back({ percent, attempt });
    std::sort(runs.begin(), runs.end(),
        [](Run& a, Run& b) { return a.percent > b.percent; });
    if (runs.size() > 10) runs.resize(10);

    if (percent > oldBest) {
        Notification::create("New best: " + std::to_string(percent) + "%")->show();
    }
}

class $modify(RunSaverPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        if (player != m_player1 || shouldIgnore(this)) return;
        recordRun(this, static_cast<int>(this->getCurrentPercent()));
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (shouldIgnore(this)) return;
        recordRun(this, 100);
    }
};

class $modify(RunSaverPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto spr = ButtonSprite::create("Runs");
        spr->setScale(0.7f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(RunSaverPause::onRuns));

        if (auto menu = this->getChildByID("right-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto menu = CCMenu::create();
            menu->addChild(btn);
            auto win = CCDirector::sharedDirector()->getWinSize();
            menu->setPosition({ win.width - 50.f, 40.f });
            this->addChild(menu);
        }
    }

    void onRuns(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl) return;

        std::string title = std::string(pl->m_level->m_levelName) + " - session runs";
        std::stringstream ss;

        auto it = s_runs.find(levelKey(pl->m_level));
        if (it == s_runs.end() || it->second.empty()) {
            ss << "No runs saved for this level yet.";
        } else {
            int i = 1;
            for (auto& r : it->second) {
                ss << i++ << ". **" << r.percent << "%** (attempt " << r.attempt << ")\n\n";
            }
        }
        MDPopup::create(title.c_str(), ss.str(), "OK")->show();
    }
};