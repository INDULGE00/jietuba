#pragma once
#include <QIcon>
#include <QString>
#include <QStringList>

namespace jietuba {
// Shared identity of the application: the name shown to the user and the icon used by the
// tray, the window title bars and the settings dialog. Both can be overridden by the user
// in the settings (About page); the built-in defaults are used when no override is set.

inline const QString DEFAULT_APP_NAME = QStringLiteral("jietuba");

// Requirements advertised in the settings UI and enforced when a file is picked:
// - file types: ICO / PNG / SVG (Qt's QIcon reads all three)
// - the file must decode to at least one image and be square; raster icons are 16x16 to 1024x1024
// - no larger than 4 MiB on disk
inline const QStringList CUSTOM_ICON_SUFFIXES = {"ico", "png", "svg"};
inline constexpr int CUSTOM_ICON_MIN_SIZE = 16;
inline constexpr int CUSTOM_ICON_MAX_SIZE = 1024;
inline constexpr qint64 CUSTOM_ICON_MAX_BYTES = 4 * 1024 * 1024;

QString display_app_name();
// Tray tooltip: the existing localized "jietuba - Click to screenshot" with the leading name
// replaced by the user's name when one is set. No new translation catalogue is needed.
QString tray_tooltip_text();
// Empty when custom_app_name is unset / blank.
QString custom_app_name();
// Empty when custom_icon_path is unset, missing or invalid.
QString custom_icon_path();
QIcon custom_icon(int size = 64);
// Full icon: the custom one when valid, else the bundled tray SVG, else the drawn fallback.
QIcon app_icon(int size = 64);
// The drawn placeholder, also used by MainApp's compatibility wrapper.
QIcon fallback_app_icon();
// Validates a candidate file; on failure returns a user-facing reason (empty on success).
QString validate_custom_icon(const QString &path);
// Copies `source` next to the settings file under a stable name and returns the stored path;
// empty on failure, with `error` set to a user-facing reason.
QString install_custom_icon(const QString &source, QString *error = nullptr);
// Removes a previously installed icon file; errors are ignored.
void remove_custom_icon(const QString &path);
// The hint shown in the settings UI about the accepted icon file types and sizes.
QString custom_icon_requirements_text();
}
