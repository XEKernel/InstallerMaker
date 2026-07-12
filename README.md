# InstallerMaker

**CMake + Qt6 + C++17 的 Windows 安装程序框架** — 12 选项卡 GUI 配置工具，一键生成专业安装包。

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![Qt](https://img.shields.io/badge/Qt-6.11-green)]()
[![Platform](https://img.shields.io/badge/Windows-10%2B-lightgrey)]()
[![License](https://img.shields.io/badge/license-MIT-brightgreen)](example_package/files/LICENSE.txt)

---

## 快速开始

### 1. 打开配置工具

双击 `build/deploy/Configurator.exe`（或从源码构建后运行）。

### 2. 填写配置

在 **基本信息** 选项卡输入产品名、版本、发布者。输入产品名后，短名称、安装文件夹、主程序文件名自动同步。

然后切换到其他选项卡按需配置：
- **安装设置** — 安装根目录、许可协议、管理员权限
- **文件策略** — 选择待打包文件目录，排除规则
- **快捷方式** — 桌面/开始菜单/启动项
- **外观** — 图标、横幅、界面样式（向导/一键安装）
- **注册表** — 自定义键值对
- **卸载项** — 卸载命令、图标
- **环境变量** — PATH 追加、系统变量
- **文件关联** — .ext → 打开命令
- **完成动作** — 安装后运行程序、重启提示
- **高级** — 日志、静默安装、系统版本检查、更新保留规则

### 3. 生成安装程序

切换到 **输出** 选项卡，选择输出目录，点击「**生成安装程序**」。

Configurator 会自动执行：
1. 生成 `package.json` + 复制文件到临时目录
2. 调用 `cmake` 配置
3. 调用 `cmake --build` 编译
4. 输出 `Setup.exe` 到指定目录

---

## 安装程序使用

### 用户双击 `Setup.exe`

两种界面样式可选（在配置工具外观选项卡设置）：

**向导样式** — 分步操作，左侧步骤指示器：

```
┌──────────┬──────────────────────────┐
│ ● 欢迎   │  欢迎使用 MyApp 安装向导   │
│ ○ 许可   │  本向导将引导您完成...    │
│ ○ 路径   │                          │
│ ○ 进度   │  版本: 1.0 | 发布者...    │
│ ○ 完成   │                          │
├──────────┴──────────────────────────┤
│      [< 上一步]  [下一步]  [取消]    │
└─────────────────────────────────────┘
```

**一键安装样式** — 所有选项在一个界面：

```
┌───────────────────────┬──────────────┐
│  MyApp 1.0.0          │  ┌────────┐  │
│  安装目录: [____][浏览]│  │ 立即安装 │  │
│  ┌─ 安装选项 ───────┐ │  └────────┘  │
│  │ ☑ 桌面快捷方式    │ │              │
│  │ ☑ 开始菜单       │ │  ████░░ 75%  │
│  │ ☐ 开机自启       │ │  正在复制...  │
│  └─────────────────┘ │              │
│  ☑ 我已同意 [许可协议] │              │
└───────────────────────┴──────────────┘
```

### 命令行模式

```bash
# 静默安装（无界面，默认路径）
Setup.exe /S
Setup.exe --silent

# 指定目录静默安装
Setup.exe --install-silent <statefile>

# 软件自动更新（保留配置文件）
Setup.exe --update "C:\Program Files\MyApp"

# 卸载
Setup.exe --uninstall "C:\Program Files\MyApp"
# 或直接双击安装目录中的 Uninstall.exe
```

### 智能升级

双击 `Setup.exe` 时自动检测是否已安装：

| 状态 | 行为 |
|---|---|
| 未安装 | 显示安装向导 |
| 已安装同版本 | 弹窗选择：覆盖安装（保留配置）/ 修复安装 |
| 已安装旧版本 | 确认后静默升级，保留配置文件 |

保留规则默认：`*.ini` `*.cfg` `*.conf` `config/*` `data/*` `*.db`（可在高级选项卡自定义）。

---

## 模板系统

配置工具的 **输出** 选项卡包含模板管理：

- **保存当前为模板** — 将当前所有选项卡配置保存为 JSON 文件（存储在 `%APPDATA%/InstallerMaker/templates/`）
- **加载选中模板** — 恢复之前保存的配置
- **删除选中模板** — 移除模板文件

模板跨会话持久化，可用于不同项目的快速切换。

---

## CLI 批量构建

```bash
Configurator.exe --batch myapp.json ./output
```

JSON 格式与 `package.json` 一致，额外支持 `_files_source` 字段指定文件源目录。

---

## 从源码构建

### 环境

- Windows 10/11 x64
- [MSYS2 UCRT64](https://www.msys2.org/)
- Qt6: `pacman -S mingw-w64-ucrt-x86_64-qt6-base`
- CMake ≥ 3.16 + Ninja

### 编译

```bash
git clone git@github.com:XEKernel/InstallerMaker.git
cd InstallerMaker
mkdir build && cd build

# 构建（使用示例包）
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DPACKAGE_DIR=../example_package
ninja

# 部署 DLL（生成可独立运行的版本）
ninja deploy-configurator deploy-setup

# 手动复制 MSYS2 运行时
cp /ucrt64/bin/libstdc++-6.dll deploy/
cp /ucrt64/bin/libwinpthread-1.dll deploy/
cp /ucrt64/bin/libgcc_s_seh-1.dll deploy/

# 产物
#  build/Configurator.exe  — 配置工具（开发用）
#  build/Setup.exe         — 安装程序（单个文件）
#  build/deploy/           — 包含所有 DLL 的可分发版本
```

> ⚠️ MOC 不支持含中文字符的构建路径。请在纯 ASCII 路径下编译，或将构建目录设为 `/tmp/build`。

---

## 项目结构

```
InstallerMaker/
├── CMakeLists.txt                    # 双目标：Configurator + Setup
├── example_package/                  # 示例包
│   ├── package.json
│   └── files/LICENSE.txt
└── src/
    ├── main.cpp                      # Setup.exe 入口（wizard/silent/update/uninstall）
    ├── installwizard.hpp / .cpp      # 安装向导（向导/一键双模式）
    ├── installerengine.hpp / .cpp    # 引擎（安装/卸载/更新/环境/关联）
    ├── shortcut.hpp / .cpp           # IShellLink COM 封装
    ├── configurator.hpp / .cpp       # 12 选项卡 GUI 配置工具
    └── configurator_main.cpp         # Configurator.exe 入口（GUI/--batch）
```

## 技术栈

- **语言**: C++17
- **UI**: Qt6 (Core, Gui, Widgets)
- **构建**: CMake 3.16+, Ninja
- **编译器**: GCC 16.1.0 (MSYS2 UCRT64)
- **Windows API**: IShellLink, ShellExecuteExW, Registry, RtlGetVersion, CopyFileW, MoveFileExW

## 许可证

MIT — 详见 [LICENSE](example_package/files/LICENSE.txt)
