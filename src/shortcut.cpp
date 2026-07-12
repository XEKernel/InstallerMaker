#include "shortcut.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shlobj.h>
#  include <objbase.h>
#endif

bool Shortcut::create(const QString& targetPath,
                      const QString& shortcutPath,
                      const QString& description,
                      const QString& arguments,
                      const QString& iconPath,
                      int            iconIndex,
                      const QString& workingDir,
                      bool           runAsAdmin)
{
#ifdef Q_OS_WIN
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return false;

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr)) { CoUninitialize(); return false; }

    shellLink->SetPath(reinterpret_cast<LPCWSTR>(targetPath.utf16()));

    if (!description.isEmpty())
        shellLink->SetDescription(reinterpret_cast<LPCWSTR>(description.utf16()));

    if (!arguments.isEmpty())
        shellLink->SetArguments(reinterpret_cast<LPCWSTR>(arguments.utf16()));

    if (!iconPath.isEmpty())
        shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(iconPath.utf16()), iconIndex);
    else
        shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(targetPath.utf16()), iconIndex);

    if (!workingDir.isEmpty())
        shellLink->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workingDir.utf16()));
    else
        shellLink->SetWorkingDirectory(
            reinterpret_cast<LPCWSTR>(QFileInfo(targetPath).absolutePath().utf16()));

    // Run as admin flag
    if (runAsAdmin) {
        IShellLinkDataList* dataList = nullptr;
        hr = shellLink->QueryInterface(IID_IShellLinkDataList,
                                       reinterpret_cast<void**>(&dataList));
        if (SUCCEEDED(hr)) {
            DWORD flags = 0;
            dataList->GetFlags(&flags);
            flags |= SLDF_RUNAS_USER;
            dataList->SetFlags(flags);
            dataList->Release();
        }
    }

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<void**>(&persistFile));
    if (SUCCEEDED(hr)) {
        QDir().mkpath(QFileInfo(shortcutPath).absolutePath());
        persistFile->Save(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    CoUninitialize();
    return SUCCEEDED(hr);
#else
    Q_UNUSED(targetPath); Q_UNUSED(shortcutPath); Q_UNUSED(description);
    Q_UNUSED(arguments); Q_UNUSED(iconPath); Q_UNUSED(iconIndex);
    Q_UNUSED(workingDir); Q_UNUSED(runAsAdmin);
    return false;
#endif
}

QString Shortcut::desktopPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

QString Shortcut::programsMenuPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

QString Shortcut::startupPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)
           + QStringLiteral("/../Startup");
}
