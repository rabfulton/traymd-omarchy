#pragma once

#include <QObject>
#include <QTextDocument>
#include <QTimer>

// One file, one document, one save destination for the document's lifetime.
// Views forward edits to this document; their layouts remain private.
class NoteDocument : public QObject {
    Q_OBJECT
public:
    explicit NoteDocument(const QString &path, const QByteArray &bytes, const QString &text, QObject *parent);
    QTextDocument *document() { return &m_document; }
    const QString &path() const { return m_path; }
    bool dirty() const { return m_dirty; }
    QString error() const { return m_error; }
    bool save();
    bool reload();
signals:
    void textChanged();
    void dirtyChanged();
    void errorChanged();
private:
    bool fail(const QString &error);
    const QString m_path;
    QTextDocument m_document;
    QByteArray m_diskBytes;
    QString m_savedText, m_observedText, m_error;
    bool m_dirty = false;
    bool m_loading = false;
    QTimer m_autosave;
};
