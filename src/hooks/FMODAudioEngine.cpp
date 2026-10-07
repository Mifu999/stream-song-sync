#include <Geode/modify/FMODAudioEngine.hpp>
#include "../managers/SecondaryAudioEngine.hpp"

using namespace geode::prelude;
using namespace streamsongsync;

// Piggybacks on GD's own audio tick so the secondary FMOD::System gets
// pumped every frame too, without a separate scheduler. update() is the
// single most frequently called FMODAudioEngine method, so this runs
// whether or not a level is even loaded (cheap no-op when the secondary
// engine hasn't been set up yet).
class $modify(StreamSongSyncFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        SecondaryAudioEngine::get()->tick();
    }
};
