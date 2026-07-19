# InstallerMaker

**CMake + Qt6 + C++17 的 Windows 安装程序框架** — 12 选项卡 GUI 配置工具，一键生成专业安装包。

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue)]()
[![Qt](https://img.shields.io/badge/Qt-6.11-green)]()
[![Platform](https://img.shields.io/badge/Windows-10%2B-lightgrey)]()
[![License](https://img.shields.io/badge/license-MIT-brightgreen)](example_package/files/LICENSE.txt)

---

## 环境要求

### 用户（运行 Configurator 生成安装包）

需要 MSYS2 UCRT64 环境，安装以下包：

```bash
pacman -S mingw-w64-ucrt-x86_64-qt6-base
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ninja
pacman -S mingw-w64-ucrt-x86_64-gcc
```

> MSYS2 UCRT64 的 `bin/` 目录必须在系统 PATH 中，否则 Configurator 无法找到 cmake/ninja/GCC。

### 最终用户（运行生成的 Setup.exe）

无需任何依赖，双击 Setup.exe 即可安装。生成的 Setup.exe 使用 QRC 内嵌所有文件。

---

## 快速开始

### 1. 打开配置工具

双击 `build/deploy/Configurator.exe`。

### 2. 填写配置

在 **基本信息** 选项卡输入产品名、版本、发布者。输入产品名后，短名称、安装文件夹、主程序文件名自动同步。

然后切换到其他选项卡按需配置：

| 选项卡 | 说明 |
|---|---|
| 基本信息 | 产品名、版本、发布者、版权、网址 |
| 安装设置 | 安装根目录、许可协议、管理员权限 |
| 文件策略 | 选择待打包文件目录，排除规则 |
| 快捷方式 | 桌面/开始菜单/启动项快捷方式 |
| 外观 | 图标、横幅、界面样式（向导/一键安装） |
| 注册表 | 自定义键值对写入注册表 |
| 卸载项 | 卸载命令、卸载器图标 |
| 环境变量 | PATH 追加、系统变量设置 |
| 文件关联 | .ext 扩展名关联到打开命令 |
| 完成动作 | 安装后运行程序、重启提示 |
| 高级 | 日志、静默安装、系统版本检查、更新保留规则 |
| 输出 | 输出目录、生成安装程序、模板管理 |

### 3. 生成安装程序

切换到 **输出** 选项卡，选择输出目录，点击「**生成安装程序**」。

Configurator 后台线程自动执行（UI 不卡死）：
1. 生成 `package.json` + 复制文件到临时目录
2. 调用 cmake 配置 + 编译（需 MSYS2 工具链）
3. 输出 `Setup.exe` 到指定目录

---

## 安装程序使用

### 用户双击 `Setup.exe`

#### 向导样式（分步操作）

```
┌──────────┬──────────────────────────┐
│ ● 欢迎   │  欢迎使用 MyApp 安装向导   │
│ ○ 许可   │                          │
│ ○ 路径   │  版本: 1.0 | 发布者...    │
│ ○ 进度   │                          │
│ ○ 完成   │  ████████░░░░ 75%        │
│          │  正在复制: MyApp.exe      │
│          │  ┌──────────────────┐    │
│          │  │ 安装日志...       │    │
│          │  └──────────────────┘    │
├──────────┴──────────────────────────┤
│      [< 上一步]  [下一步]  [取消]    │
└─────────────────────────────────────┘
```

- 左侧深色面板 + ●（当前）/ ○（其他）步骤指示器
- 进度页日志始终可见，实时显示文件复制详情

#### 一键安装样式（单页操作）

```
┌──────────────────────────────┐
│     MyApp 1.0.0              │
│     MyCompany                │
│                              │
│  安装目录: [_______] [浏览]   │
│  ┌─ 安装选项 ──────────────┐ │
│  │ ☑ 桌面快捷方式          │ │
│  │ ☑ 开始菜单             │ │
│  │ ☐ 开机自启             │ │
│  └────────────────────────┘ │
│  ☑ 我已同意 [许可协议]       │
│       [  立即安装  ]         │
│                              │
│  正在复制: MyApp.exe         │
│  ██████████████░░░░░░░░░░    │
└──────────────────────────────┘
```

- 所有选项在一个界面，进度条固定在底部
- 许可协议点击蓝色链接在独立窗口查看

### 命令行模式

```bash
# 静默安装（无界面，默认路径）
Setup.exe /S
Setup.exe --silent

# 指定状态文件静默安装
Setup.exe --install-silent <statefile>

# 自动更新（保留配置文件）
Setup.exe --update "C:\Program Files\MyApp"

# 卸载
Setup.exe --uninstall "C:\Program Files\MyApp"
# 或直接双击安装目录中的 Uninstall.exe
```

---

## 模板系统

**输出** 选项卡支持模板管理：

- **保存当前为模板** — 将所有选项卡配置保存为 JSON 文件
- **加载选中模板** — 恢复之前保存的配置
- **删除选中模板** — 移除模板文件

模板存储在 `%APPDATA%/InstallerMaker/templates/`，跨会话持久化。

---

## CLI 批量构建

```bash
Configurator.exe --batch myapp.json ./output
```

JSON 格式与 `package.json` 一致，额外支持 `_files_source` 字段指定文件源目录。

---

## 从源码构建

### 开发环境

- Windows 10/11 x64
- [MSYS2](https://www.msys2.org/) UCRT64
- 安装依赖：`pacman -S mingw-w64-ucrt-x86_64-qt6-base cmake ninja gcc`

### 编译

```bash
git clone git@github.com:XEKernel/InstallerMaker.git
cd InstallerMaker
mkdir build && cd build

cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DPACKAGE_DIR=../example_package
ninja

# 部署 DLL
ninja deploy-configurator deploy-setup

# 手动复制 MSYS2 运行时
cp /ucrt64/bin/libstdc++-6.dll deploy/
cp /ucrt64/bin/libwinpthread-1.dll deploy/
cp /ucrt64/bin/libgcc_s_seh-1.dll deploy/
```

> ⚠️ CMake 的 MOC 不支持含中文字符的构建路径，请在纯 ASCII 路径下构建。

### 产物

```
build/
├── Configurator.exe        # 配置工具
├── Setup.exe               # 安装程序（开发测试用）
└── deploy/                 # 可独立运行版本（含 Qt DLL）
    ├── Configurator.exe
    ├── Setup.exe
    ├── Qt6Core.dll
    ├── Qt6Gui.dll
    ├── Qt6Widgets.dll
    ├── libstdc++-6.dll
    ├── libwinpthread-1.dll
    ├── libgcc_s_seh-1.dll
    ├── platforms/qwindows.dll
    └── styles/qmodernwindowsstyle.dll
```

---

## 项目结构

```
InstallerMaker/
├── CMakeLists.txt                    # 双目标：Configurator + Setup
├── example_package/                  # 示例包
│   ├── package.json
│   └── files/LICENSE.txt
└── src/
    ├── main.cpp                      # Setup.exe 入口
    ├── installwizard.hpp / .cpp      # 安装向导（向导/一键双模式）
    ├── installerengine.hpp / .cpp    # 引擎（安装/卸载/更新/环境/关联）
    ├── shortcut.hpp / .cpp           # IShellLink COM 封装
    ├── configurator.hpp / .cpp       # 12 选项卡 GUI 配置工具
    └── configurator_main.cpp         # Configurator.exe 入口
```

## 技术栈

| 项 | 值 |
|---|---|
| 语言 | C++17 |
| UI | Qt6 (Core, Gui, Widgets) |
| 构建 | CMake 3.16+ / Ninja |
| 编译器 | GCC (MSYS2 UCRT64) |
| 生成方式 | cmake + ninja 重新编译（需 MSYS2 工具链） |
| 安装器格式 | QRC 内嵌 → 单文件 Setup.exe |
## 许可证

MIT — 详见 [LICENSE](example_package/files/LICENSE.txt)
