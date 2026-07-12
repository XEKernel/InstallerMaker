#include "shortcut.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shlobj.h>
#  include <objbase.h>
#endif

// ── public static ────────────────────────────────────────────────────────────

bool Shortcut::create(const QString& targetPath,
                      const QString& shortcutPath,
                      const QString& description,
                      const QString& iconPath,
                      const QString& workingDir)
{
#ifdef Q_OS_WIN
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return false;

    IShellLink* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLink, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    // Target
    shellLink->SetPath(reinterpret_cast<LPCWSTR>(targetPath.utf16()));

    // Description / tooltip
    if (!description.isEmpty())
        shellLink->SetDescription(reinterpret_cast<LPCWSTR>(description.utf16()));

    // Icon
    if (!iconPath.isEmpty())
        shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(iconPath.utf16()), 0);
    else
        shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(targetPath.utf16()), 0);

    // Working directory
    if (!workingDir.isEmpty())
        shellLink->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workingDir.utf16()));
    else
        shellLink->SetWorkingDirectory(
            reinterpret_cast<LPCWSTR>(QFileInfo(targetPath).absolutePath().utf16()));

    // Persist
    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<void**>(&persistFile));
    if (SUCCEEDED(hr)) {
        // Ensure parent directory exists
        QDir().mkpath(QFileInfo(shortcutPath).absolutePath());
        persistFile->Save(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), TRUE);
        persistFile->Release();
    }

    shellLink->Release();
    CoUninitialize();
    return SUCCEEDED(hr);
#else
    Q_UNUSED(targetPath)
    Q_UNUSED(shortcutPath)
    Q_UNUSED(description)
    Q_UNUSED(iconPath)
    Q_UNUSED(workingDir)
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
