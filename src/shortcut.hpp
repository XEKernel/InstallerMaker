#pragma once

#include <QString>

/// Windows .lnk shortcut creation via COM IShellLink.
class Shortcut {
public:
    Shortcut() = delete;

    /// Create a .lnk shortcut file.
    /// @param targetPath   Full path to the target executable.
    /// @param shortcutPath Full path where the .lnk file (including .lnk ext).
    /// @param description  Optional tooltip text.
    /// @param arguments    Optional command-line arguments.
    /// @param iconPath     Optional icon file; falls back to target.
    /// @param iconIndex    Icon index within the icon file (0 = first).
    /// @param workingDir   Optional working directory.
    /// @param runAsAdmin   If true, marks the shortcut to run as administrator.
    /// @return true on success.
    static bool create(const QString& targetPath,
                       const QString& shortcutPath,
                       const QString& description = {},
                       const QString& arguments   = {},
                       const QString& iconPath    = {},
                       int            iconIndex   = 0,
                       const QString& workingDir  = {},
                       bool           runAsAdmin  = false);

    static QString desktopPath();
    static QString programsMenuPath();
    static QString startupPath();
};
