#pragma once
// SDK-created navigation controls; no game layout asset is overwritten.
static constexpr uint32_t kPortraitPrevious = 0xbec1b001;
static constexpr uint32_t kPortraitCounter = 0xbec1b002;
static constexpr uint32_t kPortraitNext = 0xbec1b003;
static constexpr uint32_t kPortraitBackground = 0xbec1b004;
static constexpr float kPagerWidth = 92.0f, kPagerHeight = 22.0f;
// Native DefaultStyleCentered from Spore_Graphics' UI stylesheet. Unlike the
// panel's font style, this explicitly centers both text-align and text-valign.
static constexpr uint32_t kPagerCenteredFont = 0x00aebb70;
// Native hide/show code looks up the empty row using this offset from the
// matching occupied row's control ID (supported executable: 0xE049BD).
static constexpr uint32_t kEmptyPortraitOffset = 0x00323776;
// Previous, counter, next, background. The text has its own central 44-unit
// region; the full-width textured background has no caption or mouse handler.
static eastl::intrusive_ptr<UTFWin::IButton> pager_buttons[4];
static uintptr_t pager_ui_panel_identity;
static uintptr_t pager_layout_anchor_identity;
static UTFWin::IButton* (*create_portrait_button)() = UTFWin::IButton::Create;
static auto load_pager_atlas = UTFWin::Image::GetImage;
static bool pager_background_attempted;

// Pinned SDK Image::mDimensions is declared as two floats, but the supported
// engine stores two int32s: constructor 0x957950 writes them at +0x1C/+0x20;
// sliced renderer 0x95CF83/0x95CFA9 uses CVTSI2SS on those fields. Keep this
// correction local; do not change the SDK or any game-owned image.
static Math::IntDimensions pager_image_size(const UTFWin::Image& image)
{
    Math::IntDimensions size;
    static_assert(sizeof size == sizeof image.GetDimensions(), "Image dimensions layout changed");
    memcpy(&size, &image.GetDimensions(), sizeof size);
    return size;
}

// The empty row's image includes an embossed telephone. Instead use the blank,
// rounded panel from the same native atlas (Civ HUD SPUI 0x6A6CE68B, image 849).
// Own the UV wrapper: never change the atlas or an image used by the game HUD.
class PortraitPagerTexture final : public UTFWin::Image {
public:
    explicit PortraitPagerTexture(const UTFWin::Image& atlas) : Image(atlas) {
        mnRefCount = 0; // A newly owned wrapper, not the source's reference count.
        // Native image UVs are origin + extent, not opposite corners. The
        // SPUI loader subtracts its right/bottom before calling 0x957950.
        SetTexCoords(Math::Rectangle(242.0f/512, 0, 133.0f/512, 116.0f/512));
        const Math::IntDimensions size(133, 116);
        memcpy(&mDimensions, &size, sizeof size);
    }
};
class PortraitPagerBackground final : public UTFWin::StdDrawable {
public:
    explicit PortraitPagerBackground(UTFWin::Image* texture) {
        for (auto& image : mpImages) image = texture;
        mScaleType = UTFWin::Scaling::StretchCenter;
        mScaleArea = Math::Rectangle(12.0f/133, 12.0f/116, 12.0f/133, 12.0f/116);
        mScaleFactor = Math::Vector2(0.5f, 0.5f); // Preserve six-unit corner slices.
    }
};
static IDrawablePtr make_portrait_background()
{
    ImagePtr atlas;
    const bool loaded = load_pager_atlas(ResourceKey(0x03996401, 0x2f7d0004, 0x31a44893), atlas, false, -1, -1);
    if (!loaded || !atlas) {
        ModAPI::Log("[Better Civ UI] pager-background fallback: atlas-load-failed loaded=%u image=%u",
            unsigned(loaded), unsigned(bool(atlas)));
        return nullptr;
    }
    const auto size = pager_image_size(*atlas);
    if (size.width != 512 || size.height != 512) {
        ModAPI::Log("[Better Civ UI] pager-background fallback: atlas-size=%dx%d", size.width, size.height);
        return nullptr;
    }
    ImagePtr texture = new PortraitPagerTexture(*atlas);
    ModAPI::Log("[Better Civ UI] pager-background ready: rounded-native atlas=%dx%d crop=133x116", size.width, size.height);
    return new PortraitPagerBackground(texture.get());
}

static bool handle_pager_command(uint32_t control)
{
    if (!portrait_pagination_enabled || !portrait_reuse_ready || !in_civilization() ||
        !portrait_pages.panel_identity || portrait_comm_screen_active()) return false;
    if (control == kPortraitPrevious && portrait_pages.page > 0) portrait_pages.requested_step = -1;
    else if (control == kPortraitNext && portrait_pages.page + 1 < portrait_page_count()) portrait_pages.requested_step = 1;
    else return false;
    return true;
}
class PortraitPageButtons final : public UTFWin::DefaultWinProc<> {
public:
    bool HandleUIMessage(UTFWin::IWindow*, const UTFWin::Message& message) override {
        return message.IsType(UTFWin::kMsgButtonClick) && message.source &&
            handle_pager_command(message.source->GetControlID());
    }
};

static bool visible_ancestor_chain(UTFWin::IWindow* window)
{
    // Guard against malformed/modded parent chains without retaining pointers.
    for (unsigned depth = 0; window && depth < 64; ++depth) {
        if (!window->IsVisible()) return false;
        window = window->GetParent();
        if (!window) return true;
    }
    return false;
}
static UTFWin::IWindow* portrait_anchor(Native::RivalPortraitPanel* panel, bool visible_only)
{
    if (!native_find_portrait_window) return nullptr;
    for (uint32_t id : {0x046977ffu, 0x055d626fu}) {
        auto* root = native_find_portrait_window(panel, id);
        if (root && (!visible_only || visible_ancestor_chain(root))) return root;
    }
    return nullptr;
}
static void dispose_portrait_pager()
{
    for (auto& button : pager_buttons) {
        if (!button) continue;
        auto* window = button->ToWindow();
        if (auto* parent = window->GetParent()) parent->RemoveWindow(window);
        button = nullptr;
    }
    pager_ui_panel_identity = 0;
    pager_layout_anchor_identity = 0;
    pager_background_attempted = false;
}
static UTFWin::IWindow* portrait_hud_parent(UTFWin::IWindow* anchor)
{
    auto* manager = anchor ? anchor->GetWindowManager() : nullptr;
    auto* main = manager ? manager->GetMainWindow() : nullptr;
    if (!main) return nullptr;
    // Keep navigation within the HUD's own top-level subtree so later menus
    // can cover it normally. Do not create an always-on-top global overlay.
    auto* parent = anchor;
    for (unsigned depth = 0; parent && depth < 64; ++depth) {
        if (parent == main || parent->GetParent() == main) return parent;
        parent = parent->GetParent();
    }
    return nullptr;
}
static bool ensure_portrait_pager(Native::RivalPortraitPanel* panel)
{
    auto* anchor = portrait_anchor(panel, false);
    auto* parent = portrait_hud_parent(anchor);
    if (!parent) return false;
    if (pager_ui_panel_identity != reinterpret_cast<uintptr_t>(panel) ||
        (pager_buttons[0] && pager_buttons[0]->ToWindow()->GetParent() != parent)) dispose_portrait_pager();
    if (pager_buttons[0] && pager_buttons[1] && pager_buttons[2] && pager_buttons[3]) return true;
    const uint32_t controls[] = {kPortraitPrevious, kPortraitCounter, kPortraitNext, kPortraitBackground};
    const char16_t* captions[] = {u"<", u"1/1", u">", u""};
    for (unsigned i = 0; i < 4; ++i) {
        pager_buttons[i] = create_portrait_button();
        if (!pager_buttons[i]) { dispose_portrait_pager(); return false; }
        auto* window = pager_buttons[i]->ToWindow();
        window->SetControlID(controls[i]);
        window->SetTextFontID(kPagerCenteredFont);
        window->SetCaption(captions[i]);
        window->SetFlag(UTFWin::kWinFlagEnabled, true);
        window->SetFlag(UTFWin::kWinFlagAlwaysInFront, false);
        window->SetFlag(UTFWin::kWinFlagClickToFront, false);
        window->SetFlag(UTFWin::kWinFlagIgnoreMouse, i == 1 || i == 3);
        window->SetFlag(UTFWin::kWinFlagClip, true);
        window->SetVisible(false);
        // Also used as a fallback when a replacement HUD has no empty-row drawable.
        window->SetFillColor(Math::Color(i == 3 ? 0xe020211bu : i == 1 ? 0u : 0xe04c4734u));
        window->SetShadeColor(Math::Color(0xffffffffu));
        for (unsigned state = 0; state < 8; ++state)
            pager_buttons[i]->SetCaptionColor(static_cast<UTFWin::StateIndices>(state),
                Math::Color(state == 1 || state == 5 ? 0xff827b65u :
                    state == 2 || state == 6 ? 0xffffffffu : 0xffe9dfb8u));
        // Keep the engine's default caption margins. In the supported Steam
        // executable IButton vtable slot 0x54 takes four floats (ret 16), but
        // the pinned SDK declares one Rectangle reference (4 bytes). Calling
        // that declaration corrupts the stack, even while controls are hidden.
        if (i == 0 || i == 2) window->AddWinProc(new PortraitPageButtons());
    }
    // Do not infer stacking from AddWindow's documentation: the supported
    // executable inserts ordinary children at the front (0x9627CD).
    for (unsigned i = 0; i < 4; ++i) parent->AddWindow(pager_buttons[i]->ToWindow());
    for (unsigned i = 0; i < 3; ++i) parent->BringToFront(pager_buttons[i]->ToWindow());
    pager_ui_panel_identity = reinterpret_cast<uintptr_t>(panel);
    ModAPI::Log("[Better Civ UI] Pagination controls created.");
    return true;
}
static void draw_portrait_pager(Native::RivalPortraitPanel* panel)
{
    if (!pager_buttons[0] || !pager_buttons[1] || !pager_buttons[2] || !pager_buttons[3]) return;
    auto* anchor = portrait_anchor(panel, true);
    const bool talking = portrait_comm_screen_active();
    const bool visible = anchor && pager_owns_panel(panel) && portrait_page_count() > 1 && !talking;
    for (auto& button : pager_buttons) button->ToWindow()->SetVisible(visible);
    if (!visible) return;
    auto* parent = pager_buttons[0]->ToWindow()->GetParent();
    if (!parent) return;
    // Hidden placeholders can retain stale layout/transform data. Anchor to
    // the matching VISIBLE occupied row instead.
    UTFWin::IWindow* empty_row = nullptr;
    UTFWin::IWindow* first_row = nullptr;
    if (panel->slots[0].window_id) {
        empty_row = anchor->FindWindowByID(panel->slots[0].window_id + kEmptyPortraitOffset, false);
        auto* occupied_row = anchor->FindWindowByID(panel->slots[0].window_id, false);
        if (occupied_row && visible_ancestor_chain(occupied_row)) first_row = occupied_row;
        else if (empty_row && visible_ancestor_chain(empty_row)) first_row = empty_row;
    }
    if (!first_row) first_row = anchor;
    auto* background = pager_buttons[3]->ToWindow();
    if (!pager_background_attempted) {
        pager_background_attempted = true;
        auto drawable = make_portrait_background();
        background->SetDrawable(drawable.get());
        // Dim just the background for readable cream captions, not the arrows.
        background->SetShadeColor(Math::Color(0xffb8b8b8u));
    }
    auto* drawable = background->GetDrawable();
    background->SetFillColor(Math::Color(drawable ? 0x00000000u : 0xe020211bu));
    for (unsigned i : {0u, 2u})
        pager_buttons[i]->ToWindow()->SetFillColor(Math::Color(drawable ? 0x00000000u : 0xe04c4734u));
    // Restore the pre-fix-6 separation: no downward join inset. Transparent
    // edge padding in the native artwork leaves a small visible gap.
    // If there is no room above, retain the below-panel fallback.
    const auto anchor_area = anchor->GetRealArea();
    const float anchor_width = anchor_area.right - anchor_area.left;
    const float anchor_height = anchor_area.bottom - anchor_area.top;
    const auto row_area = first_row->GetRealArea();
    auto position = parent->ToLocalCoordinates(first_row->ToGlobalCoordinates(
        Math::Point(row_area.right - row_area.left, 0)));
    position.x -= kPagerWidth;
    const auto bounds = parent->GetRealArea();
    const float width = bounds.right - bounds.left;
    const float height = bounds.bottom - bounds.top;
    if (position.y >= kPagerHeight)
        position.y -= kPagerHeight;
    else position.y = parent->ToLocalCoordinates(anchor->ToGlobalCoordinates(
        Math::Point(anchor_width, anchor_height))).y;
    const float max_x = width > kPagerWidth ? width - kPagerWidth : 0;
    const float max_y = height > kPagerHeight ? height - kPagerHeight : 0;
    if (position.x < 0) position.x = 0;
    if (position.x > max_x) position.x = max_x;
    if (position.y < 0) position.y = 0;
    if (position.y > max_y) position.y = max_y;
    if (pager_layout_anchor_identity != reinterpret_cast<uintptr_t>(anchor)) {
        pager_layout_anchor_identity = reinterpret_cast<uintptr_t>(anchor);
        ModAPI::Log("[Better Civ UI] pager-layout root=%08x row=%08x texture=%u x=%.1f y=%.1f width=%.1f height=%.1f",
            anchor->GetControlID(), first_row->GetControlID(), unsigned(drawable != nullptr),
            position.x, position.y, kPagerWidth, kPagerHeight);
    }
    const float offsets[] = {0, 24, 68, 0};
    const float widths[] = {24, 44, 24, kPagerWidth};
    for (unsigned i = 0; i < 4; ++i)
        pager_buttons[i]->ToWindow()->SetArea(Math::Rectangle(position.x + offsets[i], position.y,
            position.x + offsets[i] + widths[i], position.y + kPagerHeight));
    const char16_t caption[] = {char16_t(u'1' + portrait_pages.page), u'/',
        char16_t(u'0' + portrait_page_count()), 0};
    pager_buttons[1]->ToWindow()->SetCaption(caption);
    // Put each alert inside its existing arrow button, towards the counter.
    // No extra control or geometry change; three pages can alert both ways.
    pager_buttons[0]->ToWindow()->SetCaption(portrait_pages.hidden_call_before ? u"< !" : u"<");
    pager_buttons[2]->ToWindow()->SetCaption(portrait_pages.hidden_call_after ? u"! >" : u">");
    pager_buttons[0]->ToWindow()->SetFlag(UTFWin::kWinFlagEnabled, portrait_pages.page > 0);
    pager_buttons[2]->ToWindow()->SetFlag(UTFWin::kWinFlagEnabled, portrait_pages.page + 1 < portrait_page_count());
}
