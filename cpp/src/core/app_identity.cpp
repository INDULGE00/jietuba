#include "core/app_identity.h"
#include "core/data_location.h"
#include "core/i18n.h"
#include "core/logger.h"
#include "core/resource_manager.h"
#include "settings/tool_settings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace jietuba {
namespace {
QString tr_identity(const char *text) { return i18n_tr(text, "AppIdentity"); }

// The drawn placeholder when neither a custom icon nor the bundled SVG can be used.
QIcon drawn_fallback_icon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(QColor("#40E0D0")));
    painter.drawRoundedRect(4, 4, 56, 56, 12, 12);
    QFont font;
    font.setPixelSize(38);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor("#10322E"));
    const QString initial = display_app_name().left(1).toUpper();
    painter.drawText(pixmap.rect(), Qt::AlignCenter, initial);
    painter.end();
    return QIcon(pixmap);
}

QSize decoded_icon_size(const QString &path, const QString &suffix) {
    if (suffix == QStringLiteral("svg")) {
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) return {};
        QSize size = renderer.defaultSize();
        // Some SVGs declare only a viewBox; defaultSize() is then empty but the icon is valid.
        if (size.isEmpty()) size = renderer.viewBoxF().size().toSize();
        return size;
    }
    QImageReader reader(path);
    return reader.canRead() ? reader.size() : QSize();
}

// Rasterises a file icon at `size`. SVG must be rendered explicitly: QIcon(path).availableSizes()
// is empty for vector icons, which must not be treated as a read failure.
QPixmap render_icon(const QString &path, const QString &suffix, int size) {
    if (suffix == QStringLiteral("svg")) {
        QSvgRenderer renderer(path);
        if (!renderer.isValid()) return {};
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter);
        painter.end();
        return pixmap;
    }
    return QIcon(path).pixmap(size, size);
}
}

QString custom_app_name() { return get_tool_settings_manager()->get_custom_app_name().trimmed(); }

QString display_app_name() {
    const QString custom = custom_app_name();
    return custom.isEmpty() ? DEFAULT_APP_NAME : custom;
}

QString tray_tooltip_text() {
    const QString localized = i18n_tr("jietuba - Click to screenshot", "MainApp");
    const QString custom = custom_app_name();
    if (custom.isEmpty()) return localized;
    // The localized text starts with the app name followed by " - "; keep the suffix translated.
    const int dash = localized.indexOf(QStringLiteral(" - "));
    return dash < 0 ? custom : custom + localized.mid(dash);
}

QString validate_custom_icon(const QString &path) {
    const QFileInfo info(path);
    if (!info.isFile()) return tr_identity("The file does not exist.");
    if (info.size() > CUSTOM_ICON_MAX_BYTES) return tr_identity("The icon file is larger than 4 MB.");
    const QString suffix = info.suffix().toLower();
    if (!CUSTOM_ICON_SUFFIXES.contains(suffix)) return tr_identity("Unsupported file type. Use an ICO, PNG or SVG file.");
    // SVG is vector: no meaningful pixel size. Only raster formats need a square size in range.
    if (suffix != QStringLiteral("svg")) {
        const QSize size = decoded_icon_size(path, suffix);
        if (size.isEmpty() || size.width() != size.height()) return tr_identity("The icon must be a square ICO, PNG or SVG image.");
        if (size.width() < CUSTOM_ICON_MIN_SIZE || size.width() > CUSTOM_ICON_MAX_SIZE)
            return tr_identity("The icon must be between 16x16 and 1024x1024 pixels.");
    }
    // Actually decode the file. For SVG rely on QSvgRenderer (availableSizes() is always empty
    // for vectors); for raster formats use QIcon's own decoding.
    if (render_icon(path, suffix, 64).isNull()) return tr_identity("The icon file could not be read.");
    return {};
}

QString custom_icon_path() {
    const QString path = get_tool_settings_manager()->get_custom_icon_path().trimmed();
    if (path.isEmpty() || !validate_custom_icon(path).isEmpty()) return {};
    return path;
}

QIcon custom_icon(int size) {
    const QString path = custom_icon_path();
    if (path.isEmpty()) return {};
    const QString suffix = QFileInfo(path).suffix().toLower();
    // Deliver a rasterised icon: SVG QIcon engines do not paint reliably into a system tray
    // on Windows, and the taskbar code paints into a QPixmap anyway.
    const QPixmap pixmap = render_icon(path, suffix, size);
    return pixmap.isNull() ? QIcon() : QIcon(pixmap);
}

QIcon fallback_app_icon() { return drawn_fallback_icon(); }

QIcon app_icon(int size) {
    QIcon icon = custom_icon(size);
    if (!icon.isNull()) return icon;
    const auto icon_path = ResourceManager::get_resource_path(QString::fromUtf8("svg/托盘.svg"));
    if (QFile::exists(icon_path)) {
        icon = QIcon(icon_path);
        if (!icon.isNull() && !icon.pixmap(size, size).isNull()) return icon;
    }
    return drawn_fallback_icon();
}

QString install_custom_icon(const QString &source, QString *error) {
    const auto set_error = [error](const QString &text) {
        if (error) *error = text;
        return QString();
    };
    const QString reason = validate_custom_icon(source);
    if (!reason.isEmpty()) return set_error(reason);

    const QString suffix = QFileInfo(source).suffix().toLower();
    const QString dir = data_location().dir;
    if (dir.isEmpty() || !QDir().mkpath(dir)) return set_error(tr_identity("The data folder could not be created."));

    // One install at a time: remove the old icon in every supported format so a format switch
    // cannot leave two candidates behind.
    for (const QString &old_suffix : CUSTOM_ICON_SUFFIXES) QFile::remove(QDir(dir).filePath(QStringLiteral("app_icon.") + old_suffix));
    const QString target = QDir(dir).filePath(QStringLiteral("app_icon.") + suffix);
    if (!QFile::copy(source, target)) return set_error(tr_identity("The icon file could not be copied to the data folder."));
    if (!validate_custom_icon(target).isEmpty()) {
        QFile::remove(target);
        return set_error(tr_identity("The copied icon file is damaged."));
    }
    return target;
}

void remove_custom_icon(const QString &path) {
    const QFileInfo info(path);
    if (info.fileName().startsWith(QStringLiteral("app_icon.")) && info.absolutePath() == data_location().dir) QFile::remove(path);
}

QString custom_icon_requirements_text() {
    return tr_identity("ICO / PNG / SVG; PNG/ICO must be square 16x16-1024x1024 px; up to 4 MB. Recommended: 64x64 or 256x256 PNG.");
}
}
