#pragma once
class Menu {


public:
    static inline bool IsOpen() {
        const auto ui = RE::UI::GetSingleton();
        if (ui->IsApplicationMenuOpen() || ui->IsItemMenuOpen() || ui->IsModalMenuOpen() || ui->GameIsPaused()) {
            return true;
        }
        return false;
    }
    template <class T, std::enable_if_t<std::conjunction_v<RE::UIImpl::is_menu_ptr<T*>, RE::UIImpl::has_menu_name<T>>, int> = 0>
    static inline void SetMenuAlpha(float value) {
        const auto ui = RE::UI::GetSingleton();
        auto loadingMenu = ui->GetMenu<T>();
        RE::GFxValue root;
        if (loadingMenu->uiMovie->GetVariable(&root, "_root")) {
            root.SetMember("_alpha", RE::GFxValue(value));
        }
    }

    static inline void Close() {
        auto* uiManager = RE::UI::GetSingleton();

        if (const auto inventoryMenu = uiManager->GetMenu<RE::InventoryMenu>()) {
            RE::UIMessageQueue::GetSingleton()->AddMessage("InventoryMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
            RE::UIMessageQueue::GetSingleton()->AddMessage("TweenMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }

        if (const auto favoritesMenu = uiManager->GetMenu<RE::FavoritesMenu>()) {
            RE::UIMessageQueue::GetSingleton()->AddMessage("FavoritesMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }

        if (const auto containerMenu = uiManager->GetMenu<RE::ContainerMenu>()) {
            RE::UIMessageQueue::GetSingleton()->AddMessage("ContainerMenu", RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
    }
};