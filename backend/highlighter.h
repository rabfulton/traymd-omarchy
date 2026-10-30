#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QVariantList>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY paletteChanged)
    Q_PROPERTY(QColor muted READ muted WRITE setMuted NOTIFY paletteChanged)
    Q_PROPERTY(QColor codeBackground READ codeBackground WRITE setCodeBackground NOTIFY paletteChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY paletteChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(QVariantList decorations READ decorations NOTIFY decorationsChanged)
    Q_PROPERTY(QVariantList headingColors READ headingColors WRITE setHeadingColors NOTIFY paletteChanged)
public:
    explicit MarkdownHighlighter(QObject *parent = nullptr);
    QQuickTextDocument *textDocument() const { return m_quickDocument; }
    void setTextDocument(QQuickTextDocument *document);
    QColor accent() const { return m_accent; }
    QColor muted() const { return m_muted; }
    QColor codeBackground() const { return m_codeBackground; }
    int fontSize() const { return m_fontSize; }
    void setAccent(const QColor &color);
    void setMuted(const QColor &color);
    void setCodeBackground(const QColor &color);
    void setFontSize(int size);
    int cursorPosition() const { return m_cursorPosition; }
    void setCursorPosition(int position);
    QVariantList decorations() const;
    QVariantList headingColors() const { return m_headingColors; }
    void setHeadingColors(const QVariantList &colors);
    Q_INVOKABLE int moveCursor(int position, int direction) const;
    Q_INVOKABLE int editKey(int position, int start, int end, int key);
    Q_INVOKABLE QString linkAt(int position) const;
    Q_INVOKABLE bool toggleTask(int position);
    Q_INVOKABLE int taskContentPosition(int position) const;
signals:
    void textDocumentChanged();
    void paletteChanged();
    void cursorPositionChanged();
    void decorationsChanged();
protected:
    void highlightBlock(const QString &text) override;
private:
    void mergeSpan(int start, int length, const QTextCharFormat &style);
    void hide(int start, int length);
    QPointer<QQuickTextDocument> m_quickDocument;
    QColor m_accent{"#7aa2f7"}, m_muted{"#707880"}, m_codeBackground{"#24283b"};
    int m_fontSize = 16;
    int m_cursorPosition = 0;
    QVariantList m_headingColors;
};
