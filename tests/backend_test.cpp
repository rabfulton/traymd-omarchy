#include "highlighter.h"
#include "notestore.h"
#include "theme.h"
#include "shortcut.h"
#include "keybindings.h"
#include <QSaveFile>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>

class BackendTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static void write(const QString &path, const QByteArray &bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }
    static QByteArray read(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }
    static QTextCharFormat styleAt(const QTextDocument &document, int position) {
        const auto block = document.findBlock(position);
        for (const auto &range : block.layout()->formats())
            if (position - block.position() >= range.start && position - block.position() < range.start + range.length)
                return range.format;
        return {};
    }
    static QString visibleText(const QTextDocument &document) {
        const auto text = document.toPlainText();
        QString visible;
        for (int i = 0; i < text.size(); ++i) {
            const auto style = styleAt(document, i);
            if (!style.hasProperty(QTextFormat::ForegroundBrush) || style.foreground().color().alpha() != 0)
                visible += text[i];
        }
        return visible;
    }
private slots:
    void initTestCase() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }

    void legacyMarkdownRoundTrip() {
        QTemporaryDir dir;
        const QByteArray original = QString::fromUtf8("# Shopping 📝\n\n**Bold** and *italic*\n- café\n\n```c\nint i = 0;\n```\n").toUtf8();
        const auto path = dir.filePath("20261030_120000.md");
        write(path, original);
        NoteStore store;
        QVERIFY(store.initialize(dir.path()));
        QCOMPARE(store.count(), 1);
        QCOMPARE(store.currentName(), "20261030_120000.md");
        QCOMPARE(store.text().toUtf8(), original);
        QVERIFY(store.save());
        QCOMPARE(read(path), original);
        store.setText(store.text() + "\nAnother thought\n");
        QTRY_VERIFY_WITH_TIMEOUT(!store.dirty(), 1500);
        QCOMPARE(read(path), original + "\nAnother thought\n");
    }

    void createsUniqueNotesAndFlushesBeforeNavigation() {
        QTemporaryDir dir;
        NoteStore store;
        QVERIFY(store.initialize(dir.path()));
        const auto first = store.currentName();
        store.setText("Remember this");
        QVERIFY(store.newNote());
        QVERIFY(store.currentName() != first);
        QCOMPARE(store.count(), 2);
        QCOMPARE(read(dir.filePath(first)), "Remember this");
        store.setText("Second note");
        QVERIFY(store.navigate(-1));
        QCOMPARE(store.text(), "Remember this");
        QVERIFY(!store.navigate(-1));
        QVERIFY(store.navigate(1));
        QCOMPARE(store.text(), "Second note");
    }

    void saveFailureKeepsTextAndBlocksNavigation() {
        QTemporaryDir dir;
        const auto notes = dir.filePath("notes");
        NoteStore store;
        QVERIFY(store.initialize(notes));
        QVERIFY(store.newNote());
        const auto current = store.currentName();
        store.setText("Do not lose this");
        QVERIFY(QDir().rename(notes, dir.filePath("moved")));
        QVERIFY(!store.save());
        QVERIFY(!store.error().isEmpty());
        QVERIFY(store.dirty());
        QVERIFY(!store.navigate(-1));
        QCOMPARE(store.currentName(), current);
        QCOMPARE(store.text(), "Do not lose this");
        QVERIFY(QDir().rename(dir.filePath("moved"), notes));
        QVERIFY(store.save());
        QVERIFY(store.error().isEmpty());
        QCOMPARE(read(notes + "/" + current), "Do not lose this");
    }

    void invalidUtf8IsNeverRewritten() {
        QTemporaryDir dir;
        const auto path = dir.filePath("invalid.md");
        const QByteArray invalid("\xff\xfe\x80", 3);
        write(path, invalid);
        NoteStore store;
        QVERIFY(!store.initialize(dir.path()));
        QVERIFY(!store.ready());
        store.setText("replacement");
        QVERIFY(store.save());
        QCOMPARE(read(path), invalid);
    }

    void restoresLastNoteAndSavesOnDestruction() {
        QTemporaryDir dir;
        QString name;
        {
            NoteStore store;
            QVERIFY(store.initialize(dir.path()));
            name = store.currentName();
            QVERIFY(store.newNote());
            QVERIFY(store.navigate(-1));
            store.setText("Still here");
        }
        NoteStore reopened;
        QVERIFY(reopened.initialize(dir.path()));
        QCOMPARE(reopened.currentName(), name);
        QCOMPARE(reopened.text(), "Still here");
    }

    void formattingAndThemeChangesPreserveSourceAndUndo() {
        QTextDocument document;
        document.setPlainText("# Heading\n**bold** and `code`\n```c\n# not a heading\n```\n# Heading again");
        MarkdownHighlighter highlighter;
        highlighter.setDocument(&document);
        highlighter.rehighlight();
        const auto original = document.toPlainText();
        auto first = document.firstBlock().layout()->formats();
        QVERIFY(!first.isEmpty());
        QCOMPARE(styleAt(document, 2).fontWeight(), int(QFont::Bold));
        const auto codeBlock = document.findBlockByNumber(3);
        QVERIFY(codeBlock.userState() > 0);
        QCOMPARE(document.findBlockByNumber(4).userState(), 0);
        QTextCursor cursor(&document);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("!");
        highlighter.setAccent(QColor("#ff0000"));
        QCOMPARE(document.toPlainText(), original + "!");
        first = document.firstBlock().layout()->formats();
        QCOMPARE(styleAt(document, 2).foreground().color(), QColor("#ff0000"));
        QVERIFY(document.isUndoAvailable());
        document.undo();
        QCOMPARE(document.toPlainText(), original);
    }

    void typingHeadingsRendersContentWithoutMarkers() {
        for (int level = 1; level <= 3; ++level) {
            QTextDocument document;
            document.setTextWidth(500);
            MarkdownHighlighter renderer;
            renderer.setDocument(&document);
            QTextCursor cursor(&document);
            const auto source = QString(level, '#') + " heading";
            for (const auto ch : source) {
                cursor.insertText(QString(ch));
                renderer.setCursorPosition(cursor.position());
                renderer.rehighlight();
            }
            QCOMPARE(document.toPlainText(), source);
            QCOMPARE(visibleText(document), "heading");
            const auto title = styleAt(document, level + 1);
            QCOMPARE(title.fontWeight(), int(QFont::Bold));
            QVERIFY(title.property(QTextFormat::FontPixelSize).toDouble() > renderer.fontSize());
            document.documentLayout()->documentSize();
            const auto line = document.firstBlock().layout()->lineAt(0);
            QVERIFY(line.isValid());
            // Hidden prefixes must collapse, not leave an invisible indent.
            QVERIFY(qAbs(line.cursorToX(level + 1) - line.cursorToX(0)) < 1.0);
            QCOMPARE(renderer.moveCursor(0, 0), level + 1);
            const int result = renderer.editKey(level + 1, level + 1, level + 1, Qt::Key_Backspace);
            QCOMPARE(result, 0);
            QCOMPARE(document.toPlainText(), "heading");
            document.undo();
            QCOMPARE(document.toPlainText(), source);
        }
    }

    void inlineRenderingKeepsMarkdownAndLinkTargets() {
        QTextDocument document;
        const QString source = "**bold** *italic* ***both*** `**code**` [label](https://example.com)";
        document.setPlainText(source);
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.rehighlight();
        QCOMPARE(visibleText(document), "bold italic both **code** label");
        QCOMPARE(document.toPlainText(), source);
        QCOMPARE(styleAt(document, source.indexOf("bold")).fontWeight(), int(QFont::Bold));
        QVERIFY(styleAt(document, source.indexOf("italic")).fontItalic());
        const auto both = styleAt(document, source.indexOf("both"));
        QVERIFY(both.fontItalic());
        QCOMPARE(both.fontWeight(), int(QFont::Bold));
        QCOMPARE(renderer.linkAt(source.indexOf("label")), "https://example.com");
        QVERIFY(renderer.linkAt(-1).isEmpty());
        QVERIFY(renderer.linkAt(document.characterCount()).isEmpty());
        QVERIFY(renderer.linkAt(source.indexOf("code")).isEmpty());
        QCOMPARE(renderer.moveCursor(1, 1), 2);
        QCOMPARE(renderer.moveCursor(7, -1), 6);
    }

    void listsContinueExitAndUndoWithoutAffectingCode() {
        for (const QString &source : {QString("- first"), QString("* first"), QString("9. first")}) {
            QTextDocument document;
            document.setPlainText(source);
            MarkdownHighlighter renderer;
            renderer.setDocument(&document);
            renderer.rehighlight();
            int pos = source.size();
            pos = renderer.editKey(pos, pos, pos, Qt::Key_Return);
            QVERIFY(pos > source.size());
            const QString prefix = source.startsWith("9") ? "10. " : source.left(2);
            QCOMPARE(document.toPlainText(), source + "\n" + prefix);
            pos = renderer.editKey(pos, pos, pos, Qt::Key_Return);
            QCOMPARE(document.toPlainText(), source + "\n");
            document.undo();
            QCOMPARE(document.toPlainText(), source + "\n" + prefix);
        }
        QTextDocument document;
        document.setPlainText("```python\n- code\n```\n");
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.setCursorPosition(document.toPlainText().size());
        renderer.rehighlight();
        QCOMPARE(renderer.editKey(16, 16, 16, Qt::Key_Return), -1);
        QVERIFY(!visibleText(document).contains("```"));
        QVERIFY(visibleText(document).contains("- code"));
        renderer.setCursorPosition(0);
        QVERIFY(visibleText(document).startsWith("```python"));
    }

    void quotesBulletsAndRulesRenderInPlace() {
        QTextDocument document;
        document.setPlainText("> quotation\n- item\n---\n");
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.setCursorPosition(document.toPlainText().size());
        renderer.rehighlight();
        QVERIFY(visibleText(document).startsWith("quotation"));
        QVERIFY(!visibleText(document).contains("---"));
        QCOMPARE(renderer.decorations().size(), 2);
        QCOMPARE(renderer.decorations()[0].toMap()["kind"].toString(), "bullet");
        QCOMPARE(renderer.decorations()[1].toMap()["kind"].toString(), "rule");
        renderer.setCursorPosition(document.toPlainText().indexOf("---"));
        QVERIFY(visibleText(document).contains("---"));
    }

    void originalCodeScannerPreservesCommentsAndUnicodeOffsets() {
        QTextDocument document;
        const QString source = "```c\n/*\nint ignored;\n*/\nint value; // return\n\"📝\"; int unicode;\n```\n";
        document.setPlainText(source);
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.rehighlight();
        QCOMPARE(styleAt(document, source.indexOf("int ignored")).fontWeight(), int(QFont::Normal));
        QCOMPARE(styleAt(document, source.indexOf("int value")).fontWeight(), int(QFont::Bold));
        QCOMPARE(styleAt(document, source.indexOf("return")).fontWeight(), int(QFont::Normal));
        QCOMPARE(styleAt(document, source.indexOf("int unicode")).fontWeight(), int(QFont::Bold));
        QCOMPARE(document.toPlainText(), source);
    }

    void deletionMovesSavedNoteToTrashAndKeepsAnEditableNote() {
        QTemporaryDir dir;
        NoteStore store;
        QVERIFY(store.initialize(dir.path()));
        const auto first = store.currentName();
        store.setText("keep me recoverable");
        QVERIFY(store.newNote());
        const auto second = store.currentName();
        store.setText("newer note");
        QVERIFY(store.navigate(-1));
        QCOMPARE(store.currentName(), first);
        QVERIFY(store.deleteCurrent());
        QCOMPARE(read(dir.filePath(".trash/" + first)), "keep me recoverable");
        QVERIFY(!QFile::exists(dir.filePath(first)));
        QCOMPARE(store.currentName(), second);
        QVERIFY(store.deleteCurrent());
        QCOMPARE(read(dir.filePath(".trash/" + second)), "newer note");
        QCOMPARE(store.count(), 1);
        QVERIFY(store.ready());
        QVERIFY(store.text().isEmpty());
    }

    void failedDeletionKeepsCurrentNote() {
        QTemporaryDir dir;
        NoteStore store;
        QVERIFY(store.initialize(dir.path()));
        store.setText("retained");
        write(dir.filePath(".trash"), "blocking file");
        const auto name = store.currentName();
        QVERIFY(!store.deleteCurrent());
        QCOMPARE(store.text(), "retained");
        QCOMPARE(store.currentName(), name);
        QCOMPARE(read(dir.filePath(name)), "retained");
    }

    void timestampOrderingSurvivesEditsAndRestarts() {
        QTemporaryDir dir;
        write(dir.filePath("20200101_120000.md"), "older");
        write(dir.filePath("20250101_120000.md"), "newer");
        NoteStore store;
        QVERIFY(store.initialize(dir.path()));
        QCOMPARE(store.text(), "newer");
        QVERIFY(store.navigate(-1));
        QCOMPARE(store.text(), "older");
        store.setText("edited older");
        QVERIFY(store.save());
        NoteStore reopened;
        QVERIFY(reopened.initialize(dir.path()));
        QCOMPARE(reopened.text(), "edited older");
        QVERIFY(reopened.navigate(1));
        QCOMPARE(reopened.text(), "newer");
    }

    void codeBlockIsOneRegionAndHeadingsUseThreeColors() {
        QTextDocument document;
        document.setPlainText("# One\n## Two\n### Three\n```\nline one\n\nline three\n```\n");
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.setHeadingColors({"#112233", "#445566", "#778899"});
        renderer.rehighlight();
        QCOMPARE(styleAt(document, 2).foreground().color(), QColor("#112233"));
        QCOMPARE(styleAt(document, 9).foreground().color(), QColor("#445566"));
        QCOMPARE(styleAt(document, 20).foreground().color(), QColor("#778899"));
        const auto regions = renderer.decorations();
        QCOMPARE(regions.size(), 1);
        const auto region = regions.first().toMap();
        QCOMPARE(region["kind"].toString(), "code");
        QCOMPARE(region["position"].toInt(), document.toPlainText().indexOf("line one"));
        QVERIFY(region["endPosition"].toInt() > document.toPlainText().indexOf("line three"));
        QVERIFY(styleAt(document, region["position"].toInt()).fontFamilies().toStringList().contains("monospace"));
        renderer.setHeadingColors({"#aa1122", "#bb3344", "#cc5566"});
        QCOMPARE(styleAt(document, 9).foreground().color(), QColor("#bb3344"));
    }

    void themePaletteRewatchesReplacedDirectory() {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("theme")));
        const auto path = dir.filePath("theme/colors.toml");
        write(path, "blue = \"#123456\"\nmagenta = \"#abcdef\"\n");
        ThemePalette palette(path);
        QCOMPARE(palette.colors()["blue"].toString(), "#123456");
        QVERIFY(QDir().rename(dir.filePath("theme"), dir.filePath("previous")));
        QVERIFY(QDir().mkpath(dir.filePath("theme")));
        write(path, "blue = \"#654321\"\nmagenta = \"#fedcba\"\n");
        QTRY_COMPARE_WITH_TIMEOUT(palette.colors()["blue"].toString(), "#654321", 2000);
        write(path, "blue = \"#aabbcc\"\n");
        QTRY_COMPARE_WITH_TIMEOUT(palette.colors()["blue"].toString(), "#aabbcc", 2000);
    }

    void globalShortcutPersistsAndRejectsConflicts() {
        QTemporaryDir dir;
        const auto bindings = dir.filePath("bindings.lua");
        const QByteArray original("-- personal settings\no.bind(\"SUPER + M\", \"Master\", \"some-command\")\n");
        write(bindings, original);
        write(dir.filePath("binds.json"), "[]");
        write(dir.filePath("errors"), "");
        const auto command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncase \"$1\" in\n-j) cat \"$(dirname \"$0\")/binds.json\" ;;\nreload) exit 0 ;;\nconfigerrors) cat \"$(dirname \"$0\")/errors\" ;;\nesac\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        ShortcutManager shortcut(bindings, command);
        QVERIFY(shortcut.current().isEmpty());
        QVERIFY(shortcut.setShortcut("SUPER + SHIFT + N"));
        QCOMPARE(shortcut.current(), "SUPER + SHIFT + N");
        QVERIFY(read(bindings).startsWith(original));
        ShortcutManager reopened(bindings, command);
        QCOMPARE(reopened.current(), shortcut.current());
        QVERIFY(shortcut.setShortcut("SUPER + CTRL + N"));
        QCOMPARE(read(bindings).count("BEGIN traymd-omarchy"), 1);
        const auto before = read(bindings);
        write(dir.filePath("binds.json"), "[{\"modmask\":65,\"key\":\"N\",\"arg\":\"another-app\"}]");
        QVERIFY(!shortcut.setShortcut("SUPER + SHIFT + N"));
        QCOMPARE(read(bindings), before);
        QVERIFY(!shortcut.setShortcut("SUPER + N\"); os.execute('bad')"));
        QCOMPARE(read(bindings), before);
        QVERIFY(!shortcut.setShortcut("CTRL + N"));
        write(dir.filePath("errors"), "test config error");
        QVERIFY(!shortcut.setShortcut("SUPER + ALT + N"));
        QCOMPARE(read(bindings), before);
        write(dir.filePath("errors"), "");
        QVERIFY(shortcut.setShortcut(""));
        QCOMPARE(read(bindings), original);
    }

    void inAppShortcutsCreateDefaultsAndRequireExactModifiers() {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.ini");
        Keybindings keys(path);
        QVERIFY(QFile::exists(path));
        QVERIFY(keys.error().isEmpty());
        QVERIFY(keys.matches("older_note", Qt::Key_Left, Qt::AltModifier));
        QVERIFY(keys.matches("newer_note", Qt::Key_Right, Qt::AltModifier));
        QVERIFY(!keys.matches("older_note", Qt::Key_PageUp, Qt::ControlModifier));
        QVERIFY(!keys.matches("older_note", Qt::Key_Left, Qt::ControlModifier));
        QVERIFY(keys.matches("new_note", Qt::Key_N, Qt::ControlModifier));
        QVERIFY(!keys.matches("new_note", Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier));
        QVERIFY(keys.matches("settings", Qt::Key_Comma, Qt::ControlModifier));
    }

    void inAppShortcutsReloadAtomicEditsAndRejectBadMaps() {
        QTemporaryDir dir;
        const auto path = dir.filePath("config.ini");
        Keybindings keys(path);
        QSaveFile replacement(path);
        QVERIFY(replacement.open(QIODevice::WriteOnly));
        const auto config = QByteArray("[shortcuts]\nolder_note=Ctrl+H\nnewer_note=Ctrl+L\nnew_note=\n");
        QCOMPARE(replacement.write(config), config.size());
        QVERIFY(replacement.commit());
        QTRY_VERIFY_WITH_TIMEOUT(keys.matches("older_note", Qt::Key_H, Qt::ControlModifier), 1500);
        QVERIFY(keys.matches("newer_note", Qt::Key_L, Qt::ControlModifier));
        QVERIFY(!keys.matches("new_note", Qt::Key_N, Qt::ControlModifier));
        QVERIFY(keys.matches("close", Qt::Key_W, Qt::ControlModifier));
        write(path, "[shortcuts]\nolder_note=Ctrl+H\nnewer_note=Ctrl+H\n");
        keys.reload();
        QVERIFY(!keys.error().isEmpty());
        QVERIFY(keys.matches("newer_note", Qt::Key_L, Qt::ControlModifier));
        write(path, "[shortcuts]\nolder_note=made-up-key\n");
        keys.reload();
        QVERIFY(!keys.error().isEmpty());
        QVERIFY(keys.matches("older_note", Qt::Key_H, Qt::ControlModifier));
        write(path, "[shortcuts]\nolder_note=Esc\n");
        keys.reload();
        QVERIFY(!keys.error().isEmpty());
    }

    void taskListsRenderToggleAndKeepSource() {
        QTextDocument document;
        const QString source = "- [ ] Pack **a notebook**\n  * [X] café 📝\n+ [x] done\n- [y] ordinary item";
        document.setPlainText(source);
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.rehighlight();
        const auto decorations = renderer.decorations();
        QCOMPARE(decorations.size(), 4);
        for (int i = 0; i < 3; ++i) QCOMPARE(decorations[i].toMap()["kind"].toString(), "task");
        QVERIFY(!decorations[0].toMap()["checked"].toBool());
        QVERIFY(decorations[1].toMap()["checked"].toBool());
        QVERIFY(visibleText(document).startsWith("Pack a notebook\n"));
        QVERIFY(styleAt(document, source.indexOf("café")).fontStrikeOut());
        QCOMPARE(renderer.moveCursor(0, 0), 6);
        QCOMPARE(renderer.taskContentPosition(4), 6);
        QVERIFY(renderer.toggleTask(0));
        QCOMPARE(document.toPlainText(), QString(source).replace(0, 6, "- [x] "));
        document.undo();
        QCOMPARE(document.toPlainText(), source);
        renderer.rehighlight();
        QVERIFY(renderer.toggleTask(source.indexOf("  *")));
        QVERIFY(document.toPlainText().contains("  * [ ] café 📝"));
        document.undo();
        QCOMPARE(document.toPlainText(), source);
        QVERIFY(!renderer.toggleTask(source.indexOf("- [y]")));
    }

    void taskListsContinueUncheckedAndExitEmptyItems() {
        for (const auto &source : {QString("- [ ] task"), QString("* [x] task"), QString("  + [X] task")}) {
            QTextDocument document;
            document.setPlainText(source);
            MarkdownHighlighter renderer;
            renderer.setDocument(&document);
            renderer.rehighlight();
            int pos = source.size();
            pos = renderer.editKey(pos, pos, pos, Qt::Key_Return);
            const QString indent = source.startsWith("  ") ? "  " : "";
            const auto marker = source.trimmed().left(1);
            const auto continued = source + "\n" + indent + marker + " [ ] ";
            QCOMPARE(document.toPlainText(), continued);
            pos = renderer.editKey(pos, pos, pos, Qt::Key_Return);
            QCOMPARE(document.toPlainText(), source + "\n" + indent);
            document.undo();
            QCOMPARE(document.toPlainText(), continued);
        }
        QTextDocument document;
        document.setPlainText("- [ ] task");
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.rehighlight();
        QCOMPARE(renderer.editKey(6, 6, 6, Qt::Key_Backspace), 0);
        QCOMPARE(document.toPlainText(), "task");
        document.undo();
        QCOMPARE(document.toPlainText(), "- [ ] task");
    }

    void tasksInsideCodeStayLiteral() {
        QTextDocument document;
        const QString source = "```markdown\n- [ ] literal\n```\n\ntext - [ ] literal";
        document.setPlainText(source);
        MarkdownHighlighter renderer;
        renderer.setDocument(&document);
        renderer.rehighlight();
        QCOMPARE(renderer.decorations().size(), 1);
        QCOMPARE(renderer.decorations().first().toMap()["kind"].toString(), "code");
        QVERIFY(!renderer.toggleTask(source.indexOf("- [ ]")));
        const int pos = source.indexOf("literal") + 7;
        QCOMPARE(renderer.editKey(pos, pos, pos, Qt::Key_Return), -1);
        QCOMPARE(document.toPlainText(), source);
    }
};

QTEST_MAIN(BackendTest)
#include "backend_test.moc"
