#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include "../managers/SecondaryAudioEngine.hpp"

using namespace geode::prelude;
using namespace streamsongsync;

namespace {
    bool featureEnabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    // Reads where GD's own level song currently is and mirrors it onto the
    // secondary track. Call this AFTER PlayLayer has already done whatever
    // it was doing to its own music (i.e. after Base:: in each hook below),
    // so FMODAudioEngine::getMusicTimeMS has settled on the new position.
    void resyncSecondaryToGame() {
        if (!SecondaryAudioEngine::get()->isReady()) return;
        auto ms = FMODAudioEngine::sharedEngine()->getMusicTimeMS(0);
        SecondaryAudioEngine::get()->syncTo(ms, false);
    }
}

class $modify(StreamSongSyncPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        if (!featureEnabled()) {
            return true;
        }

        auto path = Mod::get()->getSettingValue<std::filesystem::path>("secondary-track");
        if (path.empty()) {
            log::warn("[StreamSongSync] enabled but no replacement track is set, skipping");
            return true;
        }

        auto driver = static_cast<int>(Mod::get()->getSettingValue<int64_t>("output-device-index"));
        auto* engine = SecondaryAudioEngine::get();

        // Only reload if the track or device actually changed, so re-entering
        // a level doesn't needlessly recreate the FMOD::System every time.
        bool ready = engine->matches(driver, path.string()) || engine->setup(driver, path.string());
        if (ready) {
            resyncSecondaryToGame();
        }

        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (featureEnabled()) {
            resyncSecondaryToGame();
        }
    }

    void loadFromCheckpoint(CheckpointObject* object) {
        PlayLayer::loadFromCheckpoint(object);
        if (featureEnabled() && Mod::get()->getSettingValue<bool>("resync-on-checkpoint")) {
            resyncSecondaryToGame();
        }
    }

    void onQuit() {
        PlayLayer::onQuit();
        SecondaryAudioEngine::get()->stop();
    }
};
