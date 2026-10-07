#pragma once

#include <fmod.hpp>
#include <string>
#include <vector>

namespace streamsongsync {

struct AudioDeviceInfo {
    int index;
    std::string name;
};

// Owns a SECOND, independent FMOD::System — separate from GD's own
// FMODAudioEngine::sharedEngine() — bound to whatever output driver the user
// picks (normally a virtual audio cable). This lets the replacement track
// play on a device the player never hears, while OBS captures that device
// as its audio source instead of the game's real sound.
//
// Not compiled or run anywhere yet. Verify every FMOD call against a real
// build before trusting it on stream.
class SecondaryAudioEngine {
public:
    static SecondaryAudioEngine* get();

    // Lists the FMOD output drivers visible to this process (index + name),
    // using a throwaway FMOD::System so it never touches GD's own audio.
    // Safe to call at any time.
    std::vector<AudioDeviceInfo> listOutputDevices();

    // (Re)creates the secondary System bound to `driverIndex` and loads
    // `path` as a stream on it. Tears down any previous instance first.
    // Returns false (and logs the FMOD_RESULT) on failure.
    bool setup(int driverIndex, std::string const& path);

    // Starts playback, paused, on the secondary channel. Call syncTo()
    // right after to position it before unpausing.
    void play();

    void stop();
    void setPaused(bool paused);

    // Seeks the secondary track to `positionMs` and matches `paused`.
    // This is the single entry point the PlayLayer hooks call — on level
    // start, on every resetLevel(), and optionally on checkpoint restore.
    void syncTo(unsigned int positionMs, bool paused);

    // Must be pumped every frame for streaming/buffering to work —
    // piggybacked on the FMODAudioEngine::update hook so it rides GD's own
    // audio tick instead of needing a separate scheduler.
    void tick();

    void teardown();

    bool isReady() const { return m_channel != nullptr; }

    // What setup() last succeeded with, so callers can skip a redundant
    // teardown/reload when the level restarts with the same settings.
    bool matches(int driverIndex, std::string const& path) const {
        return m_system && m_driverIndex == driverIndex && m_loadedPath == path;
    }

private:
    FMOD::System*  m_system = nullptr;
    FMOD::Sound*   m_sound  = nullptr;
    FMOD::Channel* m_channel = nullptr;
    int            m_driverIndex = -1;
    std::string    m_loadedPath;
};

}
