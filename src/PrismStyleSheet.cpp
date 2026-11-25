#include "PrismStyleSheet.h"

namespace PrismStyleSheet {

QString mainWindow()
{
    return QStringLiteral(R"(
        QMainWindow {
            background-color: #0b0f14;
            color: #e2e8f0;
            font-family: 'Segoe UI', 'Inter', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget#downloadPage {
            background-color: #0b0f14;
        }
        QWidget {
            color: #e2e8f0;
            font-family: 'Segoe UI', 'Inter', Arial, sans-serif;
        }

        /* ==========================================
           SCROLLBARS & OVERLAYS (MENUS / TOOLTIPS)
           ========================================== */
        QScrollBar:vertical {
            background-color: transparent;
            width: 10px;
            margin: 4px 2px 4px 2px;
        }
        QScrollBar::handle:vertical {
            background-color: #263242;
            border-radius: 3px;
            min-height: 28px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #384960;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
            border: none;
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: transparent;
            height: 10px;
            margin: 2px 4px 2px 4px;
        }
        QScrollBar::handle:horizontal {
            background-color: #263242;
            border-radius: 3px;
            min-width: 28px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #384960;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
            border: none;
            width: 0px;
        }
        QMenu {
            background-color: #151c26;
            border: 1px solid #283546;
            border-radius: 8px;
            padding: 6px;
            color: #e2e8f0;
        }
        QMenu::item {
            padding: 8px 18px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: #1a2d28;
            color: #34d399;
        }
        QMenu::separator {
            height: 1px;
            background-color: #232f3e;
            margin: 5px 8px;
        }
        QToolTip {
            background-color: #161f2b;
            color: #e2e8f0;
            border: 1px solid #2b394c;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }

        /* ==========================================
           SIDEBAR NAVIGATION
           ========================================== */
        QFrame#sidebar {
            background-color: #10151d;
            border-right: 1px solid #1e2937;
        }
        QFrame#sidebarDivider {
            background-color: #1e2937;
            max-height: 1px;
            min-height: 1px;
            margin: 4px 16px;
        }
        QLabel#brandTitle {
            color: #f8fafc;
            font-size: 16px;
            font-weight: bold;
            letter-spacing: 0.3px;
        }
        QLabel#brandSubtitle {
            color: #10b981;
            font-size: 10px;
            font-weight: bold;
            letter-spacing: 1.6px;
        }
        QStackedWidget#mainArea {
            background-color: #0b0f14;
        }
        QPushButton#navBtn {
            background-color: transparent;
            color: #8b9bb0;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 11px 16px;
            margin: 0 12px;
            text-align: left;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#navBtn:hover {
            background-color: #17202c;
            color: #f1f5f9;
        }
        QPushButton#navBtn:checked {
            background-color: #132822;
            color: #34d399;
            border: 1px solid #1e4d3e;
            font-weight: bold;
        }
        QPushButton#updateSideBtn {
            background-color: #122621;
            color: #34d399;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #1e5242;
            border-radius: 8px;
            padding: 9px 12px;
            margin: 0 14px 4px 14px;
        }
        QPushButton#updateSideBtn:hover {
            background-color: #10b981;
            border-color: #10b981;
            color: #041b13;
        }
        QPushButton#updateSideBtn:checked {
            background-color: #10b981;
            border-color: #10b981;
            color: #041b13;
        }
        QPushButton#openFolderSideBtn {
            background-color: #132230;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #1f4562;
            border-radius: 8px;
            padding: 9px 12px;
            margin: 0 14px;
        }
        QPushButton#openFolderSideBtn:hover {
            background-color: #38bdf8;
            border-color: #38bdf8;
            color: #061824;
        }

        /* ==========================================
           ELEVATED CARDS & GROUP BOXES
           ========================================== */
        QFrame#heroCard {
            background-color: #121821;
            border: 1px solid #1f2b3a;
            border-radius: 12px;
        }
        QGroupBox {
            background-color: #121821;
            border: 1px solid #1f2b3a;
            border-radius: 12px;
            margin-top: 14px;
            font-weight: bold;
            color: #f8fafc;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 2px 10px;
            background-color: #0b0f14;
            color: #34d399;
            border-radius: 4px;
        }

        /* ==========================================
           INPUTS, COMBOS & CHECKBOXES
           ========================================== */
        QLineEdit, QComboBox {
            background-color: #17202b;
            border: 1px solid #273547;
            border-radius: 8px;
            padding: 8px 13px;
            color: #f8fafc;
            font-size: 13px;
            selection-background-color: #10b981;
            selection-color: #041b13;
        }
        QLineEdit:hover, QComboBox:hover {
            border-color: #35475e;
            background-color: #1b2532;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #10b981;
            background-color: #1a2430;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            width: 24px;
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #151d28;
            border: 1px solid #2a394c;
            border-radius: 8px;
            color: #f1f5f9;
            padding: 4px;
            selection-background-color: #10b981;
            selection-color: #041b13;
            outline: none;
        }
        QComboBox#concurrencyCombo {
            background-color: #17202b;
            border: 1px solid #2b3a4d;
            border-radius: 7px;
            padding: 4px 10px;
            color: #f8fafc;
            font-size: 13px;
            font-weight: bold;
        }
        QComboBox#concurrencyCombo:hover {
            border: 1px solid #10b981;
            background-color: #1d2836;
        }
        QComboBox#concurrencyCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: center right;
            width: 18px;
            border: none;
        }
        QComboBox#concurrencyCombo QAbstractItemView {
            background-color: #151d28;
            border: 1px solid #2b3a4d;
            border-radius: 6px;
            color: #f8fafc;
            font-size: 13px;
            font-weight: bold;
            selection-background-color: #10b981;
            selection-color: #041b13;
            outline: none;
            padding: 3px;
            min-width: 56px;
        }
        QComboBox#concurrencyCombo QAbstractItemView::item {
            min-height: 24px;
            padding: 3px 8px;
            border-radius: 4px;
        }
        QSpinBox {
            background-color: #17202b;
            border: 1px solid #2b3a4d;
            border-radius: 8px;
            padding: 4px 8px;
            color: #f8fafc;
            font-size: 13px;
            font-weight: bold;
        }
        QSpinBox:hover, QSpinBox:focus {
            border: 1px solid #10b981;
            background-color: #1d2836;
        }
        QCheckBox {
            font-size: 13px;
            color: #94a3b8;
            spacing: 8px;
        }
        QCheckBox:hover {
            color: #cbd5e1;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border: 1px solid #334357;
            border-radius: 5px;
            background-color: #17202b;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #10b981;
        }
        QCheckBox::indicator:checked {
            background-color: #10b981;
            border: 1px solid #34d399;
        }
        QLabel {
            font-size: 13px;
        }

        /* ==========================================
           PRIMARY, SECONDARY & DANGER BUTTONS
           ========================================== */
        QPushButton#startBtn {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #059669);
            color: #031a12;
            font-weight: bold;
            font-size: 13px;
            letter-spacing: 0.4px;
            border-radius: 8px;
            border: none;
            padding: 10px 18px;
        }
        QPushButton#startBtn:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #34d399, stop:1 #10b981);
            color: #02140e;
        }
        QPushButton#startBtn:pressed {
            background-color: #047857;
            color: #ecfdf5;
        }
        QPushButton#startBtn:disabled {
            background-color: #19222e;
            color: #516175;
            border: 1px solid #222e3d;
        }
        QPushButton#cancelBtn {
            background-color: #2a161b;
            color: #f87171;
            font-weight: bold;
            font-size: 12px;
            letter-spacing: 0.4px;
            border-radius: 8px;
            border: 1px solid #5c242f;
            padding: 9px 14px;
        }
        QPushButton#cancelBtn:hover {
            background-color: #dc2626;
            border-color: #ef4444;
            color: #ffffff;
        }
        QPushButton#cancelBtn:disabled {
            background-color: #161d27;
            border: 1px solid #202a38;
            color: #4f5e71;
        }
        QPushButton#browseBtn {
            background-color: #142622;
            color: #34d399;
            font-weight: bold;
            border: 1px solid #1f5544;
            border-radius: 8px;
            padding: 7px 15px;
        }
        QPushButton#browseBtn:hover {
            background-color: #1a3830;
            border-color: #10b981;
            color: #ecfdf5;
        }

        /* ==========================================
           PROGRESS BARS
           ========================================== */
        QProgressBar {
            background-color: #17202b;
            border: 1px solid #253344;
            border-radius: 10px;
            text-align: center;
            font-weight: bold;
            font-size: 12px;
            color: #f8fafc;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #059669, stop:0.6 #10b981, stop:1 #34d399);
            border-radius: 9px;
        }

        /* ==========================================
           TABLES (QUEUE & LIBRARY LIST)
           ========================================== */
        QTableWidget#libraryTable {
            background-color: #0e131b;
            alternate-background-color: #121923;
            border: 1px solid #1f2b3a;
            border-radius: 10px;
            color: #e2e8f0;
            gridline-color: #1a2431;
            font-size: 13px;
            selection-background-color: #16332a;
            selection-color: #ecfdf5;
            outline: none;
        }
        QTableWidget#libraryTable QHeaderView::section {
            background-color: #161f2b;
            color: #34d399;
            font-weight: bold;
            border: none;
            border-bottom: 1px solid #25364a;
            padding: 10px 12px;
            font-size: 12px;
        }
        QTableWidget#libraryTable::item {
            padding: 9px 12px;
            border-bottom: 1px solid #17202c;
        }
        QTableWidget#libraryTable::item:hover {
            background-color: #16202d;
        }
        QTableWidget#libraryTable::item:selected {
            background-color: #16332a;
            color: #ecfdf5;
            font-weight: bold;
        }

        /* ==========================================
           LIBRARY VIEW & CARDS
           ========================================== */
        QPushButton#libraryViewBtn {
            background-color: #161f2b;
            color: #94a3b8;
            border: 1px solid #253344;
            border-radius: 8px;
            padding: 6px 14px;
            font-weight: bold;
        }
        QPushButton#libraryViewBtn:hover {
            background-color: #1d2938;
            color: #f8fafc;
            border-color: #33475e;
        }
        QPushButton#libraryViewBtn:checked {
            background-color: #132822;
            color: #34d399;
            border-color: #10b981;
        }
        QListWidget#libraryBlocks {
            background-color: #0e131b;
            border: 1px solid #1f2b3a;
            border-radius: 10px;
            outline: none;
            padding: 10px;
        }
        QListWidget#libraryBlocks::item {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 10px;
            padding: 2px;
        }
        QListWidget#libraryBlocks::item:selected {
            background-color: #142c25;
            border: 1px solid #10b981;
        }
        QWidget#libraryCard {
            background-color: #151d28;
            border: 1px solid #222f40;
            border-radius: 9px;
        }
        QLabel#libraryCardThumb {
            background-color: #0b0f14;
            border: 1px solid #243244;
            border-radius: 6px;
            color: #64748b;
            font-size: 11px;
        }
        QLabel#libraryCardTitle {
            color: #f8fafc;
            font-size: 12px;
            font-weight: bold;
        }
        QLabel#libraryCardMeta {
            color: #34d399;
            font-size: 11px;
            font-weight: bold;
        }

        /* ==========================================
           LOGS TERMINAL
           ========================================== */
        QLabel#logSummaryLabel {
            color: #94a3b8;
            font-size: 11px;
            font-family: 'Consolas', 'Courier New', monospace;
        }
        QLineEdit#logSearchInput {
            background-color: #141c26;
            color: #e2e8f0;
            border: 1px solid #253447;
            border-radius: 7px;
            padding: 6px 10px;
        }
        QLineEdit#logSearchInput:focus {
            border-color: #10b981;
        }
        QPlainTextEdit#logArea {
            background-color: #080c11;
            border: 1px solid #1c2735;
            border-radius: 10px;
            color: #34d399;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 14px;
        }
    )");
}

}
