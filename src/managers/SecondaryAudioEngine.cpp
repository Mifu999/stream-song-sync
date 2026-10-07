#include "SecondaryAudioEngine.hpp"
#include <Geode/loader/Log.hpp>

using namespace geode::prelude;

namespace streamsongsync {

namespace {
    // Thin wrapper so every FMOD call logs the same way on failure.
    bool check(FMOD_RESULT r, char const* what) {
        if (r != FMOD_OK) {
            log::error("[StreamSongSync] {} failed: {} ({})", what, FMOD_ErrorString(r), (int)r);
            return false;
        }
        return true;
    }
}

SecondaryAudioEngine* SecondaryAudioEngine::get() {
    static SecondaryAudioEngine instance;
    return &instance;
}

std::vector<AudioDeviceInfo> SecondaryAudioEngine::listOutputDevices() {
    std::vector<AudioDeviceInfo> devices;

    FMOD::System* scratch = nullptr;
    if (!check(FMOD::System_Create(&scratch), "System_Create (device list)")) {
        return devices;
    }

    int count = 0;
    if (check(scratch->getNumDrivers(&count), "getNumDrivers")) {
        for (int i = 0; i < count; i++) {
            char name[256] = {};
            FMOD_GUID guid{};
            int systemRate = 0;
            FMOD_SPEAKERMODE speakerMode = FMOD_SPEAKERMODE_DEFAULT;
            int speakerModeChannels = 0;

            auto r = scratch->getDriverInfo(i, name, sizeof(name), &guid, &systemRate,
                                             &speakerMode, &speakerModeChannels);
            if (r == FMOD_OK) {
                devices.push_back({ i, std::string(name) });
                log::info("[StreamSongSync] output device {}: {}", i, name);
            }
        }
    }

    // This scratch System was never init()'d, so no driver was actually
    // opened — release is enough, no close() needed.
    scratch->release();
    return devices;
}

bool SecondaryAudioEngine::setup(int driverIndex, std::string const& path) {
    teardown();

    if (!check(FMOD::System_Create(&m_system), "System_Create")) {
        return false;
    }
    if (!check(m_system->setDriver(driverIndex), "setDriver")) {
        teardown();
        return false;
    }
    // maxchannels=32 is plenty for a single looping/streaming track;
    // FMOD_INIT_NORMAL matches what FMODAudioEngine itself uses for music.
    if (!check(m_system->init(32, FMOD_INIT_NORMAL, nullptr), "init")) {
        teardown();
        return false;
    }

    FMOD_CREATESOUNDEXINFO exinfo{};
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    if (!check(m_system->createStream(path.c_str(), FMOD_DEFAULT, &exinfo, &m_sound), "createStream")) {
        teardown();
        return false;
    }

    m_driverIndex = driverIndex;
    m_loadedPath = path;
    log::info("[StreamSongSync] ready: driver {} <- {}", driverIndex, path);
    return true;
}

void SecondaryAudioEngine::play() {
    if (!m_system || !m_sound) return;
    // Start paused so there is no audible pop before the first syncTo().
    check(m_system->playSound(m_sound, nullptr, true, &m_channel), "playSound");
}

void SecondaryAudioEngine::stop() {
    if (m_channel) {
        m_channel->stop();
        m_channel = nullptr;
    }
}

void SecondaryAudioEngine::setPaused(bool paused) {
    if (m_channel) {
        check(m_channel->setPaused(paused), "setPaused");
    }
}

void SecondaryAudioEngine::syncTo(unsigned int positionMs, bool paused) {
    if (!m_channel) {
        play();
    }
    if (!m_channel) return;

    check(m_channel->setPosition(positionMs, FMOD_TIMEUNIT_MS), "setPosition");
    check(m_channel->setPaused(paused), "setPaused (sync)");
}

void SecondaryAudioEngine::tick() {
    if (m_system) {
        m_system->update();
    }
}

void SecondaryAudioEngine::teardown() {
    stop();
    if (m_sound) {
        m_sound->release();
        m_sound = nullptr;
    }
    if (m_system) {
        m_system->close();
        m_system->release();
        m_system = nullptr;
    }
    m_driverIndex = -1;
    m_loadedPath.clear();
}

}
