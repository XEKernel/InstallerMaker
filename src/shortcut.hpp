#pragma once

#include <QString>

/// Windows .lnk shortcut creation via COM IShellLink.
/// All methods are static; the class is not constructible.
class Shortcut {
public:
    Shortcut() = delete;

    /// Create a .lnk shortcut file.
    /// @param targetPath  Full path to the target executable.
    /// @param shortcutPath Full path where the .lnk file should be created
    ///                     (including the .lnk extension).
    /// @param description  Optional description / tooltip.
    /// @param iconPath     Optional icon file path; falls back to target.
    /// @param workingDir   Optional working directory; falls back to target dir.
    /// @return true on success.
    static bool create(const QString& targetPath,
                       const QString& shortcutPath,
                       const QString& description = QString(),
                       const QString& iconPath    = QString(),
                       const QString& workingDir  = QString());

    /// Returns the current user's Desktop directory.
    static QString desktopPath();

    /// Returns the current user's Programs / Start Menu directory.
    static QString programsMenuPath();
};
