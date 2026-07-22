#include "../gpio_app_i.h"

void gpio_scene_test_ok_callback(InputType type, void* context) {
    furi_assert(context);
    GpioApp* app = context;

    if(type == InputTypePress) {
        // 设置flipperzero的LED为绿色, 以指示GPIO测试模式已启动
        // LED/震动/蜂鸣器由通知服务统一管理,避免多个 app 抢硬件
        notification_message(app->notifications, &sequence_set_green_255);
    } else if(type == InputTypeRelease) {
        notification_message(app->notifications, &sequence_reset_green);
    }
}

void gpio_scene_test_on_enter(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_pins(app->gpio_items, GpioModeOutputPushPull);
    // 设置GPIO测试视图的OK键回调函数
    gpio_test_set_ok_callback(app->gpio_test, gpio_scene_test_ok_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GpioAppViewGpioTest);
}

bool gpio_scene_test_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gpio_scene_test_on_exit(void* context) {
    furi_assert(context);
    GpioApp* app = context;
    gpio_items_configure_all_pins(app->gpio_items, GpioModeAnalog);
}
