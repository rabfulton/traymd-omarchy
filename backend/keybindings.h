#pragma once
#include <QObject>
#include <QVariantMap>
#include <QKeySequence>
#include <QFileSystemWatcher>
#include <QTimer>

class Keybindings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)
public:
    explicit Keybindings(const QString &path = {}, QObject *parent = nullptr);
    QVariantMap shortcuts() const { return m_shortcuts; }
    QString error() const { return m_error; }
    QString path() const { return m_path; }
    Q_INVOKABLE bool matches(const QString &action, int key, int modifiers) const;
    Q_INVOKABLE void reload();
signals:
    void changed();
private:
    QString m_path, m_error;
    QVariantMap m_shortcuts;
    QMap<QString, QKeySequence> m_sequences;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
};
