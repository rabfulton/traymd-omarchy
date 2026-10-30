#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

// One instance per QML engine, shared by bar widgets on every monitor.
class NoteStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
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
    QString text() const { return m_text; }
    QString currentName() const;
    int currentIndex() const { return m_index; }
    int count() const { return m_names.size(); }
    bool dirty() const { return m_dirty; }
    bool ready() const { return m_index >= 0; }
    QString error() const { return m_error; }
    QString directory() const { return m_directory; }
    void setText(const QString &text);
    Q_INVOKABLE bool initialize(const QString &directory = QString());
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool newNote();
    Q_INVOKABLE bool navigate(int delta);
    Q_INVOKABLE bool deleteCurrent();
signals:
    void textChanged();
    void currentChanged();
    void dirtyChanged();
    void errorChanged();
private:
    bool load(int index);
    bool fail(const QString &message);
    void clearError();
    QString m_directory, m_text, m_error;
    QStringList m_names;
    int m_index = -1;
    bool m_dirty = false;
    QTimer m_autosave;
};
