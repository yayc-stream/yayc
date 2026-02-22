pragma Singleton
import QtQuick

QtObject {
    id: properties

    property bool isDarkMode: true  // Set by MainYayc Binding; seeded from main.qml Component.onCompleted

    // --- Color palette ---
    property color textColor:              isDarkMode ? "#ffffff" : "#212121"
    property color disabledTextColor:      isDarkMode ? "#a0a1a2" : "#9e9e9e"
    property color addedTextColor:         isDarkMode ? "#32cd32" : "#2e7d32"
    property color addedDisabledTextColor: isDarkMode ? "#196619" : "#81c784"
    property color selectionColor:         isDarkMode ? "#43adee" : "#1565c0"
    property color listHighlightColor:     isDarkMode ? "#585a5c" : "#e0e0e0"
    property color paneBackgroundColor:    isDarkMode ? "#2e2f30" : "#fafafa"
    property color paneColor:              isDarkMode ? "#373839" : "#f5f5f5"
    property color viewBorderColor:        isDarkMode ? "#000000" : "#bdbdbd"
    property color itemBackgroundColor:    isDarkMode ? "#46484a" : "#eeeeee"
    property color itemColor:              isDarkMode ? "#cccccc" : "#424242"
    property color iconHighlightColor:     isDarkMode ? "#26282a" : "#e8e8e8"
    property color fileBgColor:            isDarkMode ? "#000000" : "#ffffff"
    property color categoryBgColor:        isDarkMode ? "#000000" : "#f0f0f0"
    property color checkedButtonColor:     isDarkMode ? "#EF9A9A" : "#c62828"

    // Semantic colors for elements not covered by Material theme
    property color iconColor:              isDarkMode ? "#ffffff" : "#424242"
    property color hoverOverlayColor:      isDarkMode ? "#1affffff" : "#0d000000"
    property color tooltipBgColor:         isDarkMode ? "#a6191919" : "#e6f2f2f2"
    property color tooltipBorderColor:     isDarkMode ? "#26ffffff" : "#33000000"
    property color surfaceOverlayColor:    isDarkMode ? "#0dffffff" : "#0d000000"

    // --- Typography ---
    readonly property string labelFontFamily: "Open Sans"
    readonly property real fsH0: 40
    readonly property real fsH1: 34
    readonly property real fsH2: 28
    readonly property real fsH3: 24
    readonly property real fsH4: 20
    readonly property real fsH5: 16
    readonly property real fsH6: 12
    readonly property real fsP1: 16
    readonly property real fsP2: 12
}
