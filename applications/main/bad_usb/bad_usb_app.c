#include "bad_usb_app_i.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <flipper_format/flipper_format.h>

#define BAD_USB_SETTINGS_PATH           BAD_USB_APP_BASE_FOLDER "/.badusb.settings"
#define BAD_USB_SETTINGS_FILE_TYPE      "Flipper BadUSB Settings File"
#define BAD_USB_SETTINGS_VERSION        1
#define BAD_USB_SETTINGS_DEFAULT_LAYOUT BAD_USB_APP_PATH_LAYOUT_FOLDER "/en-US.kl"

static bool bad_usb_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bad_usb_app_back_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// 处理定时器事件, 每500ms触发一次, 用于刷新界面和处理其他定时任务
// 这里调用了scene_manager_handle_tick_event函数, 该函数会根据当前场景的状态, 
// 调用相应的tick事件处理函数
static void bad_usb_app_tick_event_callback(void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

// 读取配置文件, 获取键盘布局和接口类型, 如果配置文件不存在则使用默认值
static void bad_usb_load_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool state = false;

    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;
    uint32_t interface = 0;

    if(flipper_format_file_open_existing(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_read_header(fff, temp_str, &version)) break;
            if((strcmp(furi_string_get_cstr(temp_str), BAD_USB_SETTINGS_FILE_TYPE) != 0) ||
               (version != BAD_USB_SETTINGS_VERSION))
                break;

            if(!flipper_format_read_string(fff, "layout", temp_str)) break;
            if(!flipper_format_read_uint32(fff, "interface", &interface, 1)) break;
            if(interface > BadUsbHidInterfaceBle) break;

            state = true; // 读取配置文件成功
        } while(0);
    }
    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);

    // 如果读取配置文件成功, 则将键盘布局和接口类型保存到app结构体中, 否则使用默认值
    if(state) {
        furi_string_set(app->keyboard_layout, temp_str);
        app->interface = interface;

        Storage* fs_api = furi_record_open(RECORD_STORAGE);
        FileInfo layout_file_info;
        FS_Error file_check_err = storage_common_stat(
            fs_api, furi_string_get_cstr(app->keyboard_layout), &layout_file_info);
        furi_record_close(RECORD_STORAGE);
        if((file_check_err != FSE_OK) || (layout_file_info.size != 256)) {
            furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
        }
    } else {
        furi_string_set(app->keyboard_layout, BAD_USB_SETTINGS_DEFAULT_LAYOUT);
        app->interface = BadUsbHidInterfaceUsb;
    }

    furi_string_free(temp_str);
}

static void bad_usb_save_settings(BadUsbApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_always(fff, BAD_USB_SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(
                   fff, BAD_USB_SETTINGS_FILE_TYPE, BAD_USB_SETTINGS_VERSION))
                break;
            if(!flipper_format_write_string(fff, "layout", app->keyboard_layout)) break;
            uint32_t interface_id = app->interface;
            if(!flipper_format_write_uint32(fff, "interface", (const uint32_t*)&interface_id, 1))
                break;
        } while(0);
    }

    flipper_format_free(fff);
    furi_record_close(RECORD_STORAGE);
}

// 设置接口类型, 并更新视图中的接口显示, USB或BLE
void bad_usb_set_interface(BadUsbApp* app, BadUsbHidInterface interface) {
    app->interface = interface;
    bad_usb_view_set_interface(app->bad_usb_view, interface);
}

BadUsbApp* bad_usb_app_alloc(char* arg) {
    BadUsbApp* app = malloc(sizeof(BadUsbApp));

    app->bad_usb_script = NULL;

    app->file_path = furi_string_alloc();
    app->keyboard_layout = furi_string_alloc();
    if(arg && strlen(arg)) {
        furi_string_set(app->file_path, arg);
    }

    bad_usb_load_settings(app);

    // 开启GUI, 通知和对话框服务
    // GUI服务用于显示界面, 通知服务用于显示通知、控制震动灯光等, 对话框服务用于弹出对话框
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // 申请视图调度器和场景管理器, 用于管理不同的界面和场景
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&bad_usb_scene_handlers, app);

    // 设置视图调度器的回调函数, 用于处理定时器事件、用户自定义事件和返回事件
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bad_usb_app_tick_event_callback, 500);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bad_usb_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bad_usb_app_back_event_callback);

    // 创建不同的视图, 包括自定义控件视图、弹出窗口视图、工作视图和配置视图
    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWidget, widget_get_view(app->widget));

    // Popup
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BadUsbAppViewPopup, popup_get_view(app->popup));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadUsbAppViewConfig,
        variable_item_list_get_view(app->var_item_list));
    // 打开脚本后看到的主界面
    app->bad_usb_view = bad_usb_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadUsbAppViewWork, bad_usb_view_get_view(app->bad_usb_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(furi_hal_usb_is_locked()) {
        // 如果USB接口被锁定, 则无法使用BadUSB功能, 显示错误界面
        app->error = BadUsbAppErrorCloseRpc;
        app->usb_if_prev = NULL;
        scene_manager_next_scene(app->scene_manager, BadUsbSceneError);
    } else {
        // 如果USB接口未被锁定, 则可以使用BadUSB功能, 先保存当前的USB配置, 然后禁用USB接口,
        // 以便BadUSB可以模拟键盘和鼠标, 然后根据是否有脚本文件路径来决定进入工作场景还是文件选择场景
        app->usb_if_prev = furi_hal_usb_get_config();
        furi_check(furi_hal_usb_set_config(NULL, NULL));

        if(!furi_string_empty(app->file_path)) {
            scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
        } else {
            furi_string_set(app->file_path, BAD_USB_APP_BASE_FOLDER);
            scene_manager_next_scene(app->scene_manager, BadUsbSceneFileSelect);
        }
    }

    return app;
}

void bad_usb_app_free(BadUsbApp* app) {
    furi_assert(app);

    if(app->bad_usb_script) {
        bad_usb_script_close(app->bad_usb_script);
        app->bad_usb_script = NULL;
    }

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWork);
    bad_usb_view_free(app->bad_usb_view);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewWidget);
    widget_free(app->widget);

    // Popup
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewPopup);
    popup_free(app->popup);

    // Config menu
    view_dispatcher_remove_view(app->view_dispatcher, BadUsbAppViewConfig);
    variable_item_list_free(app->var_item_list);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);

    bad_usb_save_settings(app);

    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout);

    if(app->usb_if_prev) {
        furi_check(furi_hal_usb_set_config(app->usb_if_prev, NULL));
    }

    free(app);
}

// 入口函数, 参数是启动的脚本文件的路径, 如果没有则为NULL
int32_t bad_usb_app(void* p) {
    BadUsbApp* bad_usb_app = bad_usb_app_alloc((char*)p);

    view_dispatcher_run(bad_usb_app->view_dispatcher);

    bad_usb_app_free(bad_usb_app);
    return 0;
}
