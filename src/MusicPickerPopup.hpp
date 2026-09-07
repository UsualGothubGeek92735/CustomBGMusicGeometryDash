#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ListView.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/loader/Event.hpp>
#include "MusicLibrary.hpp"

using namespace geode::prelude;

namespace CustomBGMusic {

    // A single row in the track list.
    class TrackCell : public CCNode {
    protected:
        Track m_track;
        std::function<void(Track const&)> m_onSelect;

        bool init(Track const& track, std::function<void(Track const&)> onSelect) {
            if (!CCNode::init()) return false;
            m_track = track;
            m_onSelect = onSelect;

            this->setContentSize({ 340.f, 30.f });

            auto label = CCLabelBMFont::create(track.name.c_str(), "chatFont.fnt");
            label->setAnchorPoint({ 0.f, 0.5f });
            label->setPosition({ 10.f, 15.f });
            label->limitLabelWidth(220.f, 0.6f, 0.1f);
            this->addChild(label);

            auto selectSpr = ButtonSprite::create("Select", "goldFont.fnt", "GJ_button_01.png", 0.6f);
            auto selectBtn = CCMenuItemSpriteExtra::create(selectSpr, this, menu_selector(TrackCell::onSelectClicked));
            auto menu = CCMenu::create();
            menu->addChild(selectBtn);
            menu->setPosition({ 290.f, 15.f });
            this->addChild(menu);

            return true;
        }

        void onSelectClicked(CCObject*) {
            if (m_onSelect) m_onSelect(m_track);
        }

    public:
        static TrackCell* create(Track const& track, std::function<void(Track const&)> onSelect) {
            auto ret = new TrackCell();
            if (ret->init(track, onSelect)) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
    };

    class MusicPickerPopup : public geode::Popup<> {
    protected:
        CCNode* m_listContainer = nullptr;
        EventListener<Task<Result<std::filesystem::path>>> m_pickListener;

        // Opens Windows' native "Open File" dialog filtered to .mp3, copies
        // the chosen file into the library folder, and selects it.
        void openNativeFilePicker() {
            auto filter = file::FilePickOptions::Filter {
                .description = "MP3 Audio",
                .files = { "*.mp3" }
            };
            file::FilePickOptions options {
                .filters = { filter }
            };

            m_pickListener.bind([this](Task<Result<std::filesystem::path>>::Event* e) {
                if (auto result = e->getValue()) {
                    if (result->isOk()) {
                        auto path = result->unwrap();
                        auto imported = MusicLibrary::get().importFile(path);
                        if (imported.has_value()) {
                            Notification::create(
                                fmt::format("Now playing: {}", *imported),
                                NotificationIcon::Success
                            )->show();
                            this->onClose(nullptr);
                        } else {
                            FLAlertLayer::create(
                                "Import Failed",
                                "Couldn't copy that file into your music library. Make sure it's a valid .mp3.",
                                "OK"
                            )->show();
                        }
                    }
                    // No value, or an Err result (e.g. the user cancelled the
                    // dialog) -> do nothing and stay on the popup.
                }
            });
            m_pickListener.setFilter(file::pickFile(file::PickMode::OpenFile, options));
        }

        bool setup() override {
            this->setTitle("Custom Music Library");

            MusicLibrary::get().refresh();
            auto& tracks = MusicLibrary::get().getTracks();

            auto winSize = m_mainLayer->getContentSize();

            // Always-available button: pick any mp3 from anywhere on your
            // PC via the native Windows file dialog.
            auto chooseSpr = ButtonSprite::create("Choose File From PC...", "goldFont.fnt", "GJ_button_04.png", 0.7f);
            auto chooseBtn = CCMenuItemSpriteExtra::create(chooseSpr, this, menu_selector(MusicPickerPopup::onChooseFile));
            auto chooseMenu = CCMenu::create();
            chooseMenu->addChild(chooseBtn);
            chooseMenu->setPosition({ winSize.width / 2, winSize.height - 45.f });
            m_mainLayer->addChild(chooseMenu);

            if (tracks.empty()) {
                auto folder = MusicLibrary::get().getLibraryFolder();
                auto info = CCLabelBMFont::create(
                    "No MP3s in your library yet.\nUse the button above to pick one,\nor drop files directly into:",
                    "chatFont.fnt"
                );
                info->setAlignment(kCCTextAlignmentCenter);
                info->setPosition({ winSize.width / 2, winSize.height / 2 + 10.f });
                m_mainLayer->addChild(info);

                auto pathLabel = CCLabelBMFont::create(folder.string().c_str(), "chatFont.fnt");
                pathLabel->setScale(0.4f);
                pathLabel->setPosition({ winSize.width / 2, winSize.height / 2 - 50.f });
                m_mainLayer->addChild(pathLabel);
                return true;
            }

            CCArray* cells = CCArray::create();
            for (auto const& track : tracks) {
                auto cell = TrackCell::create(track, [this](Track const& t) {
                    MusicLibrary::get().setSelectedTrackName(t.name);
                    Notification::create(
                        fmt::format("Selected: {}", t.name),
                        NotificationIcon::Success
                    )->show();
                    this->onClose(nullptr);
                });
                cells->addObject(cell);
            }

            auto listview = ListView::create(cells, 30.f, 340.f, 190.f);
            m_listContainer = listview;
            listview->setPosition({ (winSize.width - 340.f) / 2, (winSize.height - 190.f) / 2 - 25.f });
            m_mainLayer->addChild(listview);

            return true;
        }

        void onChooseFile(CCObject*) {
            openNativeFilePicker();
        }

    public:
        static MusicPickerPopup* create() {
            auto ret = new MusicPickerPopup();
            if (ret->initAnchored(380.f, 300.f)) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
    };

}
