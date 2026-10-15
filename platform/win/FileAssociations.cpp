#include "platform/win/FileAssociations.h"

#include <QDir>
#include <QSettings>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>

namespace yp::platform {
namespace {

const QString kClasses = QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
const QString kProgId = QStringLiteral("YPlayer.Media");

bool finish(QSettings& reg, QString* error) {
    reg.sync();
    if (reg.status() != QSettings::NoError) {
        if (error) *error = QStringLiteral("the registry could not be written");
        return false;
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);  // refresh Explorer icons/menus
    return true;
}

}  // namespace

QStringList associableExtensions() {
    return {QStringLiteral("mp4"),  QStringLiteral("m4v"),  QStringLiteral("mkv"),  QStringLiteral("webm"),
            QStringLiteral("mov"),  QStringLiteral("avi"),  QStringLiteral("wmv"),  QStringLiteral("flv"),
            QStringLiteral("ts"),   QStringLiteral("m2ts"), QStringLiteral("mts"),  QStringLiteral("mpg"),
            QStringLiteral("mpeg"), QStringLiteral("vob"),  QStringLiteral("ogv"),  QStringLiteral("3gp"),
            QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("m4a"),  QStringLiteral("opus"),
            QStringLiteral("ogg"),  QStringLiteral("wav"),  QStringLiteral("m3u"),  QStringLiteral("m3u8"),
            QStringLiteral("pls")};
}

QStringList registeredExtensions() {
    QSettings reg(kClasses, QSettings::NativeFormat);
    QStringList found;
    for (const QString& ext : associableExtensions()) {
        if (reg.contains(QStringLiteral(".%1/OpenWithProgids/%2").arg(ext, kProgId))) found << ext;
    }
    return found;
}

bool registerExtensions(const QStringList& extensions, const QString& exePath, QString* error) {
    QSettings reg(kClasses, QSettings::NativeFormat);
    const QString exe = QDir::toNativeSeparators(exePath);
    reg.setValue(kProgId + QStringLiteral("/Default"), QStringLiteral("Y-Player media file"));
    reg.setValue(kProgId + QStringLiteral("/DefaultIcon/Default"), QStringLiteral("\"%1\",0").arg(exe));
    reg.setValue(kProgId + QStringLiteral("/shell/open/command/Default"), QStringLiteral("\"%1\" \"%2\"").arg(exe, QStringLiteral("%1")));
    for (const QString& ext : extensions) {
        if (!associableExtensions().contains(ext)) continue;  // only the formats we actually play
        reg.setValue(QStringLiteral(".%1/OpenWithProgids/%2").arg(ext, kProgId), QString());
    }
    return finish(reg, error);
}

bool unregisterExtensions(const QStringList& extensions, QString* error) {
    QSettings reg(kClasses, QSettings::NativeFormat);
    for (const QString& ext : extensions) reg.remove(QStringLiteral(".%1/OpenWithProgids/%2").arg(ext, kProgId));
    const QStringList all = associableExtensions();
    const bool anyLeft = std::ranges::any_of(
        all, [&](const QString& ext) { return reg.contains(QStringLiteral(".%1/OpenWithProgids/%2").arg(ext, kProgId)); });
    if (!anyLeft) reg.remove(kProgId);  // last one gone: drop the ProgID too
    return finish(reg, error);
}

void openDefaultAppsSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps", nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace yp::platform
