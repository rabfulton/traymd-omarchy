#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include "notedocument.h"

// A view has its own Qt Quick layout. This attachment belongs to one note
// for its lifetime and forwards content edits, never an initial empty buffer.
class NoteEditor : public QObject {
public:
    NoteEditor(QQuickTextDocument *view, NoteDocument *note);
private:
    void loadView();
    void applyEdit();
    QPointer<QQuickTextDocument> m_view;
    NoteDocument *const m_note;
    QString m_text;
    QMetaObject::Connection m_editConnection;
};
