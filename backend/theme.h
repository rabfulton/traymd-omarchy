#pragma once
#include <QObject>
#include <QVariantMap>
#include <QFileSystemWatcher>
#include <QTimer>

class ThemePalette : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY colorsChanged)
public:
    explicit ThemePalette(const QString &path = {}, QObject *parent = nullptr);
    QVariantMap colors() const { return m_colors; }
    Q_INVOKABLE void reload();
signals:
    void colorsChanged();
private:
    QString m_path;
    QVariantMap m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
};
