#include "notestore.h"
#include "noteeditor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUuid>
#include <QRegularExpression>
#include <algorithm>

namespace {
bool ensurePrivateDirectory(const QString &path) {
    const auto permissions = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
    // mkdir's permissions overload works on Qt 6.9 too. Only the store's own
    // directory is restricted; shared parents such as XDG_DATA_HOME are kept.
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    if (!QDir().mkdir(path, permissions) && !QFileInfo(path).isDir()) return false;
    // Also protect existing notes and trash without walking their contents.
    return QFile::setPermissions(path, permissions);
}
}

NoteStore::NoteStore(QObject *parent) : QObject(parent) {
    m_rescan.setSingleShot(true);
    m_rescan.setInterval(100);
    connect(&m_rescan, &QTimer::timeout, this, &NoteStore::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescan.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_rescan.start(); });
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
    if (!m_directory.isEmpty()) return refresh();
    QString path = directory;
    if (path.startsWith("~/")) path.replace(0, 1, QDir::homePath());
    if (path.isEmpty())
        path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + "/traymd/notes";
    path = QDir::cleanPath(path);
    if (!QDir::isAbsolutePath(path)) return fail("Notes directory must be an absolute path.");
    if (!ensurePrivateDirectory(path)) return fail("Cannot create or secure notes directory: " + path);
    m_directory = path;
    return refresh();
}

void NoteStore::watchFiles() {
    const auto old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty()) m_watcher.removePaths(old);
    if (!QFile::exists(m_directory)) return;
    m_watcher.addPath(m_directory);
    m_watcher.addPath(QFileInfo(m_directory).absolutePath());
    for (const auto &name : m_names) {
        const auto path = QDir(m_directory).filePath(name);
        if (QFile::exists(path)) m_watcher.addPath(path);
    }
}

bool NoteStore::refresh() {
    if (m_directory.isEmpty()) return false;
    if (!QDir(m_directory).exists()) return fail("The notes directory is unavailable.");
    const auto selected = currentName();
    const int previousIndex = m_index;
    auto entries = QDir(m_directory).entryInfoList({"*.md"}, QDir::Files, QDir::Name);
    auto created = [](const QFileInfo &file) {
        const auto match = QRegularExpression("^(\\d{8}_\\d{6})(?:_(\\d{3}))?").match(file.fileName());
        if (match.hasMatch()) {
            auto date = QDateTime::fromString(match.captured(1), "yyyyMMdd_HHmmss");
            if (date.isValid()) return date.addMSecs(match.captured(2).toInt());
        }
        return file.birthTime().isValid() ? file.birthTime() : file.lastModified();
    };
    std::stable_sort(entries.begin(), entries.end(), [&](const auto &a, const auto &b) { return created(a) < created(b); });
    QStringList names;
    for (const auto &entry : entries) names.append(entry.fileName());
    if (m_current && !names.contains(selected) && dirty()) {
        watchFiles();
        return fail("This note was removed outside the app. Its pending edits remain open.");
    }
    const bool changed = m_names != names;
    m_names = names;
    watchFiles();
    if (m_names.isEmpty()) { m_current = nullptr; m_index = -1; return newNote(); }
    if (!selected.isEmpty() && m_names.contains(selected)) {
        m_index = m_names.indexOf(selected);
        if (changed) emit currentChanged();
        return m_current->reload();
    }
    if (m_current) return load(qBound(0, previousIndex, count() - 1));
    QSettings state(QSettings::IniFormat, QSettings::UserScope, "traymd-omarchy", "notes");
    const int last = m_names.indexOf(state.value(m_directory).toString());
    return load(last >= 0 ? last : count() - 1);
}

bool NoteStore::load(int index) {
    if (index < 0 || index >= count()) return false;
    const QString name = m_names.at(index);
    if (auto *existing = m_documents.value(name)) {
        if (!existing->reload()) return false;
        m_current = existing;
    } else {
        const QString path = QDir(m_directory).filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return fail("Cannot read note: " + file.errorString());
        const auto bytes = file.readAll();
        if (file.error() != QFile::NoError) return fail("Cannot read note: " + file.errorString());
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString content = decoder(bytes);
        if (decoder.hasError()) return fail("This note is not valid UTF-8; it has been left untouched.");
        auto *note = new NoteDocument(path, bytes, content, this);
        m_documents.insert(name, note);
        connect(note, &NoteDocument::textChanged, this, [this, note] { if (note == m_current) emit textChanged(); });
        connect(note, &NoteDocument::dirtyChanged, this, [this, note] { if (note == m_current) emit dirtyChanged(); });
        connect(note, &NoteDocument::errorChanged, this, [this, note] { if (note == m_current) emit errorChanged(); });
        m_current = note;
    }
    m_index = index;
    clearError();
    QSettings state(QSettings::IniFormat, QSettings::UserScope, "traymd-omarchy", "notes");
    state.setValue(m_directory, currentName());
    emit currentChanged();
    emit textChanged();
    emit dirtyChanged();
    return true;
}

void NoteStore::attachEditor(QQuickTextDocument *editor) {
    if (!editor || !m_current) return;
    // Disconnect the old note before initializing the view for the next one.
    if (auto *previous = m_editors.take(editor)) delete previous;
    auto *attachment = new NoteEditor(editor, m_current);
    m_editors.insert(editor, attachment);
    connect(attachment, &QObject::destroyed, this, [this, editor, attachment] {
        if (m_editors.value(editor) == attachment) m_editors.remove(editor);
    });
}

bool NoteStore::save() {
    bool success = true;
    for (auto *note : m_documents) if (!note->save()) success = false;
    return success;
}

bool NoteStore::newNote() {
    if (m_directory.isEmpty() || !save()) return false;
    const QString name = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")
        + "_" + QUuid::createUuid().toString(QUuid::Id128).left(8) + ".md";
    QFile file(QDir(m_directory).filePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly, QFile::ReadOwner | QFile::WriteOwner))
        return fail("Cannot create note: " + file.errorString());
    file.close();
    m_names.append(name);
    watchFiles();
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
    if (!ensurePrivateDirectory(trash)) return fail("Cannot create or secure note trash folder.");
    QString target = QDir(trash).filePath(currentName());
    if (QFile::exists(target)) target += "." + QUuid::createUuid().toString(QUuid::Id128);
    if (!QFile::rename(QDir(m_directory).filePath(currentName()), target))
        return fail("Cannot move this note to .trash; the note has been kept.");
    const int next = qMin(m_index, count() - 2);
    m_names.removeAt(m_index);
    m_index = -1;
    m_current = nullptr;
    watchFiles();
    return count() ? load(next) : newNote();
}
