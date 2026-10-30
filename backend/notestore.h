#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QHash>
#include <QQuickTextDocument>
#include <QStringList>
#include <QTimer>
#include "notedocument.h"
class NoteEditor;

// One instance per QML engine, shared by bar widgets on every monitor.
class NoteStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY currentChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY currentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString directory READ directory NOTIFY currentChanged)
public:
    explicit NoteStore(QObject *parent = nullptr);
    ~NoteStore() override;
    QString text() const { return m_current ? m_current->document()->toPlainText() : QString(); }
    QTextDocument *document() const { return m_current ? m_current->document() : nullptr; }
    QString currentName() const;
    int currentIndex() const { return m_index; }
    int count() const { return m_names.size(); }
    bool dirty() const { return m_current && m_current->dirty(); }
    bool ready() const { return m_current != nullptr; }
    QString error() const { return m_current && !m_current->error().isEmpty() ? m_current->error() : m_error; }
    QString directory() const { return m_directory; }
    Q_INVOKABLE void attachEditor(QQuickTextDocument *editor);
    Q_INVOKABLE bool initialize(const QString &directory = QString());
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool newNote();
    Q_INVOKABLE bool navigate(int delta);
    Q_INVOKABLE bool deleteCurrent();
    Q_INVOKABLE bool refresh();
signals:
    void textChanged();
    void currentChanged();
    void dirtyChanged();
    void errorChanged();
private:
    bool load(int index);
    bool fail(const QString &message);
    void clearError();
    void watchFiles();
    QString m_directory, m_error;
    NoteDocument *m_current = nullptr;
    QHash<QString, NoteDocument *> m_documents;
    QHash<QQuickTextDocument *, NoteEditor *> m_editors;
    QStringList m_names;
    int m_index = -1;
    QFileSystemWatcher m_watcher;
    QTimer m_rescan;
};
