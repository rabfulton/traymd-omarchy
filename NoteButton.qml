import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui

PanelActionButton {
    id: root
    fontSize: Style.font.heading
    // Fit the chosen font even when shell spacing is kept fixed.
    size: Math.ceil(Math.max(Style.space(30),
        symbolMetrics.height + Style.space(8),
        symbolMetrics.advanceWidth(root.iconText) + Style.space(8)))

    FontMetrics {
        id: symbolMetrics
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
    }
    foreground: Commons.Color.popups.text
    hoverColor: Commons.Color.accent
    focusable: true
    Accessible.role: Accessible.Button
    Accessible.name: tooltipText
    Accessible.onPressAction: clicked()
}
