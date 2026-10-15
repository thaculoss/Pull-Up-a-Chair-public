# Pull Up a Chair

[中文](#中文) | [English](#english)

## 中文

Skyrim SE 点菜与旅馆用餐模组的源码仓库。

围绕坐在餐桌前的用餐流程组织功能：呼叫员工、浏览菜品和套餐、确认价格与供货、付款、上菜，以及与原生进食行为衔接。源码保留中文阅读注释和统一格式，便于逐步阅读和维护。

本仓库保存当前 C++ 核心、服务插件、EAS 适配器、Papyrus 脚本、菜单配置和离线测试。**它是源码工程，不是可以直接安装到游戏的完整发布包。** 游戏数据、其他模组、完整美术与音频资源、已编译插件及本机编译缓存不随本次源码提交上传。

EAS observer1 测试版通过独立适配器观察 EAS 原有施法、餐具动画命令和脚本状态，不再分发或覆盖三份 `aaaEAS*` 脚本。旧版原生通知接口只保留兼容空实现。Vortex 1.7.104 的 beef 单手路径已有独立探针对照证据；新无覆盖二进制及双手、直接食物效果路径仍需游戏验证。

本轮桌面进食模块迁移：食物激活／提示、原生食用结果捕获和餐盘交互钩子已从点餐核心移到 `PullUpAChairEAS.dll`。模块内的通用桌面进食不要求 EAS；EAS 动画观察由独立配置控制。核心通过 `DiningAPI.h` v1 维护一次性操作票据、库存失败回滚和订单状态，保留原存档记录格式。完整 FOMOD 提供“桌面进食 + EAS”“仅桌面进食”“仅点餐上菜”三种选项。游戏复测待完成；这轮迁移尚不表示已支持 VR。

### 从哪里开始看

| 目录或文件 | 内容 |
| --- | --- |
| [源码阅读索引](native/源码阅读索引.txt) | 各源文件的作用与阅读顺序 |
| `native/core/src/` | 点菜、报价、付款、菜单、餐桌及设置 |
| `native/core/adapter/src/` | 可选桌面进食、食物／餐盘交互与 EAS 动画衔接 |
| `native/service/src/` | 员工、供货和服务状态管理 |
| `native/scripts/` | Papyrus 脚本源码 |
| `native/core/config/` | 菜谱、旅馆、桌面规则、语言表和配置 |
| `native/core/tests/`、`native/service/tests/` | 13 组单元/组件测试 |
| `native/integration/` | 6 组离线集成测试及测试专用 DLL |
| `payload/` | 菜单字体及对应 OFL 许可证，供字体测试和菜单使用 |
| `docs/` | 构建、测试和第三方依赖说明 |

核心阅读入口：

- [OrderPolicy.h](native/core/src/OrderPolicy.h)：菜品来源、选择和报价规则。
- [Transaction.h](native/core/src/Transaction.h)：扣货、扣款及失败回滚。
- [main.cpp](native/core/src/main.cpp)：点餐插件入口和菜单会话。
- [ServiceManager.cpp](native/service/src/ServiceManager.cpp)：服务请求和员工状态。
- [integration_tests.cpp](native/integration/integration_tests.cpp)：把多个业务组件连接起来的测试场景。

### 开发与构建

本机可用 VS Code 打开 [PullUpAChair.code-workspace](PullUpAChair.code-workspace)。常用操作见 [VS Code 使用说明](native/VS%20Code%20使用说明.txt)。

修改后如何提交和上传，见 [Git 日常操作](docs/GIT.md)。

已验证的工具链为 Windows x64、MSVC 14.51、CMake 4.3 和 C++23。完整插件构建目前依赖原开发工作区中的预编译 CommonLib、渲染库及其他第三方依赖，尚未改造成任意电脑都能直接构建的独立工程。

**6 组集成测试不依赖游戏 SDK 或游戏安装，可以单独构建。** 命令及完整插件的依赖要求见 [构建说明](docs/BUILD.md)。

### 测试记录

2026-10-14 的已保存结果：

- 单元/组件测试：13 组通过，0 失败，0 跳过。
- 离线集成测试：6 组通过，0 失败，0 跳过。
- 定向变异复测：同一批 125 个变异全部被检测到；首轮漏检的 21 个变异已由补充测试捕获。

变异检出率只针对这批指定变异，不等于任意错误都能被发现。离线测试使用引擎边界替身，不能代替 Skyrim 内的 NPC 服务、摆放、UI 和进食动画验证。详见 [测试说明与记录](docs/TESTING.md)。

### 许可证和第三方代码

仓库尚未选择项目整体许可证。已经包含的第三方内容保留各自许可证：SkyPrompt API 为 MIT，文楷字体为 SIL OFL 1.1。完整插件的外部依赖及渲染库许可情况见 [第三方说明](docs/THIRD_PARTY.md)。

---

## English

The dining-module migration moves food activation/text, native consumption-result capture and plate interaction hooks into the optional `PullUpAChairEAS.dll`. Generic click-to-eat works without EAS; EAS observation has its own setting. The core retains order state, inventory rollback and save ownership through the versioned `DiningAPI.h` interface. Existing save record formats remain unchanged. The installer offers dining with EAS, dining without EAS, or ordering/serving only. Gameplay retesting is pending; this migration does not yet enable VR.

Source repository for a Skyrim SE mod that lets you order and enjoy a meal at an inn.

The mod is built around dining while seated at a table: call a member of staff, browse dishes and set meals, check prices and availability, pay, receive your food, and connect the experience to the game's native eating behavior. The source includes Chinese reading notes and consistent formatting to make it easier to explore and maintain.

This repository contains the current C++ ordering core, service plugin, EAS adapter, Papyrus scripts, menu configuration, and offline tests. **It is a source project, not a complete mod package ready to install in the game.** Game data, other mods, the full collection of artwork and audio, compiled plugins, and local build caches are not included in this source snapshot.

The EAS observer1 test adapter observes existing native calls and retained script state without distributing or overriding the three `aaaEAS*` scripts. Old notification bindings remain as compatibility no-ops. The independent probe has correlated a beef, one-hand path on Vortex runtime 1.7.104; the new override-free binary and other animation routes still require gameplay tests.

### Where to start

| Directory or file | Contents |
| --- | --- |
| [Source reading guide](native/源码阅读索引.txt) (Chinese) | What each source file does and a suggested reading order |
| `native/core/src/` | Ordering, pricing, payment, menus, tables, and settings |
| `native/core/adapter/src/` | Integration between EAS eating animations and tabletop objects |
| `native/service/src/` | Staff selection, supply, and service state management |
| `native/scripts/` | Papyrus script source |
| `native/core/config/` | Food catalogs, inns, tabletop rules, localization tables, and configuration |
| `native/core/tests/`, `native/service/tests/` | 13 groups of unit and component tests |
| `native/integration/` | 6 groups of offline integration tests and a test fixture DLL |
| `payload/` | The menu font and its OFL license, used by the menu and font tests |
| `docs/` | Build, testing, and third-party dependency documentation |

Useful entry points:

- [OrderPolicy.h](native/core/src/OrderPolicy.h): food sources, selection, and pricing rules.
- [Transaction.h](native/core/src/Transaction.h): stock removal, payment, and rollback on failure.
- [main.cpp](native/core/src/main.cpp): the ordering plugin's entry point and menu session.
- [ServiceManager.cpp](native/service/src/ServiceManager.cpp): service requests and staff state.
- [integration_tests.cpp](native/integration/integration_tests.cpp): test scenarios that connect multiple business logic components.

### Development and builds

On the original development machine, open [PullUpAChair.code-workspace](PullUpAChair.code-workspace) in VS Code. See the [VS Code guide](native/VS%20Code%20使用说明.txt) (Chinese) for common actions, and the [Git guide](docs/GIT.md) (Chinese) for committing and pushing changes.

The verified toolchain is Windows x64, MSVC 14.51, CMake 4.3, and C++23. Full plugin builds currently depend on the prebuilt CommonLib, renderer libraries, and other third-party dependencies in the original development workspace. The project has not yet been made self-contained for building on an arbitrary machine.

**The 6 integration test groups can be built separately without the game SDK or a game installation.** Commands and full plugin build requirements are in the [build guide](docs/BUILD.md) (Chinese).

### Test results

Recorded results from October 14, 2026:

- Unit and component tests: 13 groups passed, 0 failed, 0 skipped.
- Offline integration tests: 6 groups passed, 0 failed, 0 skipped.
- Targeted mutation retest: all 125 mutations in the same test set were detected. Additional tests caught the 21 mutations that had survived the first run.

The mutation detection rate applies only to this specific set of mutations; it does not mean every possible defect can be detected. Offline tests use test doubles at the game engine boundary and do not replace in-game validation of NPC service, object placement, UI, or eating animations. See the [testing guide and records](docs/TESTING.md) (Chinese) for details.

### Licensing and third-party code

A project-wide license has not yet been selected. Included third-party content retains its own licenses: the SkyPrompt API is licensed under MIT, and the WenKai font under SIL OFL 1.1. See the [third-party notes](docs/THIRD_PARTY.md) (Chinese) for information about external dependencies and renderer licensing.
