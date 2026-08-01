#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    const char* last_action;
} NotificationExampleApp;

static const NotificationSequence sequence_eat = {
    &message_note_c7, // 发送一个C7音符
    &message_delay_50,
    &message_sound_off, // 关闭声音
    NULL,
};

static void notification_example_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    NotificationExampleApp* app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Notification Demo");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "OK: custom sound");
    canvas_draw_str(canvas, 2, 34, "UP: success  DOWN: error");
    canvas_draw_str(canvas, 2, 44, "LEFT: blue  RIGHT: vibro");
    canvas_draw_str(canvas, 2, 54, "BACK: exit");
    canvas_draw_str(canvas, 2, 64, app->last_action);
}

static void notification_example_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static NotificationExampleApp* notification_example_app_alloc(void) {
    NotificationExampleApp* app = malloc(sizeof(NotificationExampleApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->last_action = "Last: ready";

    view_port_draw_callback_set(
        app->view_port, notification_example_draw_callback, app);
    view_port_input_callback_set(
        app->view_port, notification_example_input_callback, app->event_queue);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Keep the screen awake first, then send a non-blocking custom sequence.
    notification_message_block(app->notification, &sequence_display_backlight_enforce_on);
    notification_message(app->notification, &sequence_eat);
    app->last_action = "Last: startup demo";

    return app;
}

static void notification_example_app_free(NotificationExampleApp* app) {
    furi_assert(app);

    notification_message(app->notification, &sequence_reset_sound);
    notification_message(app->notification, &sequence_reset_vibro);
    notification_message(app->notification, &sequence_reset_rgb);
    notification_message_block(app->notification, &sequence_display_backlight_enforce_auto);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

static void notification_example_handle_short_input(
    NotificationExampleApp* app,
    const InputEvent* event) {
    switch(event->key) {
    case InputKeyOk:
        notification_message(app->notification, &sequence_eat);
        app->last_action = "Last: custom sound";
        break;
    case InputKeyUp:
        notification_message(app->notification, &sequence_success);
        app->last_action = "Last: success";
        break;
    case InputKeyDown:
        notification_message(app->notification, &sequence_error);
        app->last_action = "Last: error";
        break;
    case InputKeyLeft:
        notification_message(app->notification, &sequence_blink_blue_100);
        app->last_action = "Last: blue blink";
        break;
    case InputKeyRight:
        notification_message(app->notification, &sequence_single_vibro);
        app->last_action = "Last: vibro";
        break;
    default:
        return;
    }

    view_port_update(app->view_port);
}

int32_t notification_example_app(void* p) {
    UNUSED(p);

    NotificationExampleApp* app = notification_example_app_alloc();
    InputEvent event;
    bool running = true;

    view_port_update(app->view_port);

    while(running &&
          (furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk)) {
        if((event.type == InputTypeShort) && (event.key == InputKeyBack)) {
            running = false;
        } else if(event.type == InputTypeShort) {
            notification_example_handle_short_input(app, &event);
        }
    }

    notification_example_app_free(app);

    return 0;
}
