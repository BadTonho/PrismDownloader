#include "PrismStyleSheet.h"

namespace PrismStyleSheet {

QString mainWindow()
{
    return QStringLiteral(R"(
        QMainWindow {
            background-color: #121212;
            color: #dedede;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QWidget#downloadPage {
            background-color: #121212;
        }
        QWidget {
            color: #dedede;
        }
        QFrame#sidebar {
            background-color: #181818;
            border-right: 1px solid #262626;
        }
        QStackedWidget#mainArea {
            background-color: #121212;
        }
        QPushButton#navBtn {
            background-color: transparent;
            color: #909090;
            border: none;
            border-left: 3px solid transparent;
            padding: 13px 18px;
            text-align: left;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#navBtn:hover {
            background-color: #222222;
            color: #ffffff;
        }
        QPushButton#navBtn:checked {
            background-color: #1f2a24;
            color: #10b981;
            border-left: 3px solid #10b981;
        }
        QPushButton#updateSideBtn {
            background-color: #1b2922;
            color: #10b981;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #10b981;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px 6px 14px;
        }
        QPushButton#updateSideBtn:hover {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#updateSideBtn:checked {
            background-color: #10b981;
            color: #021810;
        }
        QPushButton#openFolderSideBtn {
            background-color: #1c2e3a;
            color: #38bdf8;
            font-weight: bold;
            font-size: 13px;
            border: 1px solid #38bdf8;
            border-radius: 5px;
            padding: 9px;
            margin: 0 14px;
        }
        QPushButton#openFolderSideBtn:hover {
            background-color: #38bdf8;
            color: #061824;
        }
        QGroupBox {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 8px;
            margin-top: 14px;
            font-weight: bold;
            color: #ffffff;
            font-size: 13px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #10b981;
        }
        QLineEdit, QComboBox {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 8px 12px;
            color: #ffffff;
            font-size: 13px;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #10b981;
            background-color: #262626;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #222222;
            color: white;
            selection-background-color: #10b981;
            selection-color: black;
        }
        QCheckBox {
            font-size: 13px;
            color: #a3a3a3;
            padding-top: 4px;
        }
        QCheckBox::indicator {
            width: 17px;
            height: 17px;
            border: 1px solid #3e3e3e;
            border-radius: 4px;
            background-color: #222222;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #10b981;
        }
        QCheckBox::indicator:checked {
            background-color: #10b981;
            border: 1px solid #ffffff;
        }
        QLabel {
            font-size: 13px;
        }
        QPushButton#startBtn {
            background-color: #10b981;
            color: #021810;
            font-weight: bold;
            font-size: 14px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#startBtn:hover {
            background-color: #059669;
            color: #ffffff;
        }
        QPushButton#startBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#cancelBtn {
            background-color: #dc2626;
            color: #ffffff;
            font-weight: bold;
            font-size: 13px;
            border-radius: 6px;
            border: none;
            padding: 10px;
        }
        QPushButton#cancelBtn:hover {
            background-color: #b91c1c;
        }
        QPushButton#cancelBtn:disabled {
            background-color: #242424;
            color: #666666;
        }
        QPushButton#browseBtn {
            background-color: #263530;
            color: #10b981;
            font-weight: bold;
            border: 1px solid #10b981;
            border-radius: 6px;
            padding: 6px 14px;
        }
        QPushButton#browseBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QProgressBar {
            background-color: #202020;
            border: 1px solid #333333;
            border-radius: 6px;
            text-align: center;
            font-weight: bold;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #047857);
            border-radius: 5px;
        }
        QTableWidget#libraryTable {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 6px;
            color: #ffffff;
            gridline-color: #282828;
            font-size: 13px;
            selection-background-color: #10b981;
            selection-color: #021810;
        }
        QTableWidget#libraryTable QHeaderView::section {
            background-color: #242424;
            color: #10b981;
            font-weight: bold;
            border: none;
            border-bottom: 2px solid #10b981;
            padding: 8px 10px;
            font-size: 13px;
        }
        QTableWidget#libraryTable::item {
            padding: 8px 10px;
            border-bottom: 1px solid #222222;
        }
        QTableWidget#libraryTable::item:selected {
            background-color: #10b981;
            color: #021810;
            font-weight: bold;
        }
        QPushButton#libraryViewBtn {
            background-color: #263530;
            color: #a7f3d0;
            border: 1px solid #315344;
            border-radius: 6px;
            padding: 6px 12px;
            font-weight: bold;
        }
        QPushButton#libraryViewBtn:hover {
            background-color: #354a43;
            color: #ffffff;
        }
        QPushButton#libraryViewBtn:checked {
            background-color: #10b981;
            color: #021810;
            border-color: #10b981;
        }
        QListWidget#libraryBlocks {
            background-color: #1a1a1a;
            border: 1px solid #282828;
            border-radius: 6px;
            outline: none;
            padding: 8px;
        }
        QListWidget#libraryBlocks::item {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 8px;
            padding: 2px;
        }
        QListWidget#libraryBlocks::item:selected {
            background-color: #263e34;
            border: 1px solid #10b981;
        }
        QWidget#libraryCard {
            background-color: #202522;
            border: 1px solid #303a34;
            border-radius: 7px;
        }
        QLabel#libraryCardThumb {
            background-color: #111513;
            border: 1px solid #35443b;
            border-radius: 5px;
            color: #94a3b8;
            font-size: 11px;
        }
        QLabel#libraryCardTitle {
            color: #f8fafc;
            font-size: 12px;
            font-weight: bold;
        }
        QLabel#libraryCardMeta {
            color: #10b981;
            font-size: 11px;
            font-weight: bold;
        }
        QLabel#logSummaryLabel {
            color: #94a3b8;
            font-size: 11px;
            font-family: 'Consolas', 'Courier New', monospace;
        }
        QLineEdit#logSearchInput {
            background-color: #171f1a;
            color: #d1fae5;
            border: 1px solid #315344;
            border-radius: 5px;
            padding: 5px 8px;
        }
        QPlainTextEdit#logArea {
            background-color: #0a0e0b;
            border: 1px solid #1a241c;
            border-radius: 6px;
            color: #10b981;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 12px;
        }
    )");
}

}
