#include <Geode/modify/PlayLayer.hpp>
#include "managers/SecondaryAudioEngine.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    // The secondary FMOD::System used for playback is created lazily, the
    // first time a level with the feature enabled starts (see
    // src/hooks/PlayLayer.cpp). This throwaway enumeration just prints the
    // available output devices + indices to the Geode console at launch, so
    // "Output device index" in the settings can be filled in without
    // guessing.
    log::info("[StreamSongSync] enumerating output devices...");
    streamsongsync::SecondaryAudioEngine::get()->listOutputDevices();
}
