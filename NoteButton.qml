import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui

PanelActionButton {
    size: Math.min(Style.space(30), 36)
    fontSize: Math.min(Style.font.heading, size - 8)
    foreground: Commons.Color.popups.text
    hoverColor: Commons.Color.accent
    focusable: true
    Accessible.role: Accessible.Button
    Accessible.name: tooltipText
    Accessible.onPressAction: clicked()
}
