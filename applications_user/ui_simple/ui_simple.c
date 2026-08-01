/**
 * @file ui_simple.c
 * @brief 一个演示 Flipper Zero GUI 模块的示例 App。
 *
 * 使用 ViewDispatcher 管理多个 View，通过一个主菜单 (Submenu) 在
 * 各 GUI 组件演示页之间切换。演示的组件来自 temp.md：
 *
 *   - Submenu          : 主菜单（垂直列表 + 标题）
 *   - Widget           : 多功能容器（文字 / 边框 / 按钮）
 *   - Popup            : 提示信息 + 超时自动返回
 *   - VariableItemList : 配置菜单（每项可左右切换多个取值）
 *   - TextInput        : 文本输入
 *   - ButtonMenu       : 大按钮式垂直菜单
 *   - Menu             : 系统主菜单样式（带图标）
 *   - NumberInput      : 数字输入
 *   - ByteInput        : 16 进制字节输入
 *   - DialogEx         : 带 2~3 个按钮的对话框
 *   - TextBox          : 可滚动的长文本
 *   - Loading          : 加载动画
 *   - EmptyScreen      : 空白屏幕
 *
 * 未包含的两个组件（需要额外资源，这里仅作说明）：
 *   - ButtonPanel : 每个按钮项都必须提供图标，images 文件夹为空时无法显示。
 *   - FileBrowser : 依赖 RECORD_STORAGE 与 FuriString 路径，逻辑较复杂。
 *
 * 在任意子页面按 Back 返回主菜单；在主菜单按 Back 退出 App。
 * 所有界面显示文字使用英文——Flipper 默认字体不含中文字形。
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/menu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/loading.h>
#include <gui/modules/empty_screen.h>

/* fbt 根据 images 文件夹里的 .png 自动生成 */
#include <ui_simple_icons.h>

// 每个 View 在 ViewDispatcher 里的索引
typedef enum {
    UiSimpleViewMenu, // 主菜单 (Submenu)
    UiSimpleViewWidget,
    UiSimpleViewPopup,
    UiSimpleViewConfig, // VariableItemList
    UiSimpleViewTextInput,
    UiSimpleViewButtonMenu,
    UiSimpleViewSysMenu, // Menu
    UiSimpleViewNumberInput,
    UiSimpleViewByteInput,
    UiSimpleViewDialog, // DialogEx
    UiSimpleViewTextBox,
    UiSimpleViewLoading,
    UiSimpleViewEmpty, // EmptyScreen
} UiSimpleView;

// 主菜单里各选项的索引
typedef enum {
    UiSimpleMenuWidget,
    UiSimpleMenuPopup,
    UiSimpleMenuConfig,
    UiSimpleMenuTextInput,
    UiSimpleMenuButtonMenu,
    UiSimpleMenuSysMenu,
    UiSimpleMenuNumberInput,
    UiSimpleMenuByteInput,
    UiSimpleMenuDialog,
    UiSimpleMenuTextBox,
    UiSimpleMenuLoading,
    UiSimpleMenuEmpty,
} UiSimpleMenuIndex;

// App 主结构：持有所有 View 模块与共享状态
typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    VariableItemList* config;
    TextInput* text_input;
    ButtonMenu* button_menu;
    Menu* menu;
    NumberInput* number_input;
    ByteInput* byte_input;
    DialogEx* dialog;
    TextBox* text_box;
    Loading* loading;
    EmptyScreen* empty_screen;

    char name_buffer[32]; // TextInput 缓冲区
    uint8_t brightness_index; // Config 页亮度索引
    int32_t number_value; // NumberInput 结果
    uint8_t byte_buffer[4]; // ByteInput 缓冲区
    UiSimpleView current_view; // 当前 View（ViewDispatcher 未提供查询接口）
} UiSimpleApp;

// VariableItemList 里“亮度”一项的可选取值
static const char* const brightness_labels[] = {"Low", "Mid", "High"};

// 切换 View 的同时记录当前 View，供 Back 导航判断使用。
static void ui_simple_switch_view(UiSimpleApp* app, UiSimpleView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

/* -------------------------------------------------------------------------- */
/*                                 导航回调                                     */
/* -------------------------------------------------------------------------- */

// 用户在任意 View 里按 Back 且该 View 自身未处理时被调用。
// 只有主菜单会退出 App，其它子页面统一返回主菜单。
static bool ui_simple_navigation_callback(void* context) {
    furi_assert(context);
    UiSimpleApp* app = context;

    if(app->current_view == UiSimpleViewMenu) {
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        ui_simple_switch_view(app, UiSimpleViewMenu);
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                              主菜单 (Submenu)                                */
/* -------------------------------------------------------------------------- */

// 主菜单选项被选中时触发。菜单索引直接映射到目标 View 索引。
static void ui_simple_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    UiSimpleApp* app = context;

    // 菜单索引 → View 索引的映射表
    static const UiSimpleView menu_to_view[] = {
        [UiSimpleMenuWidget] = UiSimpleViewWidget,
        [UiSimpleMenuPopup] = UiSimpleViewPopup,
        [UiSimpleMenuConfig] = UiSimpleViewConfig,
        [UiSimpleMenuTextInput] = UiSimpleViewTextInput,
        [UiSimpleMenuButtonMenu] = UiSimpleViewButtonMenu,
        [UiSimpleMenuSysMenu] = UiSimpleViewSysMenu,
        [UiSimpleMenuNumberInput] = UiSimpleViewNumberInput,
        [UiSimpleMenuByteInput] = UiSimpleViewByteInput,
        [UiSimpleMenuDialog] = UiSimpleViewDialog,
        [UiSimpleMenuTextBox] = UiSimpleViewTextBox,
        [UiSimpleMenuLoading] = UiSimpleViewLoading,
        [UiSimpleMenuEmpty] = UiSimpleViewEmpty,
    };
    if(index < COUNT_OF(menu_to_view)) {
        ui_simple_switch_view(app, menu_to_view[index]);
    }
}

/* -------------------------------------------------------------------------- */
/*                                 Widget 演示                                  */
/* -------------------------------------------------------------------------- */

// Widget 页里 “To Popup” 按钮的回调。
static void ui_simple_widget_button_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    furi_assert(context);
    UiSimpleApp* app = context;
    if(button_type == GuiButtonTypeCenter && input_type == InputTypeShort) {
        ui_simple_switch_view(app, UiSimpleViewPopup);
    }
}

/* -------------------------------------------------------------------------- */
/*                                 Popup 演示                                   */
/* -------------------------------------------------------------------------- */

// Popup 超时后触发，自动返回主菜单。
static void ui_simple_popup_callback(void* context) {
    furi_assert(context);
    UiSimpleApp* app = context;
    ui_simple_switch_view(app, UiSimpleViewMenu);
}

/* -------------------------------------------------------------------------- */
/*                          VariableItemList 演示                              */
/* -------------------------------------------------------------------------- */

// “亮度”这一项的值被左右切换时触发，负责更新右侧显示文本。
static void ui_simple_brightness_changed(VariableItem* item) {
    UiSimpleApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->brightness_index = index;
    variable_item_set_current_value_text(item, brightness_labels[index]);
}

/* -------------------------------------------------------------------------- */
/*                                TextInput 演示                                */
/* -------------------------------------------------------------------------- */

// 用户在 TextInput 里按 Save 时触发：把输入结果显示在 Popup 上。
static void ui_simple_text_input_callback(void* context) {
    furi_assert(context);
    UiSimpleApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Hello", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(app->popup, app->name_buffer, 64, 40, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, ui_simple_popup_callback);
    popup_set_context(app->popup, app);

    ui_simple_switch_view(app, UiSimpleViewPopup);
}

/* -------------------------------------------------------------------------- */
/*                               ButtonMenu 演示                                */
/* -------------------------------------------------------------------------- */

// ButtonMenu 项被点击（短按 OK）时触发，返回主菜单。
static void ui_simple_button_menu_callback(void* context, int32_t index, InputType type) {
    furi_assert(context);
    UNUSED(index);
    UiSimpleApp* app = context;
    if(type == InputTypeShort) {
        ui_simple_switch_view(app, UiSimpleViewMenu);
    }
}

/* -------------------------------------------------------------------------- */
/*                                  Menu 演示                                   */
/* -------------------------------------------------------------------------- */

// Menu 项被选中时触发，返回主菜单。
static void ui_simple_menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    UNUSED(index);
    UiSimpleApp* app = context;
    ui_simple_switch_view(app, UiSimpleViewMenu);
}

/* -------------------------------------------------------------------------- */
/*                               NumberInput 演示                               */
/* -------------------------------------------------------------------------- */

// 用户按 Save 保存数字时触发，把结果显示在 Popup 上。
static void ui_simple_number_input_callback(void* context, int32_t number) {
    furi_assert(context);
    UiSimpleApp* app = context;
    app->number_value = number;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Number", 64, 20, AlignCenter, AlignCenter);
    // 数字转字符串后显示（复用 name_buffer 作临时缓冲）
    snprintf(app->name_buffer, sizeof(app->name_buffer), "%ld", (long)number);
    popup_set_text(app->popup, app->name_buffer, 64, 40, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, ui_simple_popup_callback);
    popup_set_context(app->popup, app);

    ui_simple_switch_view(app, UiSimpleViewPopup);
}

/* -------------------------------------------------------------------------- */
/*                               ByteInput 演示                                 */
/* -------------------------------------------------------------------------- */

// 用户按 Save 保存字节时触发，返回主菜单。
static void ui_simple_byte_input_callback(void* context) {
    furi_assert(context);
    UiSimpleApp* app = context;
    ui_simple_switch_view(app, UiSimpleViewMenu);
}

/* -------------------------------------------------------------------------- */
/*                                DialogEx 演示                                 */
/* -------------------------------------------------------------------------- */

// 对话框三个按钮的结果回调，统一返回主菜单。
static void ui_simple_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    UNUSED(result);
    UiSimpleApp* app = context;
    ui_simple_switch_view(app, UiSimpleViewMenu);
}

/* -------------------------------------------------------------------------- */
/*                             构造 / 析构 / 运行                               */
/* -------------------------------------------------------------------------- */

static UiSimpleApp* ui_simple_app_alloc(void) {
    UiSimpleApp* app = malloc(sizeof(UiSimpleApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    // ---- ViewDispatcher ----
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ui_simple_navigation_callback);

    // ---- 主菜单 (Submenu) ----
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "UI Components");
    submenu_add_item(app->submenu, "Widget", UiSimpleMenuWidget, ui_simple_submenu_callback, app);
    submenu_add_item(app->submenu, "Popup", UiSimpleMenuPopup, ui_simple_submenu_callback, app);
    submenu_add_item(app->submenu, "Config", UiSimpleMenuConfig, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Text Input", UiSimpleMenuTextInput, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Button Menu", UiSimpleMenuButtonMenu, ui_simple_submenu_callback, app);
    submenu_add_item(app->submenu, "Menu", UiSimpleMenuSysMenu, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Number Input", UiSimpleMenuNumberInput, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Byte Input", UiSimpleMenuByteInput, ui_simple_submenu_callback, app);
    submenu_add_item(app->submenu, "Dialog", UiSimpleMenuDialog, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Text Box", UiSimpleMenuTextBox, ui_simple_submenu_callback, app);
    submenu_add_item(app->submenu, "Loading", UiSimpleMenuLoading, ui_simple_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Empty Screen", UiSimpleMenuEmpty, ui_simple_submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewMenu, submenu_get_view(app->submenu));

    // ---- Widget 演示：边框 + 文字 + 按钮 ----
    app->widget = widget_alloc();
    widget_add_frame_element(app->widget, 0, 0, 128, 52, 3);
    widget_add_string_element(
        app->widget, 64, 12, AlignCenter, AlignCenter, FontPrimary, "Widget Container");
    widget_add_string_multiline_element(
        app->widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Place text, icons,\nbuttons freely");
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "To Popup", ui_simple_widget_button_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewWidget, widget_get_view(app->widget));

    // ---- Popup 演示 ----
    app->popup = popup_alloc();
    popup_set_header(app->popup, "Popup", 64, 18, AlignCenter, AlignCenter);
    popup_set_text(app->popup, "Popup with icon\nand timeout", 64, 38, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 3000);
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, ui_simple_popup_callback);
    popup_set_context(app->popup, app);
    view_dispatcher_add_view(app->view_dispatcher, UiSimpleViewPopup, popup_get_view(app->popup));

    // ---- VariableItemList 演示 ----
    app->brightness_index = 1; // 默认 “Mid”
    app->config = variable_item_list_alloc();
    VariableItem* item = variable_item_list_add(
        app->config, "Brightness", COUNT_OF(brightness_labels), ui_simple_brightness_changed, app);
    variable_item_set_current_value_index(item, app->brightness_index);
    variable_item_set_current_value_text(item, brightness_labels[app->brightness_index]);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewConfig, variable_item_list_get_view(app->config));

    // ---- TextInput 演示 ----
    app->name_buffer[0] = '\0';
    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Enter your name");
    text_input_set_result_callback(
        app->text_input,
        ui_simple_text_input_callback,
        app,
        app->name_buffer,
        sizeof(app->name_buffer),
        true);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewTextInput, text_input_get_view(app->text_input));

    // ---- ButtonMenu 演示 ----
    app->button_menu = button_menu_alloc();
    button_menu_set_header(app->button_menu, "Button Menu");
    button_menu_add_item(
        app->button_menu, "Common", 0, ui_simple_button_menu_callback, ButtonMenuItemTypeCommon, app);
    button_menu_add_item(
        app->button_menu,
        "Control",
        1,
        ui_simple_button_menu_callback,
        ButtonMenuItemTypeControl,
        app);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewButtonMenu, button_menu_get_view(app->button_menu));

    // ---- Menu 演示（图标传 NULL，模块会用默认图标）----
    app->menu = menu_alloc();
    menu_add_item(app->menu, "Item 1", NULL, 0, ui_simple_menu_callback, app);
    menu_add_item(app->menu, "Item 2", NULL, 1, ui_simple_menu_callback, app);
    menu_add_item(app->menu, "Item 3", NULL, 2, ui_simple_menu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, UiSimpleViewSysMenu, menu_get_view(app->menu));

    // ---- NumberInput 演示 ----
    app->number_value = 42;
    app->number_input = number_input_alloc();
    number_input_set_header_text(app->number_input, "Enter a number");
    number_input_set_result_callback(
        app->number_input, ui_simple_number_input_callback, app, app->number_value, 0, 1000);
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewNumberInput, number_input_get_view(app->number_input));

    // ---- ByteInput 演示 ----
    memset(app->byte_buffer, 0, sizeof(app->byte_buffer));
    app->byte_input = byte_input_alloc();
    byte_input_set_header_text(app->byte_input, "Enter bytes (hex)");
    byte_input_set_result_callback(
        app->byte_input,
        ui_simple_byte_input_callback,
        NULL,
        app,
        app->byte_buffer,
        sizeof(app->byte_buffer));
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewByteInput, byte_input_get_view(app->byte_input));

    // ---- DialogEx 演示 ----
    app->dialog = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, ui_simple_dialog_callback);
    dialog_ex_set_header(app->dialog, "Dialog", 64, 8, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, "Pick a button below", 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog, "No");
    dialog_ex_set_center_button_text(app->dialog, "OK");
    dialog_ex_set_right_button_text(app->dialog, "Yes");
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewDialog, dialog_ex_get_view(app->dialog));

    // ---- TextBox 演示（长文本可滚动）----
    app->text_box = text_box_alloc();
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(
        app->text_box,
        "TextBox shows long scrollable text.\n"
        "Use Up/Down keys to scroll.\n\n"
        "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n"
        "Line 6\nLine 7\nLine 8\nLine 9\nLine 10\n"
        "-- end --");
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewTextBox, text_box_get_view(app->text_box));

    // ---- Loading 演示（旋转加载动画）----
    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewLoading, loading_get_view(app->loading));

    // ---- EmptyScreen 演示（纯空白）----
    app->empty_screen = empty_screen_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UiSimpleViewEmpty, empty_screen_get_view(app->empty_screen));

    return app;
}

static void ui_simple_app_free(UiSimpleApp* app) {
    // 删除 ViewDispatcher 前必须先移除所有 View
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewButtonMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewSysMenu);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, UiSimpleViewEmpty);
    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->submenu);
    widget_free(app->widget);
    popup_free(app->popup);
    variable_item_list_free(app->config);
    text_input_free(app->text_input);
    button_menu_free(app->button_menu);
    menu_free(app->menu);
    number_input_free(app->number_input);
    byte_input_free(app->byte_input);
    dialog_ex_free(app->dialog);
    text_box_free(app->text_box);
    loading_free(app->loading);
    empty_screen_free(app->empty_screen);

    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t ui_simple_app(void* p) {
    UNUSED(p);

    UiSimpleApp* app = ui_simple_app_alloc();
    // 先显示主菜单，然后阻塞运行事件循环，直到 view_dispatcher_stop()
    ui_simple_switch_view(app, UiSimpleViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    ui_simple_app_free(app);

    return 0;
}
