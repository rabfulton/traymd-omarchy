#include "highlighter.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
extern "C" {
#include "code_highlight.h"
}

MarkdownHighlighter::MarkdownHighlighter(QObject *parent) : QSyntaxHighlighter(parent) {}

void MarkdownHighlighter::setTextDocument(QQuickTextDocument *document) {
    if (document == m_quickDocument) return;
    m_quickDocument = document;
    setDocument(document ? document->textDocument() : nullptr);
    emit textDocumentChanged();
}

void MarkdownHighlighter::setAccent(const QColor &color) {
    if (color == m_accent) return;
    m_accent = color; rehighlight(); emit paletteChanged();
}
void MarkdownHighlighter::setMuted(const QColor &color) {
    if (color == m_muted) return;
    m_muted = color; rehighlight(); emit paletteChanged();
}
void MarkdownHighlighter::setCodeBackground(const QColor &color) {
    if (color == m_codeBackground) return;
    m_codeBackground = color; rehighlight(); emit paletteChanged();
}
void MarkdownHighlighter::setFontSize(int size) {
    if (size == m_fontSize) return;
    m_fontSize = size; rehighlight(); emit paletteChanged();
}

void MarkdownHighlighter::setHeadingColors(const QVariantList &colors) {
    if (colors == m_headingColors) return;
    m_headingColors = colors;
    rehighlight(); emit paletteChanged();
}

void MarkdownHighlighter::mergeSpan(int start, int length, const QTextCharFormat &style) {
    for (int i = start; i < start + length; ++i) {
        auto combined = format(i);
        combined.merge(style);
        setFormat(i, 1, combined);
    }
}

namespace {
struct Span { int start; int length; };
struct BlockData : QTextBlockUserData {
    bool code = false;
    QList<Span> hidden;
    QString decoration;
    int decorationOffset = 0;
    bool checked = false;
    int taskContentStart = 0;
    QString taskLabel;
    QString language;
    MarkydCodeScanState scanState{};
};
const BlockData *data(const QTextBlock &block) {
    return dynamic_cast<const BlockData *>(block.userData());
}
const QRegularExpression &taskExpression() {
    static const QRegularExpression expression(R"(^([ \t]*)([-*+]) +\[([ xX])\](?:[ \t]+(.*)|$))");
    return expression;
}
}

void MarkdownHighlighter::hide(int start, int length) {
    if (length <= 0) return;
    auto *info = static_cast<BlockData *>(currentBlockUserData());
    info->hidden.append({start, length});
    // Qt Quick has no GtkTextTag::invisible equivalent. Collapse only the
    // marker glyphs to subpixel advances, and paint them transparent. Source
    // characters stay in the document, so edits and undo never round-trip HTML.
    QTextCharFormat hidden;
    hidden.setFontPointSize(0.01);
    hidden.setForeground(Qt::transparent);
    hidden.setBackground(Qt::transparent);
    setFormat(start, length, hidden);
}

void MarkdownHighlighter::setCursorPosition(int position) {
    if (m_cursorPosition == position) return;
    const auto oldBlock = document() ? document()->findBlock(m_cursorPosition) : QTextBlock();
    m_cursorPosition = position;
    const auto newBlock = document() ? document()->findBlock(position) : QTextBlock();
    if (oldBlock != newBlock) {
        if (oldBlock.isValid()) rehighlightBlock(oldBlock);
        if (newBlock.isValid()) rehighlightBlock(newBlock);
    }
    emit cursorPositionChanged();
    emit decorationsChanged();
}

QVariantList MarkdownHighlighter::decorations() const {
    QVariantList result;
    if (!document()) return result;
    int codeStart = -1, codeEnd = -1;
    auto flush = [&] {
        if (codeStart >= 0) result.append(QVariantMap{{"position", codeStart}, {"endPosition", codeEnd}, {"kind", "code"}});
        codeStart = -1;
    };
    for (auto block = document()->begin(); block.isValid(); block = block.next()) {
        const auto *info = data(block);
        if (info && info->code) {
            if (codeStart < 0) codeStart = block.position();
            codeEnd = block.position() + block.length() - 1;
            continue;
        }
        flush();
        if (info && !info->decoration.isEmpty())
            result.append(QVariantMap{{"position", block.position() + info->decorationOffset}, {"kind", info->decoration},
                {"checked", info->checked}, {"label", info->taskLabel}});
    }
    flush();
    return result;
}

int MarkdownHighlighter::moveCursor(int position, int direction) const {
    if (!document()) return position;
    position = qBound(0, position, document()->characterCount() - 1);
    const auto block = document()->findBlock(position);
    const auto *info = data(block);
    if (!info) return position;
    if (info->decoration == "task" && position >= block.position() + info->decorationOffset
        && position < block.position() + info->taskContentStart)
        return direction < 0 ? qMax(0, block.position() - 1) : block.position() + info->taskContentStart;
    for (const auto &span : info->hidden) {
        const int start = block.position() + span.start;
        const int end = start + span.length;
        if (position >= start && position < end) {
            // Home/click at a hidden paragraph prefix lands before its content.
            if (span.start == 0 && start == block.position()) return direction < 0 ? qMax(0, start - 1) : end;
            return direction < 0 ? start : end;
        }
    }
    return position;
}

int MarkdownHighlighter::editKey(int position, int start, int end, int key) {
    if (!document() || start != end) return -1;
    auto block = document()->findBlock(position);
    const auto text = block.text();
    const int offset = position - block.position();
    QTextCursor cursor(document());
    cursor.setPosition(position);
    const auto *info = data(block);
    if (key == Qt::Key_Backspace && info && info->decoration == "task" && offset == info->taskContentStart) {
        cursor.beginEditBlock();
        cursor.setPosition(block.position() + info->decorationOffset);
        cursor.setPosition(position, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
        return cursor.position();
    }
    if (key == Qt::Key_Backspace && info) {
        for (const auto &span : info->hidden) {
            const int a = block.position() + span.start, b = a + span.length;
            if (position == b) {
                cursor.beginEditBlock();
                if (span.start == 0) {
                    cursor.setPosition(a); cursor.setPosition(b, QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                } else {
                    cursor.setPosition(a); cursor.deletePreviousChar();
                }
                cursor.endEditBlock();
                return cursor.position();
            }
        }
    }
    if (key != Qt::Key_Return && key != Qt::Key_Enter) return -1;
    // Fence lines and code content never continue Markdown list markers.
    if (block.userState() > 0 || QRegularExpression(R"(^\s*(`{3,}|~{3,}))").match(text).hasMatch()) return -1;
    const auto task = taskExpression().match(text);
    if (task.hasMatch()) {
        const int contentStart = task.capturedStart(4) >= 0 ? task.capturedStart(4) : text.size();
        if (offset < contentStart) return -1;
        cursor.beginEditBlock();
        if (task.captured(4).trimmed().isEmpty()) {
            cursor.setPosition(block.position() + task.capturedLength(1));
            cursor.setPosition(block.position() + text.size(), QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
        } else {
            cursor.insertText("\n" + task.captured(1) + task.captured(2) + " [ ] ");
        }
        cursor.endEditBlock();
        return cursor.position();
    }
    static const QRegularExpression list(R"(^(\s*)([-*+]|\d+\.) (.*)$)");
    const auto match = list.match(text);
    if (!match.hasMatch()) return -1;
    cursor.beginEditBlock();
    if (match.captured(3).isEmpty()) {
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + text.size(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (offset >= match.capturedStart(3)) {
        QString marker = match.captured(2);
        if (marker.endsWith('.')) marker = QString::number(marker.chopped(1).toInt() + 1) + ".";
        cursor.insertText("\n" + match.captured(1) + marker + " ");
    } else {
        cursor.endEditBlock(); return -1;
    }
    cursor.endEditBlock();
    return cursor.position();
}

QString MarkdownHighlighter::linkAt(int position) const {
    if (!document()) return {};
    const auto block = document()->findBlock(position);
    if (!block.isValid() || !block.layout()) return {};
    const int offset = position - block.position();
    for (const auto &range : block.layout()->formats()) {
        if (offset >= range.start && offset < range.start + range.length && range.format.isAnchor())
            return range.format.anchorHref();
    }
    return {};
}

bool MarkdownHighlighter::toggleTask(int position) {
    if (!document()) return false;
    const auto block = document()->findBlock(position);
    const auto *info = data(block);
    if (!info || info->decoration != "task") return false;
    const auto match = taskExpression().match(block.text());
    if (!match.hasMatch()) return false;
    QTextCursor cursor(document());
    cursor.beginEditBlock();
    cursor.setPosition(block.position() + match.capturedStart(3));
    cursor.setPosition(cursor.position() + 1, QTextCursor::KeepAnchor);
    cursor.insertText(match.captured(3) == " " ? "x" : " ");
    cursor.endEditBlock();
    return true;
}

int MarkdownHighlighter::taskContentPosition(int position) const {
    if (!document()) return position;
    const auto block = document()->findBlock(position);
    const auto *info = data(block);
    if (!info || info->decoration != "task") return position;
    const auto match = taskExpression().match(block.text());
    if (!match.hasMatch()) return position;
    const int content = match.capturedStart(4) >= 0 ? match.capturedStart(4) : block.text().size();
    return position >= block.position() + match.capturedLength(1) && position < block.position() + content
        ? block.position() + content : position;
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    auto *info = new BlockData;
    setCurrentBlockUserData(info);
    const bool active = document()->findBlock(m_cursorPosition) == currentBlock();
    static const QRegularExpression fence(R"(^\s{0,3}(`{3,}|~{3,})(.*)$)");
    const auto match = fence.match(text);
    const int previous = previousBlockState() > 0 ? previousBlockState() : 0;
    const int type = match.hasMatch() ? (match.captured(1).startsWith('`') ? 1 : 2) : 0;
    const int length = match.capturedLength(1);
    setCurrentBlockState(0);
    if (previous || type) {
        QTextCharFormat code;
        code.setFontFamilies({"monospace"});
        bool isFence = !previous;
        if (previous) {
            const bool closing = type == (previous & 3) && length >= ((previous & 0xffff) >> 2)
                && match.captured(2).trimmed().isEmpty();
            setCurrentBlockState(closing ? 0 : previous);
            isFence = closing;
            if (const auto *prev = data(currentBlock().previous())) {
                info->language = prev->language;
                info->scanState = prev->scanState;
            }
        } else {
            setCurrentBlockState((length << 2) | type);
            info->language = match.captured(2).trimmed().toLower();
        }
        setFormat(0, text.size(), code);
        if (isFence) {
            if (!active) hide(0, text.size());
        } else {
            // Share TrayMD's lexer, including comments and multiline strings.
            info->code = true;
            struct Context { MarkdownHighlighter *renderer; QList<int> offsets; } context{this, {0}};
            for (const auto point : text.toUcs4())
                context.offsets.append(context.offsets.last() + (point > 0xffff ? 2 : 1));
            markyd_code_scan_line(markyd_code_lookup_language(info->language.toUtf8().constData()),
                text.toUtf8().constData(), &info->scanState,
                [](gint start, gint end, const gchar *tag, gpointer opaque) {
                    auto *ctx = static_cast<Context *>(opaque);
                    const int a = ctx->offsets.at(start);
                    const int b = ctx->offsets.at(end);
                    QTextCharFormat style;
                    const QColor accent = ctx->renderer->accent();
                    style.setForeground(QString::fromUtf8(tag) == MARKYD_TAG_CODE_KW_A ? accent : accent.lighter(120));
                    if (QString::fromUtf8(tag) != MARKYD_TAG_CODE_LITERAL) style.setFontWeight(QFont::Bold);
                    ctx->renderer->mergeSpan(a, b - a, style);
                }, &context);
            // Propagate lexer-state edits to following blocks.
            if (currentBlockState() > 0)
                setCurrentBlockState((currentBlockState() & 0xffff) | (int(info->scanState.flags) << 16));
        }
        emit decorationsChanged();
        return;
    }

    static const QRegularExpression heading(R"(^(#{1,6}) (.*)$)");
    const auto h = heading.match(text);
    const auto task = taskExpression().match(text);
    int contentStart = 0;
    if (h.hasMatch()) {
        QTextCharFormat style;
        const int headingLevel = h.capturedLength(1);
        const QColor headingColor(m_headingColors.value((headingLevel - 1) % 3).toString());
        style.setForeground(headingColor.isValid() ? headingColor : m_accent);
        style.setFontWeight(QFont::Bold);
        const int level = h.capturedLength(1);
        const double scale = level == 1 ? 2.0 : level == 2 ? 1.6 : level == 3 ? 1.3 : 1.15;
        style.setProperty(QTextFormat::FontPixelSize, m_fontSize * scale);
        setFormat(0, text.size(), style);
        contentStart = h.capturedStart(2);
        info->hidden.append({0, contentStart});
    } else if (text.startsWith("> ")) {
        QTextCharFormat quote; quote.setForeground(m_muted); quote.setFontItalic(true);
        quote.setBackground(m_codeBackground);
        setFormat(0, text.size(), quote);
        contentStart = 2;
        info->hidden.append({0, 2});
    } else if (task.hasMatch()) {
        const int marker = task.capturedLength(1);
        contentStart = task.capturedStart(4) >= 0 ? task.capturedStart(4) : text.size();
        info->decoration = "task";
        info->decorationOffset = marker;
        info->checked = task.captured(3) != " ";
        info->taskContentStart = contentStart;
        info->taskLabel = task.captured(4);
        // Reserve a normal list-marker width for the checkbox, collapsing
        // the remaining Markdown syntax without changing source positions.
        QTextCharFormat markerStyle; markerStyle.setForeground(Qt::transparent);
        setFormat(marker, 2, markerStyle);
        info->hidden.append({marker + 2, contentStart - marker - 2});
        if (info->checked) {
            QTextCharFormat completed;
            completed.setFontStrikeOut(true);
            completed.setForeground(m_muted);
            setFormat(contentStart, text.size() - contentStart, completed);
        }
    } else if (QRegularExpression(R"(^\s*([-*])\s+)").match(text).hasMatch()) {
        const int marker = text.indexOf(QRegularExpression("[-*]"));
        QTextCharFormat bullet; bullet.setForeground(Qt::transparent);
        setFormat(marker, 1, bullet);
        info->decoration = "bullet";
        info->decorationOffset = marker;
        contentStart = marker + 2;
    } else if (QRegularExpression(R"(^\s*\d+\. )").match(text).hasMatch()) {
        const auto marker = QRegularExpression(R"(^\s*\d+\. )").match(text);
        QTextCharFormat number; number.setForeground(m_accent);
        setFormat(0, marker.capturedLength(), number);
        contentStart = marker.capturedLength();
    }
    if (!active && currentBlock().next().isValid() &&
        QRegularExpression(R"(^\s*(-{3,}|\*{3,})\s*$)").match(text).hasMatch()) {
        info->decoration = "rule";
        hide(0, text.size());
        emit decorationsChanged();
        return;
    }
    // Parse left-to-right, matching TrayMD's precedence. Inline code and link
    // destinations are opaque to emphasis parsing.
    for (int pos = contentStart; pos < text.size();) {
        const QString rest = text.mid(pos);
        QString delimiter;
        QTextCharFormat style;
        if (rest.startsWith("***")) { delimiter = "***"; style.setFontWeight(QFont::Bold); style.setFontItalic(true); }
        else if (rest.startsWith("**")) { delimiter = "**"; style.setFontWeight(QFont::Bold); }
        else if (rest.startsWith("__")) { delimiter = "__"; style.setFontWeight(QFont::Bold); }
        else if (rest.startsWith('*')) { delimiter = "*"; style.setFontItalic(true); }
        else if (rest.startsWith('_')) { delimiter = "_"; style.setFontItalic(true); }
        else if (rest.startsWith("~~")) { delimiter = "~~"; style.setFontStrikeOut(true); }
        else if (rest.startsWith('`')) { delimiter = "`"; style.setFontFamilies({"monospace"}); style.setBackground(m_codeBackground); }
        if (!delimiter.isEmpty()) {
            const int end = text.indexOf(delimiter, pos + delimiter.size());
            if (end > pos + delimiter.size()) {
                mergeSpan(pos + delimiter.size(), end - pos - delimiter.size(), style);
                info->hidden.append({pos, int(delimiter.size())});
                info->hidden.append({end, int(delimiter.size())});
                pos = end + delimiter.size(); continue;
            }
        }
        const auto link = QRegularExpression(R"(^\[([^\]]+)\]\(([^)]+)\))").match(rest);
        if (link.hasMatch()) {
            style.setForeground(m_accent); style.setFontUnderline(true); style.setAnchor(true);
            style.setAnchorHref(link.captured(2));
            mergeSpan(pos + 1, link.capturedLength(1), style);
            info->hidden.append({pos, 1});
            info->hidden.append({pos + 1 + int(link.capturedLength(1)), int(link.capturedLength() - link.capturedLength(1) - 1)});
            pos += link.capturedLength(); continue;
        }
        const auto url = QRegularExpression(R"(^(https?://[^\s<>()]+|www\.[^\s<>()]+))").match(rest);
        if (url.hasMatch()) {
            auto target = url.captured();
            while (!target.isEmpty() && QString(".,;:!?]}\"'").contains(target.back())) target.chop(1);
            style.setForeground(m_accent); style.setFontUnderline(true); style.setAnchor(true);
            style.setAnchorHref(target.startsWith("www.") ? "https://" + target : target);
            mergeSpan(pos, target.size(), style);
            pos += url.capturedLength(); continue;
        }
        ++pos;
    }
    // Apply hiding last so no nested style can make a delimiter visible again.
    const auto hidden = info->hidden;
    info->hidden.clear();
    for (const auto &span : hidden) hide(span.start, span.length);
    emit decorationsChanged();
}
