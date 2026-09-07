#include <Geode/Geode.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <fmod.hpp>
#include "MusicLibrary.hpp"
#include "MusicPickerPopup.hpp"

using namespace geode::prelude;
using namespace CustomBGMusic;

// Tracks whether the currently-playing channel is OUR custom stream, so we
// don't accidentally treat other mods' music channels as ours.
static bool s_playingCustomTrack = false;

// Actually starts playback of an external mp3 through FMOD, mirroring what
// FMODAudioEngine::playMusic does internally (create stream -> play on the
// background music channel group -> loop).
static bool playCustomTrack(FMODAudioEngine* engine, std::filesystem::path const& path, bool loop) {
    auto system = engine->m_system;
    if (!system) return false;

    // Stop whatever is currently on the background music channel first,
    // so we don't end up with overlapping tracks if this is called twice.
    if (engine->m_backgroundMusicChannel) {
        engine->m_backgroundMusicChannel->stop();
        engine->m_backgroundMusicChannel = nullptr;
    }

    FMOD::Sound* sound = nullptr;
    auto mode = FMOD_DEFAULT | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_2D;

    auto pathStr = path.string();
    auto result = system->createStream(pathStr.c_str(), mode, nullptr, &sound);
    if (result != FMOD_OK || !sound) {
        log::error("CustomBGMusic: failed to load '{}' (fmod error {})", pathStr, static_cast<int>(result));
        return false;
    }

    FMOD::Channel* channel = nullptr;
    result = system->playSound(sound, nullptr, false, &channel);
    if (result != FMOD_OK || !channel) {
        log::error("CustomBGMusic: failed to play '{}' (fmod error {})", pathStr, static_cast<int>(result));
        return false;
    }

    channel->setVolume(engine->m_backgroundMusicVolume);
    engine->m_backgroundMusicChannel = channel;
    s_playingCustomTrack = true;
    return true;
}

namespace CustomBGMusic {
    // Called right after the user selects/imports a track so it starts
    // playing immediately, without needing to leave and re-enter a layer.
    void applySelectedTrackImmediately() {
        if (!Mod::get()->getSettingValue<bool>("enabled")) return;

        auto selected = MusicLibrary::get().getSelectedTrackPath();
        if (!selected.has_value()) return;

        auto engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        playCustomTrack(engine, *selected, true);
    }
}

class $modify(CustomFMODAudioEngine, FMODAudioEngine) {
    void playMusic(gd::string filename, bool loop, int idk) {
        // Respect the toggle in mod settings; if disabled, defer entirely
        // to the base game / other mods hooking this same function.
        if (!Mod::get()->getSettingValue<bool>("enabled")) {
            s_playingCustomTrack = false;
            FMODAudioEngine::playMusic(filename, loop, idk);
            return;
        }

        auto selected = MusicLibrary::get().getSelectedTrackPath();
        if (!selected.has_value()) {
            // No custom track chosen yet -- fall back to normal behavior,
            // letting other music mods (or the base game) handle it.
            s_playingCustomTrack = false;
            FMODAudioEngine::playMusic(filename, loop, idk);
            return;
        }

        if (!playCustomTrack(this, *selected, loop)) {
            // If loading the custom file failed for any reason, don't leave
            // the player with silence -- gracefully fall back.
            s_playingCustomTrack = false;
            FMODAudioEngine::playMusic(filename, loop, idk);
        }
    }

    void stopMusic() {
        s_playingCustomTrack = false;
        FMODAudioEngine::stopMusic();
    }
};

// Ensures menu music also respects the "apply to menus" setting, since
// MenuLayer triggers its own playMusic call on init.
class $modify(CustomMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (!Mod::get()->getSettingValue<bool>("apply-to-menu") && s_playingCustomTrack) {
            // User wants menus to keep vanilla/other-mod music; stop our
            // custom track and let the normal menu music call take over.
            if (auto engine = FMODAudioEngine::sharedEngine()) {
                if (engine->m_backgroundMusicChannel) {
                    engine->m_backgroundMusicChannel->stop();
                    engine->m_backgroundMusicChannel = nullptr;
                }
            }
            s_playingCustomTrack = false;
        }

        // Add a music-note button that opens a native file picker for
        // one-click MP3 selection, plus a smaller button to re-browse
        // previously imported tracks. Positioned to avoid overlapping
        // buttons other mods commonly add to this menu.
        if (auto menu = this->getChildByID("bottom-menu")) {
            auto pickSpr = CircleButtonSprite::create(
                CCSprite::createWithSpriteFrameName("GJ_musicOnBtn_001.png")
            );
            pickSpr->setScale(0.85f);
            auto pickBtn = CCMenuItemSpriteExtra::create(pickSpr, this, menu_selector(CustomMenuLayer::onOpenMusicLibrary));
            pickBtn->setID("custom-bg-music-pick-button"_spr);
            menu->addChild(pickBtn);

            auto listSpr = CircleButtonSprite::create(
                CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png"),
                CircleBaseColor::Green,
                CircleBaseSize::Small
            );
            auto listBtn = CCMenuItemSpriteExtra::create(listSpr, this, menu_selector(CustomMenuLayer::onOpenLibraryList));
            listBtn->setID("custom-bg-music-list-button"_spr);
            menu->addChild(listBtn);

            menu->updateLayout();
        }

        return true;
    }

    void onOpenMusicLibrary(CCObject*) {
        // Opens the native Windows file picker directly, filtered to mp3s,
        // so the user can grab any mp3 on their PC in one click instead of
        // having to manually drop it into the library folder first.
        file::FilePickOptions options = {
            .startPath = std::nullopt,
            .filters = {
                file::FilePickOptions::Filter {
                    .description = "MP3 Audio",
                    .files = { "*.mp3" }
                }
            }
        };

        file::pick(file::PickMode::OpenFile, options).listen(
            [](Result<std::filesystem::path, std::string>* result) {
                if (!result || result->isErr()) {
                    // User cancelled the dialog, or the OS picker errored --
                    // either way, there's nothing to import.
                    return;
                }

                auto sourcePath = result->unwrap();
                auto destPath = MusicLibrary::get().getLibraryFolder() / sourcePath.filename();

                std::error_code ec;
                std::filesystem::copy_file(
                    sourcePath, destPath,
                    std::filesystem::copy_options::overwrite_existing, ec
                );

                if (ec) {
                    log::error("CustomBGMusic: failed to import '{}': {}", sourcePath.string(), ec.message());
                    Notification::create("Failed to import that MP3", NotificationIcon::Error)->show();
                    return;
                }

                MusicLibrary::get().refresh();
                auto trackName = destPath.stem().string();
                MusicLibrary::get().setSelectedTrackName(trackName);
                applySelectedTrackImmediately();

                Notification::create(
                    fmt::format("Now playing: {}", trackName),
                    NotificationIcon::Success
                )->show();
            }
        );
    }

    // Long-standing library button (browse/re-select tracks already
    // imported previously) stays available as a second, smaller button
    // right next to the main one.
    void onOpenLibraryList(CCObject*) {
        MusicPickerPopup::create()->show();
    }
};

// Nothing PlayLayer-specific is required right now (playMusic hook above
// already covers level music), but this hook point is kept so future
// per-level track overrides can be added without touching the audio hook.
class $modify(CustomPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level) {
        return PlayLayer::init(level);
    }
};
