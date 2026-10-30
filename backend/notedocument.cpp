#include "notedocument.h"
#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>

NoteDocument::NoteDocument(const QString &path, const QByteArray &bytes, const QString &text, QObject *parent)
    : QObject(parent), m_path(path), m_diskBytes(bytes) {
    m_document.setPlainText(text);
    m_document.setModified(false);
    m_savedText = m_observedText = m_document.toPlainText();
    m_autosave.setSingleShot(true);
    m_autosave.setInterval(1500);
    connect(&m_autosave, &QTimer::timeout, this, &NoteDocument::save);
    connect(&m_document, &QTextDocument::contentsChanged, this, [this] {
        const auto text = m_document.toPlainText();
        if (text == m_observedText) return; // Formatting is not a content edit.
        m_observedText = text;
        emit textChanged();
        if (m_loading) return;
        m_dirty = text != m_savedText;
        emit dirtyChanged();
        if (m_dirty) m_autosave.start();
        else m_autosave.stop();
    });
}

bool NoteDocument::fail(const QString &error) {
    m_error = error;
    emit errorChanged();
    return false;
}

bool NoteDocument::save() {
    m_autosave.stop();
    if (!m_dirty) return true;
    QFile disk(m_path);
    if (!disk.open(QIODevice::ReadOnly)) return fail("Cannot read the note before saving: " + disk.errorString());
    const auto bytes = disk.readAll();
    if (disk.error() != QFile::NoError) return fail("Cannot read note: " + disk.errorString());
    // This is an actual external-edit conflict, not an editor lifecycle event.
    // Neither writer's changes can silently take precedence over the other.
    if (bytes != m_diskBytes)
        return fail("This file changed outside the app while you were editing. Your pending edits remain open; the external file has been kept.");
    disk.close();
    const auto text = m_document.toPlainText();
    const auto output = text.toUtf8();
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size() || !file.commit())
        return fail("Could not save: " + file.errorString());
    m_diskBytes = output;
    m_savedText = text;
    m_dirty = false;
    m_error.clear();
    emit dirtyChanged();
    emit errorChanged();
    return true;
}

bool NoteDocument::reload() {
    if (m_dirty) return true; // Save resolves any simultaneous external edits.
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return fail("Cannot read note: " + file.errorString());
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError) return fail("Cannot read note: " + file.errorString());
    if (bytes == m_diskBytes) return true;
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    if (decoder.hasError()) return fail("This note is not valid UTF-8; it has been left untouched.");
    m_loading = true;
    m_document.setPlainText(text);
    m_document.setModified(false);
    m_loading = false;
    m_diskBytes = bytes;
    m_savedText = m_observedText = m_document.toPlainText();
    m_error.clear();
    emit errorChanged();
    return true;
}
