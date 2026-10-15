# 构建与开发环境

## 单独构建集成测试

需要 Windows、支持当前 C++23 代码的 MSVC 工具链以及 CMake。已验证 MSVC 14.51、CMake 4.3.1。以下命令从仓库根目录运行；`cmake` 和 `ctest` 应在 PATH 中。

```powershell
cmake -S native/integration -B build/integration -A x64
cmake --build build/integration --config Release --target integration_service_fixture integration_tests --parallel 4
ctest --test-dir build/integration -C Release --output-on-failure
```

这会运行 6 个集成场景。生成的 `integration_service_fixture.dll` 只是测试替身，不能安装到游戏，也不能替代正式服务插件。

## 完整插件和 13 组单元/组件测试

现有 CMake 文件通过 `WORKSPACE` 参数引用外部依赖工作区。当前结构包括：

```text
<dependency-workspace>/
  buy-displayed-sp-autopay-20261009/deps/
    commonlib-prebuilt/commonlibsse-ng-prebuilt-v11.0.0-all-msvc-cmake/
    CommonLibSSE-NG/  fmt/  spdlog/  rapidcsv/
    DirectXTK/  DirectXMath/
  food-gallery-20261010/
    deps/MeshRenderingFramework-source/QTR-Modding-Mesh-Rendering-Framework-c92dd6a/
    deps/nifly/Thiago099-nifly-cca0a77/
    deps/DirectXTex/microsoft-DirectXTex-74f9b02/
    build/  # 已编译的 food_renderer、nifly、DirectXTex、fmt、spdlog、dxmath
  inn-ordering-core-20261010/preview/imgui/
```

MinHook 1.3.3 的原版源码已随仓库保存在 `native/core/third_party/minhook/`，由 `PUACMinHook` 静态目标构建，无须外部 MinHook 头文件或预编译库。

这些路径对应当前开发快照；本仓库没有下载或重建所有外部依赖的自动脚本。设置 `WORKSPACE` 不会自动获取缺失依赖。菜单 API、CommonLib 布局宏及预编译库版本必须一致，不能随意替换。

在依赖已经准备好的 Windows 开发机器上：

```powershell
# 把这个路径换成上述外部依赖所在的工作区。
$dependencyWorkspace = 'C:/iniRePather-Skyrim'

cmake -S native/core -B build/core -A x64 "-DWORKSPACE=$dependencyWorkspace"
cmake -S native/service -B build/service -A x64 "-DWORKSPACE=$dependencyWorkspace"

cmake --build build/core --config Release --target PullUpAChairOrdering PullUpAChairEAS --parallel 4
cmake --build build/service --config Release --target PullUpAChairService --parallel 4

cmake --build build/core --config Release --target menu_typeface_tests inn_routine_tests scene_reservation_tests service_scene_tests core_tests adapter_tests food_pack_tests supplier_tests call_tests service_bridge_tests serving_size_tests settings_store_tests --parallel 4
cmake --build build/service --config Release --target service_tests --parallel 4

ctest --test-dir build/core -C Release --output-on-failure
ctest --test-dir build/service -C Release --output-on-failure
```

中文菜单字体测试会使用仓库内的 `payload/SKSE/Plugins/Fonts/PUAC-WenKai-Medium.ttf`。插件默认使用 Release；离线测试可独立使用 Debug，正式插件的 Debug 构建还涉及外部预编译库的运行时匹配，不作为已验证路径。

## 桌面进食模块迁移

`DiningAPI.h` 是核心对可选交互模块的进程内 C 接口，跨 DLL 仅传固定宽度编号、结构及函数指针。`DiningClaims.h` 由核心维护食物引用和基础物品的独占票据；读档清空活动票据但不重置序号。核心不安装食物／餐盘虚函数钩子，也不调用 EquipObject；这些入口由 `adapter/src/main.cpp` 安装。

`dining_tests` 覆盖接口缺失、重复／嵌套操作、成功、失败、异常及旧票据。`dining_core_tests` 直接编译生产 `TableManager.cpp` 中 Query/Begin/Finish/API 函数，在引擎边界替身上检查真实的库存变更、订单提交和回滚。两者均不能替代游戏内验证。

本轮完整发布构建基于已发布 0.9.9 的独立源码树 `C:/iniRePather-Skyrim/eating-module-20261015/native/core`，避免用仓库中较早的菜单核心回退已发布功能；迁移涉及的公共文件和适配器同步回本仓库。

## 本机 VS Code

原开发机器的本地设置和预设已经放在工作副本中，Git 会忽略以下文件：

- `.vscode/settings.json`：编译器、CMake、格式化器和 Git 的本机路径。
- `native/*/CMakeUserPresets.json`：本机 Visual Studio 实例、依赖和构建预设。
- `native/.build-vscode/`：构建输出。

因此，其他电脑 clone 后需要自行配置这些本机文件，或先使用上面的 CMake 命令。共享的 `.vscode/tasks.json` 和 `launch.json` 依赖本机预设，不能在尚未配置预设时直接使用。

原开发机器的快捷入口：Ctrl+Shift+B 编译插件；运行任务菜单启动测试；F5 调试核心测试或指定集成场景。默认任务没有安装或部署步骤。

## Papyrus、ESP 和发布资源

`native/scripts/` 保存 Papyrus 源码，编译仍需游戏/CK 的编译器与对应依赖脚本。`native/core/tools/build_service_package.py` 保存旧工作区中的 ESP 构建工具，它另外依赖 `inspect_records`、`inspect_plugins` 等本地工具和合法的游戏数据，并不是独立可运行脚本。

本仓库不包含完整发布包的 ESP、模型、贴图、音频和已编译脚本。不要将整个源码仓库直接当作 MO2 模组安装。
