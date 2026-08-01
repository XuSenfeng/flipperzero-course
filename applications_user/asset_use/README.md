# asset_use

这个示例演示了 Flipper Zero 外部应用里最常见的几类资源文件怎么使用：

- 应用菜单图标：`asset_use.png`
- 编译期静态图片资源：`images/AssetDolphin_71x25.png`
- 编译期动画资源：`images/AssetAlarm_47x39/`
- 运行时文件资源：`files/docs/readme.txt`、`files/config/demo.ini`

## 目录结构

```text
asset_use/
├── application.fam
├── asset_use.c
├── asset_use.png
├── images/
│   ├── AssetDolphin_71x25.png
│   └── AssetAlarm_47x39/
│       ├── frame_0.png
│       ├── frame_1.png
│       ├── frame_2.png
│       ├── frame_3.png
│       ├── frame_4.png
│       └── frame_rate
└── files/
    ├── docs/readme.txt
    └── config/demo.ini
```

## 1. 应用菜单图标 `asset_use.png`

这个文件由 `application.fam` 里的 `fap_icon` 指定：

```python
fap_icon="asset_use.png"
```

它的作用是：

- 显示在 Flipper 的 App 列表里
- 这是 FAP 包本身的图标，不会生成到 `asset_use_icons.h`

注意：

- 必须是 `10x10` 的 `1-bit PNG`
- 它和 `images/` 目录里的资源不是一回事

## 2. 静态图片资源 `images/AssetDolphin_71x25.png`

这类资源由 `application.fam` 里的 `fap_icon_assets="images"` 开启：

```python
fap_icon_assets="images"
```

构建后，`fbt` 会自动生成：

- `asset_use_icons.h`
- `asset_use_icons.c`

并把图片转换成 `Icon` 符号：

```c
extern const Icon I_AssetDolphin_71x25;
```

在代码里这样使用：

```c
#include <asset_use_icons.h>
canvas_draw_icon(canvas, 4, 18, &I_AssetDolphin_71x25);
```

本项目里的对应实现：

- 资源文件：`images/AssetDolphin_71x25.png`
- 使用代码：`asset_use_prepare_icon_demo()`

命名规则：

- 单张图片文件名会生成 `I_` 前缀
- 例如 `AssetDolphin_71x25.png` -> `I_AssetDolphin_71x25`

## 3. 动画资源 `images/AssetAlarm_47x39/`

动画资源不是一个单独的 png，而是一个目录。

目录里需要有：

- 多帧图片：`frame_0.png`、`frame_1.png` ...
- 帧率文件：`frame_rate`

例如本项目：

- `images/AssetAlarm_47x39/frame_0.png`
- `images/AssetAlarm_47x39/frame_1.png`
- `images/AssetAlarm_47x39/frame_2.png`
- `images/AssetAlarm_47x39/frame_3.png`
- `images/AssetAlarm_47x39/frame_4.png`
- `images/AssetAlarm_47x39/frame_rate`

构建后会生成动画 `Icon` 符号：

```c
extern const Icon A_AssetAlarm_47x39;
```

可以看到，动画资源会生成 `A_` 前缀。

在代码里通常这样使用：

```c
IconAnimation* animation = icon_animation_alloc(&A_AssetAlarm_47x39);
view_tie_icon_animation(view, animation);
canvas_draw_icon_animation(canvas, 40, 20, animation);
icon_animation_start(animation);
```

如果只想记住“这套动画怎么跑起来”，最简代码可以整理成下面这样：

```c
typedef struct {
    IconAnimation* animation;
    bool running;
} DemoAnimModel;

static void demo_anim_draw(Canvas* canvas, void* model_) {
    DemoAnimModel* model = model_;

    canvas_clear(canvas);
    canvas_draw_icon_animation(canvas, 40, 20, model->animation);
}

static bool demo_anim_input(InputEvent* event, void* context) {
    App* app = context;

    if((event->type != InputTypeShort) || (event->key != InputKeyOk)) {
        return false;
    }

    bool should_start = false;
    with_view_model(
        app->animation_view,
        DemoAnimModel * model,
        {
            model->running = !model->running;
            should_start = model->running;
        },
        true);

    if(should_start) {
        icon_animation_start(app->animation);
    } else {
        icon_animation_stop(app->animation);
    }

    return true;
}

static void demo_anim_enter(void* context) {
    App* app = context;

    with_view_model(
        app->animation_view, DemoAnimModel * model, { model->running = true; }, true);
    icon_animation_start(app->animation);
}

static void demo_anim_exit(void* context) {
    App* app = context;

    icon_animation_stop(app->animation);
    with_view_model(
        app->animation_view, DemoAnimModel * model, { model->running = false; }, false);
}

static void demo_anim_init(App* app) {
    app->animation_view = view_alloc();
    app->animation = icon_animation_alloc(&A_AssetAlarm_47x39);

    view_allocate_model(app->animation_view, ViewModelTypeLocking, sizeof(DemoAnimModel));
    with_view_model(
        app->animation_view,
        DemoAnimModel * model,
        {
            model->animation = app->animation;
            model->running = false;
        },
        false);

    view_set_context(app->animation_view, app);
    view_set_draw_callback(app->animation_view, demo_anim_draw);
    view_set_input_callback(app->animation_view, demo_anim_input);
    view_set_enter_callback(app->animation_view, demo_anim_enter);
    view_set_exit_callback(app->animation_view, demo_anim_exit);
    view_tie_icon_animation(app->animation_view, app->animation);
}
```

这段最简代码的职责划分是：

- `IconAnimation` 负责“当前播到第几帧”
- `View` 负责“这个页面怎么画、怎么响应按键”
- `Model` 负责“这个页面当前状态是什么”，这里主要是 `running`
- `view_tie_icon_animation()` 负责把“动画帧变化”和“页面刷新”连起来

按运行顺序看，这套流程是：

1. `icon_animation_alloc()` 创建动画对象。
2. `view_allocate_model()` 给动画页面分配 model。
3. `view_set_draw_callback()` 注册绘制函数，里面用 `canvas_draw_icon_animation()` 画当前帧。
4. `view_tie_icon_animation()` 把动画对象绑定到 view，动画每跳一帧就会触发页面刷新。
5. 切到这个页面时，`enter callback` 调用 `icon_animation_start()`，定时器开始推进帧。
6. 定时器每 tick 一次，动画切到下一帧，view 收到刷新通知后重新 draw。
7. 按 `OK` 时切换 `running`，再决定调用 `icon_animation_start()` 还是 `icon_animation_stop()`。
8. 离开页面时，`exit callback` 停止动画并把状态复位。

可以把它记成一句话：

```text
资源生成 A_XXX -> icon_animation_alloc -> draw 里画当前帧
-> view_tie_icon_animation 负责自动刷新 -> start/stop 控制播放
```

本项目里的对应实现：

- 动画分配：`asset_use_alloc()`
- 动画绘制：`asset_use_animation_draw()`
- 开始/停止：`asset_use_animation_enter()`、`asset_use_animation_exit()`、`asset_use_animation_input()`

适用场景：

- 加载动画
- 状态指示动画
- 小型 UI 动效

## 4. 文件资源 `files/...`

这类资源由 `application.fam` 里的 `fap_file_assets="files"` 开启：

```python
fap_file_assets="files"
```

它和 `fap_icon_assets` 的区别是：

- `fap_icon_assets`：把图片编译进代码，变成 `Icon` 符号
- `fap_file_assets`：把文件打进 FAP，运行时解包到 app 的资源目录

运行时访问时不要写死路径，推荐用：

```c
APP_ASSETS_PATH("docs/readme.txt")
APP_ASSETS_PATH("config/demo.ini")
```

本项目里就是这样读取的：

```c
asset_use_append_file(storage, app->file_text, APP_ASSETS_PATH("docs/readme.txt"), ...);
asset_use_append_file(storage, app->file_text, APP_ASSETS_PATH("config/demo.ini"), ...);
```

本项目里的对应文件：

- `files/docs/readme.txt`
- `files/config/demo.ini`

适用场景：

- 配置文件
- 文本数据库
- 关卡数据
- 映射表
- 规则文件

## 5. 这个示例里每类资源对应到哪里

- `asset_use.png`
  - 用途：应用在菜单中的图标
  - 配置位置：`application.fam` 的 `fap_icon`

- `images/AssetDolphin_71x25.png`
  - 用途：静态 `Icon`
  - 生成符号：`I_AssetDolphin_71x25`
  - 使用位置：`asset_use_prepare_icon_demo()`

- `images/AssetAlarm_47x39/`
  - 用途：动画 `Icon`
  - 生成符号：`A_AssetAlarm_47x39`
  - 使用位置：`asset_use_alloc()`、`asset_use_animation_draw()`

- `files/docs/readme.txt`
  - 用途：运行时读取的文本资源
  - 使用位置：`asset_use_prepare_files_demo()`

- `files/config/demo.ini`
  - 用途：运行时读取的配置资源
  - 使用位置：`asset_use_prepare_files_demo()`

## 6. 什么时候该用哪一种

- 只是想在屏幕上画一张图：用 `fap_icon_assets`
- 想播放一个小动画：用 `fap_icon_assets` + 动画目录
- 想随应用附带文本、配置、数据文件：用 `fap_file_assets`
- 想设置应用在菜单里的图标：用 `fap_icon`

## 7. 新增资源时的建议

- 静态图片命名尽量带尺寸，例如 `MyLogo_32x32.png`
- 动画目录命名也带尺寸，例如 `Loading_24x24/`
- 文件资源按用途分目录，例如 `files/config/`、`files/data/`、`files/docs/`
- 代码里统一通过 `APP_ASSETS_PATH(...)` 访问文件资源

## 8. 构建

构建这个示例：

```bash
./fbt build APPSRC=applications_user/asset_use
```

如果一切正常：

- `images/` 会生成 `asset_use_icons.h`
- `files/` 会被打包进 `.fap`
- 运行时可以在 app 中看到三种资源的演示
