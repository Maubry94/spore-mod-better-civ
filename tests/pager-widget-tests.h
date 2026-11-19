// Exercise the production widget setup/cleanup through SDK interface slots.
// Unimplemented calls abort, including the incompatible caption-margin slot.
struct PagerWidgetFixture {
    uintptr_t* table;
    PagerWidgetFixture* parent = nullptr;
    PagerWidgetFixture* window = nullptr;
    int refs = 0;
    uint32_t id = 0, flags = UTFWin::kWinFlagVisible, font = 123;
    unsigned colors = 0;
    uint32_t fill = 0;
    Math::Rectangle area{0, 0, 100, 100};
    UTFWin::IDrawable* drawable = nullptr;
    eastl::intrusive_ptr<UTFWin::IWinProc> listener;
    std::u16string caption;
};
static PagerWidgetFixture widget_main{}, widget_hud{}, widget_anchor{}, widget_manager{};
static PagerWidgetFixture widget_empty{}, widget_occupied{};
static ImagePtr widget_atlas;
static unsigned widget_atlas_loads;
static bool widget_atlas_available = true;
static bool widget_load_atlas(const ResourceKey& key, ImagePtr& output, bool flag, int height, int width) {
    assert(key.instanceID == 0x03996401 && key.typeID == 0x2f7d0004 && key.groupID == 0x31a44893);
    assert(!flag && height == -1 && width == -1);
    ++widget_atlas_loads;
    if (widget_atlas_available) output = widget_atlas;
    return widget_atlas_available;
}
static PagerWidgetFixture widget_windows[4]{}, widget_buttons[4]{};
static unsigned widget_created, widget_fail_at = UINT32_MAX, widget_attached;
static uint32_t widget_attach_order[4]{};
static PagerWidgetFixture* widget_stack[4]{}; // Frontmost first, as in the executable.
static bool widget_row_available = true;
static void __cdecl unexpected_widget_call() { assert(false && "Unexpected or incompatible SDK widget call"); abort(); }
static int __fastcall widget_addref(PagerWidgetFixture* p, void*) { return ++p->refs; }
static int __fastcall widget_release(PagerWidgetFixture* p, void*) {
    assert(p->refs > 0);
    --p->refs;
    if (!p->refs && p->window) {
        auto* w = p->window;
        w->listener = nullptr;
        if (w->drawable) { w->drawable->Release(); w->drawable = nullptr; }
    }
    return p->refs;
}
static UTFWin::IWindow* __fastcall widget_parent(PagerWidgetFixture* p, void*) { return reinterpret_cast<UTFWin::IWindow*>(p->parent); }
static UTFWin::IWindow* __fastcall widget_to_window(PagerWidgetFixture* p, void*) { return reinterpret_cast<UTFWin::IWindow*>(p->window); }
static UTFWin::IWindow* __fastcall widget_get_main(void*, void*) { return reinterpret_cast<UTFWin::IWindow*>(&widget_main); }
static UTFWin::IWindowManager* __fastcall widget_get_manager(void*, void*) { return reinterpret_cast<UTFWin::IWindowManager*>(&widget_manager); }
static uint32_t __fastcall widget_get_id(PagerWidgetFixture* p, void*) { return p->id; }
static uint32_t __fastcall widget_get_flags(PagerWidgetFixture* p, void*) { return p->flags; }
static uint32_t __fastcall widget_get_font(PagerWidgetFixture* p, void*) { return p->font; }
static void __fastcall widget_set_id(PagerWidgetFixture* p, void*, uint32_t id) { p->id = id; }
static void __fastcall widget_set_font(PagerWidgetFixture* p, void*, uint32_t font) { p->font = font; }
static void __fastcall widget_set_flag(PagerWidgetFixture* p, void*, uint32_t flag, bool enabled) {
    if (enabled) p->flags |= flag; else p->flags &= ~flag;
}
static void __fastcall widget_set_color(void*, void*, uint32_t) {}
static void __fastcall widget_set_fill(PagerWidgetFixture* p, void*, uint32_t color) { p->fill = color; }
static const Math::Rectangle& __fastcall widget_area(PagerWidgetFixture* p, void*) { return p->area; }
static void __fastcall widget_set_area(PagerWidgetFixture* p, void*, const Math::Rectangle& area) { p->area = area; }
// Thiscall Point returns use an explicit hidden result pointer on the stack.
static Math::Point* __fastcall widget_global(PagerWidgetFixture* p, void*, Math::Point* result, Math::Point point) {
    *result = point;
    for (; p; p = p->parent) { result->x += p->area.left; result->y += p->area.top; }
    return result;
}
static Math::Point* __fastcall widget_local(PagerWidgetFixture* p, void*, Math::Point* result, Math::Point point) {
    *result = point;
    for (; p; p = p->parent) { result->x -= p->area.left; result->y -= p->area.top; }
    return result;
}
static UTFWin::IDrawable* __fastcall widget_get_drawable(PagerWidgetFixture* p, void*) {
    assert(p != &widget_empty); // The telephone-bearing native row is never reused.
    return p->drawable;
}
static void __fastcall widget_set_drawable(PagerWidgetFixture* p, void*, UTFWin::IDrawable* drawable) {
    if (drawable) drawable->AddRef();
    if (p->drawable) p->drawable->Release();
    p->drawable = drawable;
}
static UTFWin::IWindow* __fastcall widget_find_row(PagerWidgetFixture* p, void*, uint32_t id, bool recursive) {
    assert(p == &widget_anchor && !recursive);
    if (!widget_row_available) return nullptr;
    if (id == widget_empty.id) return reinterpret_cast<UTFWin::IWindow*>(&widget_empty);
    if (id == widget_occupied.id) return reinterpret_cast<UTFWin::IWindow*>(&widget_occupied);
    return nullptr;
}
static void __fastcall widget_set_caption(PagerWidgetFixture* p, void*, const char16_t* caption) { p->caption = caption; }
static void __fastcall widget_caption_color(PagerWidgetFixture* p, void*, unsigned state, uint32_t) {
    assert(state < 8); ++p->colors;
}
static void __fastcall widget_add_proc(PagerWidgetFixture* p, void*, UTFWin::IWinProc* proc) { p->listener = proc; }
static void __fastcall widget_add_child(PagerWidgetFixture* p, void*, PagerWidgetFixture* child) {
    assert(!child->parent && widget_attached < 4); child->parent = p; ++child->refs;
    for (unsigned i = widget_attached; i > 0; --i) widget_stack[i] = widget_stack[i - 1];
    widget_stack[0] = child; // Native AddWindow inserts at the front, contrary to SDK prose.
    widget_attach_order[widget_attached++] = child->id;
}
static void __fastcall widget_bring_front(PagerWidgetFixture* p, void*, PagerWidgetFixture* child) {
    assert(child->parent == p);
    unsigned at = 0;
    while (at < widget_attached && widget_stack[at] != child) ++at;
    assert(at < widget_attached);
    for (; at > 0; --at) widget_stack[at] = widget_stack[at - 1];
    widget_stack[0] = child;
}
static void __fastcall widget_remove_child(PagerWidgetFixture* p, void*, PagerWidgetFixture* child) {
    assert(child->parent == p && child->refs > 0 && widget_attached);
    unsigned at = 0;
    while (at < widget_attached && widget_stack[at] != child) ++at;
    assert(at < widget_attached);
    for (; at + 1 < widget_attached; ++at) widget_stack[at] = widget_stack[at + 1];
    widget_stack[widget_attached - 1] = nullptr;
    child->parent = nullptr; --child->refs; --widget_attached; child->listener = nullptr;
}
static UTFWin::IWindow* __fastcall widget_find(void*, void*, uint32_t id) {
    assert(id == 0x046977ff || id == 0x055d626f);
    return reinterpret_cast<UTFWin::IWindow*>(&widget_anchor);
}
static UTFWin::IButton* widget_create() {
    if (widget_created == widget_fail_at) return nullptr;
    assert(widget_created < 4);
    return reinterpret_cast<UTFWin::IButton*>(&widget_buttons[widget_created++]);
}
static bool widget_not_talking() { return false; }

static void verify_pager_widget_setup()
{
    uintptr_t window_table[0x108 / 4], button_table[0x70 / 4], manager_table[2];
    for (auto& slot : window_table) slot = reinterpret_cast<uintptr_t>(unexpected_widget_call);
    for (auto& slot : button_table) slot = reinterpret_cast<uintptr_t>(unexpected_widget_call);
    manager_table[0] = reinterpret_cast<uintptr_t>(unexpected_widget_call);
    manager_table[1] = reinterpret_cast<uintptr_t>(widget_get_main);
    window_table[0] = button_table[0] = reinterpret_cast<uintptr_t>(widget_addref);
    window_table[1] = button_table[1] = reinterpret_cast<uintptr_t>(widget_release);
    window_table[0x10/4] = reinterpret_cast<uintptr_t>(widget_parent);
    window_table[0x14/4] = reinterpret_cast<uintptr_t>(widget_get_manager);
    window_table[0x1c/4] = reinterpret_cast<uintptr_t>(widget_get_id);
    window_table[0x28/4] = reinterpret_cast<uintptr_t>(widget_get_flags);
    window_table[0x38/4] = reinterpret_cast<uintptr_t>(widget_area);
    window_table[0x40/4] = reinterpret_cast<uintptr_t>(widget_get_font);
    window_table[0x50/4] = reinterpret_cast<uintptr_t>(widget_set_id);
    window_table[0x5c/4] = reinterpret_cast<uintptr_t>(widget_set_color);
    window_table[0x60/4] = reinterpret_cast<uintptr_t>(widget_set_area);
    window_table[0xac/4] = reinterpret_cast<uintptr_t>(widget_set_fill);
    window_table[0xa8/4] = reinterpret_cast<uintptr_t>(widget_get_drawable);
    window_table[0xb0/4] = reinterpret_cast<uintptr_t>(widget_set_drawable);
    window_table[0xc0/4] = reinterpret_cast<uintptr_t>(widget_global);
    window_table[0xc4/4] = reinterpret_cast<uintptr_t>(widget_local);
    window_table[0x7c/4] = reinterpret_cast<uintptr_t>(widget_set_flag);
    window_table[0x80/4] = reinterpret_cast<uintptr_t>(widget_set_caption);
    window_table[0x84/4] = reinterpret_cast<uintptr_t>(widget_set_font);
    window_table[0xd8/4] = reinterpret_cast<uintptr_t>(widget_add_child);
    window_table[0xdc/4] = reinterpret_cast<uintptr_t>(widget_remove_child);
    window_table[0xe8/4] = reinterpret_cast<uintptr_t>(widget_bring_front);
    window_table[0xf0/4] = reinterpret_cast<uintptr_t>(widget_find_row);
    window_table[0x104/4] = reinterpret_cast<uintptr_t>(widget_add_proc);
    button_table[0x10/4] = reinterpret_cast<uintptr_t>(widget_to_window);
    button_table[0x4c/4] = reinterpret_cast<uintptr_t>(widget_caption_color);
    // Slot 0x54 intentionally remains fatal: its native ABI differs from SDK.
    widget_main.table = widget_hud.table = widget_anchor.table = widget_empty.table = widget_occupied.table = window_table;
    widget_manager.table = manager_table;
    widget_hud.parent = &widget_main; widget_anchor.parent = &widget_hud;
    widget_empty.parent = &widget_anchor;
    widget_occupied.parent = &widget_anchor;
    for (unsigned i=0; i<4; ++i) {
        widget_buttons[i].table = button_table; widget_buttons[i].window = &widget_windows[i];
        widget_windows[i].table = window_table;
    }
    const auto old_create = create_portrait_button;
    const auto old_find = native_find_portrait_window;
    const auto old_talking = portrait_comm_screen_active;
    const auto old_load_atlas = load_pager_atlas;
    load_pager_atlas = widget_load_atlas;
    widget_atlas = new UTFWin::Image();
    // Model independently observed engine bytes, not the incorrect float
    // setter in the SDK. Fix 7's float-valued fixture hid the real rejection.
    const int32_t native_size[2] = {512, 512};
    memcpy(reinterpret_cast<unsigned char*>(widget_atlas.get()) + 0x1c, native_size, sizeof native_size);
    widget_atlas->SetTexCoords(Math::Rectangle(0, 0, 1, 1));
    assert(widget_atlas->GetDimensions().width != 512.0f); // Reproduces the old guard's failure.
    assert(pager_image_size(*widget_atlas).width == 512 && pager_image_size(*widget_atlas).height == 512);
    // Reject malformed/different layouts rather than constructing oversized
    // native slice dimensions from IEEE float bits.
    widget_atlas->SetDimensions(Math::Dimensions(512, 512));
    assert(!make_portrait_background());
    const int32_t wrong_size[2] = {256, 256};
    memcpy(reinterpret_cast<unsigned char*>(widget_atlas.get()) + 0x1c, wrong_size, sizeof wrong_size);
    assert(!make_portrait_background());
    memcpy(reinterpret_cast<unsigned char*>(widget_atlas.get()) + 0x1c, native_size, sizeof native_size);
    widget_atlas_loads = 0;
    create_portrait_button = widget_create;
    native_find_portrait_window = reinterpret_cast<FindPortraitWindow>(widget_find);
    portrait_comm_screen_active = widget_not_talking;
    Native::RivalPortraitPanel panel{};
    panel.slots[0].window_id = 0x1000;
    widget_occupied.id = panel.slots[0].window_id;
    widget_empty.id = panel.slots[0].window_id + kEmptyPortraitOffset;
    // New-game path: controls are created even when no rival qualifies yet.
    portrait_pages = {}; widget_created = widget_attached = 0;
    assert(ensure_portrait_pager(&panel));
    assert(widget_created == 4 && widget_attached == 4);
    assert(pager_ui_panel_identity == reinterpret_cast<uintptr_t>(&panel));
    assert(ensure_portrait_pager(&panel)); // Repeated updates must not recreate.
    assert(widget_created == 4 && widget_attached == 4);
    draw_portrait_pager(&panel); // Hidden before the fifth eligible rival.
    for (unsigned i=0; i<4; ++i) {
        assert(widget_buttons[i].refs == 1 && widget_windows[i].refs == 1);
        assert(widget_buttons[i].colors == 8 && widget_windows[i].font == kPagerCenteredFont);
        assert(!(widget_windows[i].flags & UTFWin::kWinFlagVisible));
        assert(widget_windows[i].id == kPortraitPrevious + i);
        assert(bool(widget_windows[i].listener) == (i == 0 || i == 2));
        assert(widget_windows[i].caption.empty() == (i == 3));
        assert(widget_windows[i].flags & UTFWin::kWinFlagClip);
    }
    for (unsigned i = 0; i < 4; ++i) assert(widget_attach_order[i] == kPortraitPrevious + i);
    assert(widget_stack[3] == &widget_windows[3]); // Background below all three captions.
    for (unsigned i = 0; i < 3; ++i) assert(widget_stack[i] != &widget_windows[3]);

    // Hidden placeholders may keep stale positions: use the occupied row for
    // geometry. The new background owns its UV wrapper and never changes the
    // original atlas. Global and local origins differ.
    portrait_reuse_ready = true;
    portrait_pages.panel_identity = reinterpret_cast<uintptr_t>(&panel);
    portrait_pages.count = 5;
    widget_hud.area = {10, 20, 810, 620};
    widget_anchor.area = {500, 300, 610, 450};
    widget_occupied.area = {5, 20, 105, 45};
    widget_empty.area = {-100, 0, 0, 25}; // Stale hidden geometry, like the reported regression.
    widget_empty.flags = 0;
    draw_portrait_pager(&panel);
    assert(widget_windows[3].drawable && widget_atlas_loads == 1);
    auto* first_background = widget_windows[3].drawable;
    assert(memcmp(reinterpret_cast<unsigned char*>(widget_atlas.get()) + 0x1c, native_size, sizeof native_size) == 0);
    assert(widget_atlas->GetTexCoords().left == 0 && widget_atlas->GetTexCoords().right == 1);
    ImagePtr crop = new PortraitPagerTexture(*widget_atlas);
    const int32_t native_crop_size[2] = {133, 116};
    assert(memcmp(reinterpret_cast<unsigned char*>(crop.get()) + 0x1c, native_crop_size, sizeof native_crop_size) == 0);
    // Native UV layout: x, y, width, height. The right edge is x + width.
    const float native_crop_uv[4] = {242.0f/512, 0, 133.0f/512, 116.0f/512};
    assert(memcmp(reinterpret_cast<unsigned char*>(crop.get()) + 0xc, native_crop_uv, sizeof native_crop_uv) == 0);
    assert(crop->GetTexCoords().left + crop->GetTexCoords().right == 375.0f/512);
    crop = nullptr;
    assert(widget_windows[3].area.left == 513 && widget_windows[3].area.right == 605);
    assert(widget_windows[3].area.top == 298 && widget_windows[3].area.bottom == 320);
    assert(widget_windows[0].fill == 0 && widget_windows[2].fill == 0);
    assert(widget_windows[1].flags & UTFWin::kWinFlagIgnoreMouse);
    assert(!(widget_windows[0].flags & UTFWin::kWinFlagEnabled));
    assert(widget_windows[2].flags & UTFWin::kWinFlagEnabled);
    draw_portrait_pager(&panel);
    assert(widget_windows[3].drawable == first_background && widget_atlas_loads == 1);

    // The page text is centered in a dedicated middle region and clipped to
    // it, never painted on an arrow or used as the full-width background.
    assert(!widget_windows[1].drawable && widget_windows[1].fill == 0);
    assert(widget_windows[1].area.left == widget_windows[0].area.right);
    assert(widget_windows[1].area.right == widget_windows[2].area.left);
    assert(widget_windows[1].area.left + widget_windows[1].area.right ==
        widget_windows[3].area.left + widget_windows[3].area.right);
    assert(widget_windows[3].flags & UTFWin::kWinFlagIgnoreMouse);
    assert(!widget_windows[3].listener && widget_windows[3].caption.empty());
    portrait_pages.count = 9;
    for (unsigned page = 0; page < 3; ++page) {
        portrait_pages.page = page;
        draw_portrait_pager(&panel);
        const std::u16string expected = std::u16string(1, char16_t(u'1' + page)) + u"/3";
        assert(widget_windows[1].caption == expected);
        assert(widget_windows[1].area.right - widget_windows[1].area.left == 44);
    }
    portrait_pages.hidden_call_before = true;
    draw_portrait_pager(&panel); assert(widget_windows[1].caption == u"3/3");
    assert(widget_windows[0].caption == u"< !" && widget_windows[2].caption == u">");
    assert(widget_windows[1].area.left == widget_windows[0].area.right);
    // Exercise all alert combinations on the middle of three pages; the
    // counter never gains an exclamation mark and arrow regions stay fixed.
    portrait_pages.page = 1;
    for (unsigned mask = 0; mask < 4; ++mask) {
        portrait_pages.hidden_call_before = (mask & 1) != 0;
        portrait_pages.hidden_call_after = (mask & 2) != 0;
        draw_portrait_pager(&panel);
        assert(widget_windows[1].caption == u"2/3");
        assert(widget_windows[0].caption == (mask & 1 ? u"< !" : u"<"));
        assert(widget_windows[2].caption == (mask & 2 ? u"! >" : u">"));
        assert(widget_windows[0].area.right - widget_windows[0].area.left == 24);
        assert(widget_windows[2].area.right - widget_windows[2].area.left == 24);
        assert(widget_windows[1].area.left == widget_windows[0].area.right);
        assert(widget_windows[1].area.right == widget_windows[2].area.left);
    }
    portrait_pages.hidden_call_before = portrait_pages.hidden_call_after = false;
    portrait_pages.count = 5;

    // Compact/expanded layout movement and boundary arrows track each frame.
    widget_anchor.area = {430, 260, 610, 450};
    widget_occupied.area = {5, 25, 175, 50};
    portrait_pages.page = 1; draw_portrait_pager(&panel);
    assert(widget_windows[3].area.bottom == 285 && widget_windows[3].area.right == 605);
    assert(widget_windows[1].caption == u"2/2");
    assert(widget_windows[0].flags & UTFWin::kWinFlagEnabled);
    assert(!(widget_windows[2].flags & UTFWin::kWinFlagEnabled));

    // No room above: place below the whole root, never over the next portrait.
    widget_anchor.area = {500, 0, 610, 150};
    widget_occupied.area = {5, 5, 105, 30};
    draw_portrait_pager(&panel); assert(widget_windows[3].area.top == 150);
    widget_anchor.area = {790, 590, 900, 740};
    draw_portrait_pager(&panel);
    assert(widget_windows[3].area.right <= 800 && widget_windows[3].area.bottom <= 600);

    // An actually visible empty row can supply geometry when no occupied row
    // is shown. If BOTH are hidden, use the visible root, not stale transforms.
    widget_anchor.area = {500, 300, 610, 450};
    widget_occupied.flags &= ~UTFWin::kWinFlagVisible;
    widget_empty.flags = UTFWin::kWinFlagVisible;
    widget_empty.area = {5, 20, 105, 45};
    draw_portrait_pager(&panel);
    assert(widget_windows[3].area.bottom == 320 && widget_windows[3].area.right == 605);
    widget_empty.flags = 0;
    draw_portrait_pager(&panel);
    assert(widget_windows[3].area.bottom == 300 && widget_windows[3].area.right == 610);
    widget_occupied.flags |= UTFWin::kWinFlagVisible;

    // A missing placeholder affects only anchoring, not the owned texture.
    widget_row_available = false;
    draw_portrait_pager(&panel);
    assert(widget_windows[3].drawable == first_background && widget_atlas_loads == 1);
    assert(widget_windows[3].area.right - widget_windows[3].area.left == 92);
    assert(widget_windows[1].area.right - widget_windows[1].area.left == 44);
    assert(widget_windows[0].fill == 0);
    widget_row_available = true;
    draw_portrait_pager(&panel);
    portrait_pages.count = 4; portrait_pages.page = 0; draw_portrait_pager(&panel);
    for (auto& w : widget_windows) assert(!(w.flags & UTFWin::kWinFlagVisible));
    dispose_portrait_pager();
    assert(!widget_windows[3].drawable && !pager_background_attempted);
    assert(widget_attached == 0 && pager_ui_panel_identity == 0);
    for (unsigned i=0; i<4; ++i) assert(!widget_buttons[i].refs && !widget_windows[i].refs);
    // Missing/replaced atlas: readable flat fallback, no retry every frame.
    widget_created = 0; widget_atlas_available = false;
    assert(ensure_portrait_pager(&panel));
    portrait_pages.count = 5;
    draw_portrait_pager(&panel); draw_portrait_pager(&panel);
    assert(!widget_windows[3].drawable && widget_atlas_loads == 2);
    assert(widget_windows[0].fill == 0xe04c4734u);
    dispose_portrait_pager();
    widget_atlas_available = true;
    // A partial allocation failure must leave no attached control or listener.
    for (unsigned failure : {1u, 3u}) {
        widget_created = 0; widget_fail_at = failure;
        assert(!ensure_portrait_pager(&panel));
        assert(widget_attached == 0 && pager_ui_panel_identity == 0);
        for (unsigned i=0; i<4; ++i) {
            assert(!widget_buttons[i].refs && !widget_windows[i].refs);
            assert(!widget_windows[i].listener);
        }
    }
    widget_fail_at = UINT32_MAX;
    create_portrait_button = old_create; native_find_portrait_window = old_find;
    portrait_comm_screen_active = old_talking;
    load_pager_atlas = old_load_atlas;
    widget_atlas = nullptr;
    portrait_pages = {}; portrait_reuse_ready = false;
    puts("Production pager widget setup, hidden first frame, reuse and cleanup checks passed.");
}
