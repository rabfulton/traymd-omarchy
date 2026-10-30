#pragma once
#include <QObject>

class ShortcutManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit ShortcutManager(const QString &path = {}, const QString &command = "hyprctl", QObject *parent = nullptr);
    QString current() const { return m_current; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool setShortcut(const QString &keys);
signals:
    void changed();
private:
    QString m_path, m_command, m_current, m_error;
    bool fail(const QString &error);
    bool run(const QStringList &args, QByteArray *output = nullptr);
};
