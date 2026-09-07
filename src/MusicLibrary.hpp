#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

namespace CustomBGMusic {

    // Represents a single mp3 in the user's GD-themed music library.
    struct Track {
        std::string name;          // display name (filename without extension)
        std::filesystem::path path; // full path on disk
    };

    class MusicLibrary {
    public:
        static MusicLibrary& get() {
            static MusicLibrary instance;
            return instance;
        }

        // Folder lives inside the mod's own save directory, so it never
        // collides with other mods' files or the base game's resources.
        std::filesystem::path getLibraryFolder() {
            auto folderName = geode::Mod::get()->getSettingValue<std::string>("library-folder");
            if (folderName.empty()) {
                folderName = "GD Music Library";
            }
            auto base = geode::Mod::get()->getSaveDir() / folderName;

            std::error_code ec;
            if (!std::filesystem::exists(base, ec)) {
                std::filesystem::create_directories(base, ec);
                if (ec) {
                    geode::log::error("CustomBGMusic: failed to create library folder: {}", ec.message());
                }
            }
            return base;
        }

        // Rescans disk for .mp3 files. Cheap enough to call whenever the
        // picker UI opens; we don't need a filesystem watcher.
        void refresh() {
            m_tracks.clear();
            m_scannedOnce = true;

            auto folder = getLibraryFolder();
            std::error_code ec;
            if (!std::filesystem::exists(folder, ec)) return;

            for (auto const& entry : std::filesystem::directory_iterator(folder, ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".mp3") continue;

                Track t;
                t.path = entry.path();
                t.name = entry.path().stem().string();
                m_tracks.push_back(std::move(t));
            }

            std::sort(m_tracks.begin(), m_tracks.end(), [](Track const& a, Track const& b) {
                return a.name < b.name;
            });
        }

        // Only auto-refreshes when we've never scanned yet; call refresh()
        // explicitly (e.g. when the picker UI opens) to pick up new files
        // added since the last scan.
        std::vector<Track> const& getTracks() {
            if (!m_scannedOnce) refresh();
            return m_tracks;
        }

        // Which track is currently selected, persisted via Geode's mod save data
        // (separate from settings so it can be changed at runtime from the UI).
        std::string getSelectedTrackName() {
            return geode::Mod::get()->getSavedValue<std::string>("selected-track", "");
        }

        void setSelectedTrackName(std::string const& name) {
            geode::Mod::get()->setSavedValue("selected-track", name);
        }

        // Copies an arbitrary mp3 (picked from anywhere on disk via the
        // native file dialog) into the GD-themed library folder, then
        // selects it. Returns the new display name on success.
        std::optional<std::string> importFile(std::filesystem::path const& source) {
            std::error_code ec;
            if (!std::filesystem::exists(source, ec) || ec) return std::nullopt;

            auto ext = source.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".mp3") return std::nullopt;

            auto destFolder = getLibraryFolder();

            // If the file is already sitting inside the library folder,
            // just select it in place instead of making a needless copy.
            if (std::filesystem::equivalent(source.parent_path(), destFolder, ec) && !ec) {
                refresh();
                auto name = source.stem().string();
                setSelectedTrackName(name);
                return name;
            }

            auto dest = destFolder / source.filename();

            // Avoid clobbering an existing file with the same name.
            int suffix = 1;
            while (std::filesystem::exists(dest, ec)) {
                dest = destFolder / (source.stem().string() + " (" + std::to_string(suffix) + ")" + source.extension().string());
                suffix++;
            }

            std::filesystem::copy_file(source, dest, ec);
            if (ec) {
                geode::log::error("CustomBGMusic: failed to import '{}': {}", source.string(), ec.message());
                return std::nullopt;
            }

            refresh();
            auto name = dest.stem().string();
            setSelectedTrackName(name);
            return name;
        }

        // Returns the full path to the currently selected track, or std::nullopt
        // if none is selected / it no longer exists on disk.
        std::optional<std::filesystem::path> getSelectedTrackPath() {
            auto selected = getSelectedTrackName();
            if (selected.empty()) return std::nullopt;

            for (auto const& t : getTracks()) {
                if (t.name == selected) {
                    std::error_code ec;
                    if (std::filesystem::exists(t.path, ec)) {
                        return t.path;
                    }
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }

    private:
        std::vector<Track> m_tracks;
        bool m_scannedOnce = false;
    };

    // Defined in main.cpp (needs FMOD/FMODAudioEngine internals). Restarts
    // playback right now with whatever track is currently selected, instead
    // of waiting for the next natural playMusic() call. Called immediately
    // after the user picks a track so there's no need to leave and re-enter
    // the menu to hear the change.
    void applySelectedTrackImmediately();

}
