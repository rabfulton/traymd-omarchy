#include "highlighter.h"
#include "notestore.h"
#include "theme.h"
#include "shortcut.h"
#include "keybindings.h"
#include <QQmlExtensionPlugin>
#include <qqml.h>

class TrayMDPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override {
        qmlRegisterType<MarkdownHighlighter>(uri, 1, 0, "MarkdownHighlighter");
        qmlRegisterSingletonType<Keybindings>(uri, 1, 0, "Keybindings",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new Keybindings; });
        qmlRegisterSingletonType<ShortcutManager>(uri, 1, 0, "Shortcut",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new ShortcutManager; });
        qmlRegisterSingletonType<ThemePalette>(uri, 1, 0, "Theme",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new ThemePalette; });
        qmlRegisterSingletonType<NoteStore>(uri, 1, 0, "Notes",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new NoteStore; });
    }
};

#include "plugin.moc"
