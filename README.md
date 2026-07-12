# InstallerMaker

基于 **CMake + Qt6 + C++17** 的 Windows 软件安装程序框架，使用 MSYS2 UCRT 编译。

## 项目组成

| 产物 | 说明 |
|---|---|
| `Configurator.exe` | GUI 配置工具：填写应用信息 → 一键生成 `Setup.exe` |
| `Setup.exe` | 安装/卸载二合一程序，单窗口左侧步骤指示器 |

## 特性

- **单窗口安装向导**：左侧步骤指示器 (✓/●/○)，右侧 QStackedWidget 切换内容，无页面跳动
- **完整卸载**：`Setup.exe` 安装时释放 `Uninstall.exe` + `uninstall.json` 到安装目录，双击即可卸载
- **Windows 集成**：COM IShellLink 快捷方式、注册表写入、控制面板「添加/删除程序」注册
- **管理员提权**：自动检测写入权限，通过 `ShellExecuteExW(runas)` 提升
- **后台安装**：QThread + moveToThread 线程模型，信号更新 UI 进度条，不冻结界面
- **配置驱动**：所有行为由 `package.json` 控制，Configurator 可视化编辑

## 构建

### 环境要求

- Windows 10/11 x64
- [MSYS2 UCRT64](https://www.msys2.org/) 环境
- Qt6 (Core, Gui, Widgets) — `pacman -S mingw-w64-ucrt-x86_64-qt6-base`
- CMake ≥ 3.16 + Ninja

### 编译

```bash
# 克隆
git clone https://github.com/XEKernel/InstallerMaker.git
cd InstallerMaker

# 构建（使用示例包）
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DPACKAGE_DIR=../example_package
ninja

# 产物
#  Configurator.exe — 配置工具
#  Setup.exe        — 安装/卸载程序
```

> **注意**：MOC 不支持含中文字符的构建路径，请在纯 ASCII 路径下构建。

### 使用 Configurator 生成安装程序

1. 运行 `Configurator.exe`
2. 填写应用名称、版本、发布者
3. 选择待打包文件目录
4. 可选：设置快捷方式、图标、许可协议、注册表项等
5. 点击「生成安装程序」→ 自动编译 → 输出 `Setup.exe`

## 配置项参考 (package.json)

```json
{
  "installer": {
    "name": "MyApp",
    "version": "1.0.0",
    "publisher": "MyCompany",
    "description": "应用描述（可选）",
    "default_install_folder": "MyApp",
    "default_install_root": "$LOCALAPPDATA$",
    "main_executable": "MyApp.exe",
    "start_menu_folder": "MyApp",
    "app_icon": "app.ico",
    "support_url": "https://example.com/support",
    "update_url": "https://example.com/update",
    "welcome_text": "欢迎使用 MyApp！",
    "require_admin": false,
    "auto_start": false,
    "require_restart": false,
    "run_after_install": true
  },
  "shortcuts": [
    { "target": "MyApp.exe", "location": "Desktop" },
    { "target": "MyApp.exe", "location": "StartMenu" }
  ],
  "registry": {
    "HKEY_CURRENT_USER\\Software\\MyApp": {
      "Version": "{VERSION}",
      "Path": "{INSTALL_DIR}"
    }
  },
  "license_file": "LICENSE.txt",
  "finish_message": "安装完成！"
}
```

## 项目结构

```
InstallerMaker/
├── CMakeLists.txt              # 双目标：Configurator + Setup
├── example_package/            # 示例包（package.json + files/）
└── src/
    ├── main.cpp                # 入口（wizard / silent / uninstall）
    ├── configurator.hpp/.cpp   # GUI 配置工具
    ├── configurator_main.cpp   # Configurator 入口
    ├── installwizard.hpp/.cpp  # 单窗口安装/卸载向导
    ├── installerengine.hpp/.cpp# 核心引擎
    └── shortcut.hpp/.cpp       # IShellLink COM 封装
```

## 许可证

MIT License
