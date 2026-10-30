#include "theme.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

ThemePalette::ThemePalette(const QString &path, QObject *parent) : QObject(parent),
    m_path(path.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml" : path) {
    m_reload.setSingleShot(true);
    m_reload.setInterval(80);
    connect(&m_reload, &QTimer::timeout, this, &ThemePalette::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reload.start(); });
    reload();
}

void ThemePalette::reload() {
    // Omarchy replaces the entire theme directory, so watch its parent too.
    // A watch may still report the old pathname after its inode was renamed;
    // rebuild the watches so subsequent edits follow the replacement inode.
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) m_watcher.removePaths(watched);
    const QFileInfo file(m_path);
    const QStringList paths{m_path, file.absolutePath(), QFileInfo(file.absolutePath()).absolutePath()};
    for (const auto &path : paths)
        if (QFile::exists(path)) m_watcher.addPath(path);
    QFile input(m_path);
    if (!input.open(QIODevice::ReadOnly)) return;
    QVariantMap colors;
    const auto raw = QString::fromUtf8(input.readAll());
    const QRegularExpression expression("^\\s*([\\w-]+)\\s*=\\s*[\"'](#[0-9a-fA-F]{6})[\"']", QRegularExpression::MultilineOption);
    auto matches = expression.globalMatch(raw);
    while (matches.hasNext()) { const auto match = matches.next(); colors.insert(match.captured(1), match.captured(2)); }
    if (m_colors != colors) { m_colors = colors; emit colorsChanged(); }
}
