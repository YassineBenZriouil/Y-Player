#pragma once

#include <QString>
#include <QStringList>

namespace yp::platform {

// Per-user file associations under HKEY_CURRENT_USER\Software\Classes (no admin rights).
// Registering adds Y-Player to "Open with" for each extension; Windows 10/11 deliberately do not
// let programs make themselves the default, so the user confirms that in Settings > Default apps.
QStringList associableExtensions();  // without dots, e.g. "mkv"
QStringList registeredExtensions();  // those currently pointing at Y-Player
bool registerExtensions(const QStringList& extensions, const QString& exePath, QString* error = nullptr);
bool unregisterExtensions(const QStringList& extensions, QString* error = nullptr);
void openDefaultAppsSettings();

}  // namespace yp::platform
