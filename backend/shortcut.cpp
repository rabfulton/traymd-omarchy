#include "shortcut.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace {
const QString begin = "-- BEGIN traymd-omarchy shortcut";
const QString end = "-- END traymd-omarchy shortcut";
QRegularExpression blockExpression() {
    return QRegularExpression(QRegularExpression::escape(begin) + ".*?" + QRegularExpression::escape(end) + "\\n?", QRegularExpression::DotMatchesEverythingOption);
}
QString readShortcut(const QString &source) {
    const auto block = blockExpression().match(source).captured();
    return QRegularExpression("o\\.bind\\(\"([^\"]+)\"").match(block).captured(1);
}
bool writeFile(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}

ShortcutManager::ShortcutManager(const QString &path, const QString &command, QObject *parent)
    : QObject(parent), m_path(path.isEmpty() ? QDir::homePath() + "/.config/hypr/bindings.lua" : path), m_command(command) {
    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) m_current = readShortcut(QString::fromUtf8(file.readAll()));
}
bool ShortcutManager::fail(const QString &error) { m_error = error; emit changed(); return false; }
bool ShortcutManager::run(const QStringList &args, QByteArray *output) {
    QProcess process;
    process.start(m_command, args);
    if (!process.waitForFinished(3000)) { process.kill(); process.waitForFinished(); return false; }
    if (output) *output = process.readAllStandardOutput();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
bool ShortcutManager::setShortcut(const QString &keys) {
    auto tokens = keys.toUpper().split('+', Qt::SkipEmptyParts);
    for (auto &token : tokens) token = token.trimmed();
    const auto combination = tokens.join(" + ");
    int mask = 0;
    QString key;
    for (const auto &token : tokens) {
        if (token == "SUPER") mask |= 64;
        else if (token == "CTRL") mask |= 4;
        else if (token == "ALT") mask |= 8;
        else if (token == "SHIFT") mask |= 1;
        else if (key.isEmpty() && QRegularExpression("^(?:[A-Z0-9]|F[1-9][0-9]?|SPACE|RETURN|TAB|ESCAPE|HOME|END|INSERT|DELETE|LEFT|RIGHT|UP|DOWN|PAGEUP|PAGEDOWN)$").match(token).hasMatch()) key = token;
        else return fail("Choose a valid shortcut.");
    }
    if (!keys.isEmpty() && (!(mask & 64) || key.isEmpty())) return fail("Include Super and a key.");
    QByteArray bindings;
    if (!run({"-j", "binds"}, &bindings)) return fail("Cannot reach Hyprland. Shortcut was not changed.");
    QJsonParseError parseError;
    const auto bindingsJson = QJsonDocument::fromJson(bindings, &parseError);
    if (parseError.error != QJsonParseError::NoError || !bindingsJson.isArray()) return fail("Cannot read existing shortcuts.");
    if (!keys.isEmpty()) {
        for (const auto &value : bindingsJson.array()) {
            const auto binding = value.toObject();
            if (binding["modmask"].toInt() == mask && binding["key"].toString().compare(key, Qt::CaseInsensitive) == 0
                && (binding["arg"].toString() != "omarchy-shell shell toggle traymd-omarchy" || m_current != combination))
                return fail("That shortcut is already in use. Choose another.");
        }
    }
    QFile existing(m_path);
    if (!existing.open(QIODevice::ReadOnly)) return fail("Cannot read your keyboard settings.");
    const auto original = existing.readAll();
    QString source = QString::fromUtf8(original);
    if (source.count(begin) != source.count(end) || source.count(begin) > 1) return fail("Shortcut settings need manual repair.");
    source.remove(blockExpression());
    if (!keys.isEmpty()) {
        if (!source.endsWith('\n')) source += '\n';
        source += begin + "\no.bind(\"" + combination + "\", \"Quick notes\", \"omarchy-shell shell toggle traymd-omarchy\")\n" + end + "\n";
    }
    const auto backup = m_path + ".traymd-" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".bak";
    if (!QFile::copy(m_path, backup)) return fail("Could not back up keyboard settings.");
    if (!writeFile(m_path, source.toUtf8())) return fail("Could not save keyboard settings.");
    QByteArray errors;
    if (!run({"reload"}) || !run({"configerrors"}, &errors) || !errors.trimmed().isEmpty()) {
        const bool restored = writeFile(m_path, original);
        run({"reload"});
        return fail(restored ? "Hyprland rejected the shortcut. Previous settings restored." : "Could not restore settings. Backup: " + backup);
    }
    m_current = combination;
    m_error.clear();
    emit changed();
    return true;
}
