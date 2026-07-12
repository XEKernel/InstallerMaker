# InstallerMaker

**基于 CMake + Qt6 + C++17 的 Windows 安装程序框架**，适用于为 Windows 桌面应用快速构建专业安装包。

## 为什么选择 InstallerMaker

- **零依赖分发** — 生成的 `Setup.exe` 是单个自包含可执行文件，内嵌所有资源
- **可视化配置** — 12 选项卡 GUI 工具，从基本信息到高级设置一站式配置
- **一键生成** — 填写表单 → 点击按钮 → 自动编译 → 输出安装包
- **完整 Windows 集成** — 快捷方式、注册表、控制面板卸载项、文件关联、环境变量
- **智能升级** — 自动检测已安装版本，同版本覆盖/修复，跨版本静默更新并保留配置文件

## 功能矩阵

### 安装向导

| 特性 | 说明 |
|---|---|
| 单窗口布局 | 左侧步骤指示器 (✓/●/○)，右侧内容切换，无页面跳动 |
| 自定义外观 | 窗口图标、横幅图片、背景色、欢迎/完成页自定义标题正文 |
| 许可协议 | 支持纯文本和 RTF 协议文件，必须勾选接受才能继续 |
| 路径选择 | 4 种安装根目录，用户可自定义，管理员权限自动检测 |
| 后台线程 | QThread + moveToThread，信号更新进度条，界面不冻结 |
| 管理员提权 | `ShellExecuteExW(runas)` 自动提升，静默安装后跳转完成页 |

### 安装功能

| 功能 | 实现 |
|---|---|
| 文件复制 | 镜像 `files/` 目录结构，支持排除规则 (glob) |
| 快捷方式 | IShellLink COM 接口，支持桌面/开始菜单/启动文件夹，命令行参数、图标、管理员运行 |
| 注册表 | 自动生成 `HKCU\Software\<App>` 键 + 自定义键值对，支持 `{VERSION}` `{INSTALL_DIR}` 变量 |
| 环境变量 | 用户/系统级 PATH 追加，广播 `WM_SETTINGCHANGE` 即时生效 |
| 文件关联 | 注册 `HKCU\Software\Classes\.ext` → ProgID → open 命令 + 图标 |
| 开机自启 | `HKCU\...\Run` 注册表项 |
| 卸载程序 | 安装时释放 `Uninstall.exe` + `uninstall.json`，注册到控制面板 |

### 卸载功能

| 操作 | 说明 |
|---|---|
| 文件清理 | 递归删除安装目录所有文件 |
| 快捷方式移除 | 桌面、开始菜单、启动文件夹 |
| 注册表清理 | 应用键 + 卸载键 + 自定义键 |
| 环境变量还原 | PATH 中追加部分移除，自定义变量删除 |
| 文件关联移除 | 删除 HKCU Classes 相关项 |
| 开机自启移除 | 删除 Run 键 |

### 版本管理

| 模式 | 触发方式 | 行为 |
|---|---|---|
| 新安装 | 双击 `Setup.exe` | 完整安装向导 |
| 静默安装 | `/S` 或 `--silent` | 无界面，默认路径 |
| 同版本覆盖 | 检测到相同版本 → 对话框选择 | 保留配置文件，重新安装 |
| 同版本修复 | 检测到相同版本 → 对话框选择 | 仅修复缺失/损坏文件 |
| 版本升级 | 检测到不同版本 → 确认 | 静默更新，保留配置 |
| 自动更新 | `--update <dir>`（软件内部触发） | 备份配置 → 删除旧文件 → 安装新文件 → 还原配置 |

### 配置工具 (Configurator.exe)

12 个选项卡覆盖全部配置：

| 选项卡 | 内容 |
|---|---|
| 基本信息 | 产品名、短名称、版本、发布者、版权、网址、邮箱 |
| 安装设置 | 安装根目录 (4种)、文件夹名、主程序、许可协议、管理员权限 |
| 文件策略 | 模式 (mirror/mapping)、覆盖策略、排除规则 (glob) |
| 快捷方式 | 动态增删行：目标、位置、名称、描述、参数、工作目录、图标、菜单文件夹、管理员标记 |
| 外观 | 窗口/setup/卸载图标、横幅、背景色、欢迎/完成页标题正文 |
| 注册表 | 自动生成 + 自定义键值对动态表格 |
| 卸载项 | 卸载命令、静默命令、图标、估计大小、禁止修改/修复、帮助链接 |
| 环境变量 | 用户/系统 PATH + 自定义变量 |
| 文件关联 | 扩展名、描述、打开命令、图标、ProgID |
| 完成动作 | 运行程序 + 参数、打开自述文件、重启提示 |
| 高级 | 卸载器、日志、静默安装、最低系统版本、64位限制、更新保留规则 |
| 输出 | 目录选择 → 一键编译生成 |

### 命令行

```bash
# 静默安装（无界面）
Setup.exe /S
Setup.exe --silent

# 静默更新（软件内部触发，保留配置）
Setup.exe --update "C:\Program Files\MyApp"

# 卸载
Setup.exe --uninstall "C:\Program Files\MyApp"
# 或直接双击安装目录中的 Uninstall.exe

# CLI 批量构建
Configurator.exe --batch myapp.json ./output
```

## 构建

### 环境

- Windows 10/11 x64
- [MSYS2 UCRT64](https://www.msys2.org/)
- Qt6 — `pacman -S mingw-w64-ucrt-x86_64-qt6-base`
- CMake ≥ 3.16 + Ninja

### 编译

```bash
git clone git@github.com:XEKernel/InstallerMaker.git
cd InstallerMaker
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DPACKAGE_DIR=../example_package
ninja
# 产物: Configurator.exe + Setup.exe
```

> MOC 不支持含中文字符的构建路径，请在纯 ASCII 路径下构建。

## package.json 配置参考

```json
{
  "installer": {
    "name": "MyApp",
    "short_name": "myapp",
    "version": "1.0.0.0",
    "publisher": "MyCompany",
    "copyright": "Copyright (C) 2026",
    "website": "https://example.com",
    "default_install_root": "$LOCALAPPDATA$",
    "default_install_folder": "MyApp",
    "main_executable": "MyApp.exe",
    "license_file": "LICENSE.txt",
    "welcome_title": "欢迎安装 MyApp",
    "finish_message": "MyApp 已准备就绪！",
    "icon": "assets/app.ico",
    "requires_admin": false
  },
  "files_strategy": {
    "mode": "mirror",
    "overwrite": "ask",
    "exclude": ["*.pdb", "temp/**"]
  },
  "shortcuts": [
    { "target": "MyApp.exe", "location": "Desktop", "description": "启动 MyApp" },
    { "target": "MyApp.exe", "location": "StartMenu", "name": "MyApp", "start_menu_folder": "MyApp" }
  ],
  "registry": {
    "HKEY_CURRENT_USER\\Software\\MyCompany\\MyApp": {
      "Version": "{VERSION}",
      "Path": "{INSTALL_DIR}"
    }
  },
  "uninstall": {
    "uninstall_string": "\"{INSTALL_DIR}\\uninstall.exe\"",
    "no_modify": true,
    "no_repair": true
  },
  "environment": {
    "user": { "PATH": "{INSTALL_DIR}\\bin" }
  },
  "file_associations": [
    { "extension": ".mydoc", "description": "MyApp Document",
      "open_command": "\"{INSTALL_DIR}\\MyApp.exe\" \"%1\"" }
  ],
  "finish_actions": {
    "run_program": true,
    "run_arguments": "--welcome"
  },
  "advanced": {
    "create_uninstaller": true,
    "silent_install": true,
    "update_preserve_patterns": ["*.ini", "*.cfg", "config/*", "data/*"]
  }
}
```

## 项目结构

```
InstallerMaker/
├── CMakeLists.txt
├── .gitignore
├── README.md
├── example_package/          # 示例包
│   ├── package.json
│   └── files/
│       └── LICENSE.txt
└── src/
    ├── main.cpp              # Setup.exe 入口
    ├── installwizard.hpp/cpp # 安装/卸载向导
    ├── installerengine.hpp/cpp# 核心引擎
    ├── shortcut.hpp/cpp      # IShellLink COM 封装
    ├── configurator.hpp/cpp  # GUI 配置工具
    └── configurator_main.cpp # Configurator.exe 入口
```

## 技术栈

- **语言**: C++17
- **UI**: Qt6 (Core, Gui, Widgets)
- **构建**: CMake 3.16+, Ninja
- **平台**: Windows 10/11 x64
- **编译器**: GCC 16.1.0 (MSYS2 UCRT64)
- **Windows API**: IShellLink, ShellExecuteExW, Registry, RtlGetVersion

## 许可证

MIT License — 详见 [LICENSE](example_package/files/LICENSE.txt)
