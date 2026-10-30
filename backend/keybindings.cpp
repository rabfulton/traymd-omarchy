#include "keybindings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>

namespace {
const QVariantMap defaults{
    {"older_note", "Alt+Left"}, {"newer_note", "Alt+Right"},
    {"new_note", "Ctrl+N"}, {"close", "Ctrl+W"},
    {"settings", "Ctrl+,"}, {"save", "Ctrl+S"},
    {"delete_note", "Ctrl+Shift+Del"}
};
}

Keybindings::Keybindings(const QString &path, QObject *parent) : QObject(parent),
    m_path(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + "/traymd-omarchy/config.ini" : path) {
    for (auto i = defaults.begin(); i != defaults.end(); ++i) {
        m_shortcuts[i.key()] = i.value();
        m_sequences[i.key()] = QKeySequence::fromString(i.value().toString(), QKeySequence::PortableText);
    }
    if (!QFile::exists(m_path) && QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        QSettings settings(m_path, QSettings::IniFormat);
        settings.beginGroup("shortcuts");
        for (auto i = defaults.begin(); i != defaults.end(); ++i) settings.setValue(i.key(), i.value());
        settings.sync();
    }
    m_reload.setSingleShot(true);
    m_reload.setInterval(80);
    connect(&m_reload, &QTimer::timeout, this, &Keybindings::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reload.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reload.start(); });
    reload();
}

void Keybindings::reload() {
    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) m_watcher.removePaths(watched);
    const QStringList paths{m_path, QFileInfo(m_path).absolutePath()};
    for (const auto &path : paths) if (QFile::exists(path)) m_watcher.addPath(path);
    m_error.clear();
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = "Cannot read shortcut settings: " + m_path;
        emit changed(); return;
    }
    file.close();
    QSettings settings(m_path, QSettings::IniFormat);
    settings.beginGroup("shortcuts");
    QVariantMap names;
    QMap<QString, QKeySequence> sequences;
    QSet<int> used;
    for (auto i = defaults.begin(); i != defaults.end(); ++i) {
        const auto value = settings.value(i.key(), i.value()).toString().trimmed();
        const auto sequence = QKeySequence::fromString(value, QKeySequence::PortableText);
        if (!value.isEmpty()) {
            if (sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown || sequence[0].key() == 0) {
                m_error = "Invalid shortcut for " + i.key() + " in config.ini"; break;
            }
            if (i.key() != "close" && sequence[0].key() == Qt::Key_Escape) {
                m_error = "Escape is reserved for closing and cancelling."; break;
            }
            if (used.contains(sequence[0].toCombined())) {
                m_error = "Duplicate shortcut in config.ini: " + value; break;
            }
            used.insert(sequence[0].toCombined());
        }
        names[i.key()] = value.isEmpty() ? QString() : sequence.toString(QKeySequence::PortableText);
        sequences[i.key()] = sequence;
    }
    if (settings.status() != QSettings::NoError) m_error = "Invalid shortcut settings in config.ini";
    // Keep the last working map when an editor temporarily writes invalid text.
    if (m_error.isEmpty()) { m_shortcuts = names; m_sequences = sequences; }
    emit changed();
}

bool Keybindings::matches(const QString &action, int key, int modifiers) const {
    const auto sequence = m_sequences.value(action);
    if (sequence.isEmpty()) return false;
    constexpr int relevant = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    return sequence[0] == QKeyCombination(Qt::KeyboardModifiers(modifiers & relevant), Qt::Key(key));
}
