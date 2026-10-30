#include "notestore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUuid>
#include <QRegularExpression>
#include <algorithm>

NoteStore::NoteStore(QObject *parent) : QObject(parent) {
    m_autosave.setSingleShot(true);
    m_autosave.setInterval(400);
    connect(&m_autosave, &QTimer::timeout, this, &NoteStore::save);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &NoteStore::save);
}

NoteStore::~NoteStore() { save(); }

QString NoteStore::currentName() const {
    return ready() ? m_names.value(m_index) : QString();
}

bool NoteStore::fail(const QString &message) {
    m_error = message;
    emit errorChanged();
    return false;
}

void NoteStore::clearError() {
    if (m_error.isEmpty()) return;
    m_error.clear();
    emit errorChanged();
}

bool NoteStore::initialize(const QString &directory) {
    // The store is shared across monitors. Configuration applies on load.
    if (!m_directory.isEmpty()) return ready() || (count() > 0 ? load(0) : newNote());
    QString path = directory;
    if (path.startsWith("~/")) path.replace(0, 1, QDir::homePath());
    if (path.isEmpty())
        path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + "/traymd/notes";
    path = QDir::cleanPath(path);
    if (!QDir::isAbsolutePath(path)) return fail("Notes directory must be an absolute path.");
    if (!QDir().mkpath(path)) return fail("Cannot create notes directory: " + path);
    m_directory = path;
    auto entries = QDir(path).entryInfoList({"*.md"}, QDir::Files, QDir::Name);
    auto created = [](const QFileInfo &file) {
        const auto match = QRegularExpression("^(\\d{8}_\\d{6})(?:_(\\d{3}))?").match(file.fileName());
        if (match.hasMatch()) {
            auto date = QDateTime::fromString(match.captured(1), "yyyyMMdd_HHmmss");
            if (date.isValid()) return date.addMSecs(match.captured(2).toInt());
        }
        return file.birthTime().isValid() ? file.birthTime() : file.lastModified();
    };
    std::stable_sort(entries.begin(), entries.end(), [&](const auto &a, const auto &b) { return created(a) < created(b); });
    for (const auto &entry : entries) m_names.append(entry.fileName());
    if (m_names.isEmpty()) return newNote();
    QSettings state(QSettings::IniFormat, QSettings::UserScope, "traymd-omarchy", "notes");
    const int last = m_names.indexOf(state.value(m_directory).toString());
    return load(last >= 0 ? last : count() - 1);
}

bool NoteStore::load(int index) {
    if (index < 0 || index >= count()) return false;
    QFile file(QDir(m_directory).filePath(m_names.at(index)));
    if (!file.open(QIODevice::ReadOnly))
        return fail("Cannot read note: " + file.errorString());
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError) return fail("Cannot read note: " + file.errorString());
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString content = decoder(bytes);
    if (decoder.hasError()) return fail("This note is not valid UTF-8; it has been left untouched.");
    m_index = index;
    m_text = content;
    m_dirty = false;
    clearError();
    QSettings state(QSettings::IniFormat, QSettings::UserScope, "traymd-omarchy", "notes");
    state.setValue(m_directory, currentName());
    emit currentChanged();
    emit textChanged();
    emit dirtyChanged();
    return true;
}

void NoteStore::setText(const QString &text) {
    if (!ready() || text == m_text) return;
    m_text = text;
    m_dirty = true;
    emit textChanged();
    emit dirtyChanged();
    m_autosave.start();
}

bool NoteStore::save() {
    m_autosave.stop();
    if (!m_dirty) return true;
    QSaveFile file(QDir(m_directory).filePath(currentName()));
    if (!file.open(QIODevice::WriteOnly)) return fail("Could not save: " + file.errorString());
    const auto bytes = m_text.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail("Could not save: " + file.errorString());
    m_dirty = false;
    clearError();
    emit dirtyChanged();
    return true;
}

bool NoteStore::newNote() {
    if (m_directory.isEmpty() || !save()) return false;
    const QString name = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")
        + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".md";
    QFile file(QDir(m_directory).filePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return fail("Cannot create note: " + file.errorString());
    file.close();
    m_names.append(name);
    return load(count() - 1);
}

bool NoteStore::navigate(int delta) {
    const int index = m_index + delta;
    if (index < 0 || index >= count() || !save()) return false;
    return load(index);
}

bool NoteStore::deleteCurrent() {
    if (!ready() || !save()) return false;
    const auto trash = QDir(m_directory).filePath(".trash");
    if (!QDir().mkpath(trash)) return fail("Cannot create note trash folder.");
    QString target = QDir(trash).filePath(currentName());
    if (QFile::exists(target)) target += "." + QUuid::createUuid().toString(QUuid::Id128);
    if (!QFile::rename(QDir(m_directory).filePath(currentName()), target))
        return fail("Cannot move this note to .trash; the note has been kept.");
    const int next = qMin(m_index, count() - 2);
    m_names.removeAt(m_index);
    m_index = -1;
    m_text.clear();
    emit currentChanged();
    emit textChanged();
    return count() ? load(next) : newNote();
}
