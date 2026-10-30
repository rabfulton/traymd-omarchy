#include "noteeditor.h"
#include <QTextCursor>

NoteEditor::NoteEditor(QQuickTextDocument *view, NoteDocument *note)
    : QObject(view), m_view(view), m_note(note) {
    // Load before subscribing to edits. Construction has no write path.
    loadView();
    connect(note, &NoteDocument::textChanged, this, &NoteEditor::loadView);
    connect(note, &QObject::destroyed, this, [this] { disconnect(m_editConnection); });
    // QObject::destroyed precedes deletion of the editor's child document.
    connect(view->parent(), &QObject::destroyed, this, [this] { disconnect(m_editConnection); });
}

void NoteEditor::loadView() {
    if (!m_view) return;
    const auto text = m_note->document()->toPlainText();
    if (text == m_text && m_view->textDocument()->toPlainText() == text && m_editConnection) return;
    disconnect(m_editConnection);
    m_text = text;
    // Use TextArea's normal setter so Qt Quick invalidates its own paint and
    // layout caches. The note's document is never installed in a TextArea.
    m_view->parent()->setProperty("text", text);
    m_editConnection = connect(m_view->textDocument(), &QTextDocument::contentsChanged, this, &NoteEditor::applyEdit);
}

void NoteEditor::applyEdit() {
    const auto text = m_view->textDocument()->toPlainText();
    if (text == m_text) return; // Layout and highlighting do not edit content.
    int start = 0;
    while (start < text.size() && start < m_text.size() && text[start] == m_text[start]) ++start;
    int end = 0;
    while (end < text.size() - start && end < m_text.size() - start && text[text.size() - 1 - end] == m_text[m_text.size() - 1 - end]) ++end;
    const int removed = m_text.size() - start - end;
    const auto inserted = text.mid(start, text.size() - start - end);
    m_text = text;
    QTextCursor edit(m_note->document());
    edit.setPosition(start);
    edit.setPosition(start + removed, QTextCursor::KeepAnchor);
    edit.insertText(inserted);
}
