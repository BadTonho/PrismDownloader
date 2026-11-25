#include "PlaylistSelectionDialog.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

PlaylistSelectionDialog::PlaylistSelectionDialog(const QList<PlaylistItem> &items,
                                                 const QString &baseStyleSheet,
                                                 QWidget *parent)
    : QDialog(parent),
      m_items(items)
{
    setObjectName("playlistSelectionDialog");
    setWindowTitle("Itens da playlist - Prism Studio Suite");
    resize(900, 650);
    setMinimumSize(760, 520);
    setStyleSheet(baseStyleSheet + R"(
        QDialog#playlistSelectionDialog { background-color: #0b0f14; }
        QFrame#playlistHeader {
            background-color: #121b22;
            border: 1px solid #1e4237;
            border-radius: 12px;
        }
        QLabel#playlistKicker {
            color: #34d399;
            font-size: 11px;
            font-weight: bold;
            letter-spacing: 1.2px;
        }
        QLabel#playlistTitle { color: #f8fafc; font-size: 20px; font-weight: bold; }
        QLabel#playlistSubtitle { color: #94a3b8; font-size: 13px; }
        QLabel#playlistSelectionCount { color: #34d399; font-weight: bold; font-size: 13px; }
        QLineEdit#playlistSearch {
            background-color: #151d28;
            border: 1px solid #253447;
            border-radius: 8px;
            padding: 9px 13px;
            color: #f8fafc;
        }
        QLineEdit#playlistSearch:focus { border-color: #10b981; background-color: #1a2430; }
        QListWidget#playlistItems {
            background-color: #0e131b;
            border: 1px solid #1f2b3a;
            border-radius: 10px;
            outline: none;
        }
        QListWidget#playlistItems::item {
            min-height: 30px;
            padding: 5px 10px;
            border-bottom: 1px solid #18212e;
            color: #e2e8f0;
        }
        QListWidget#playlistItems::item:hover { background-color: #16242c; }
        QListWidget#playlistItems::item:selected { background-color: #16332a; }
        QWidget#playlistItemRow { background-color: #0e131b; border-bottom: 1px solid #18212e; }
        QCheckBox#playlistItemCheck { padding-left: 6px; }
        QPushButton#playlistItemTitle {
            background: transparent;
            border: none;
            color: #e2e8f0;
            font-size: 13px;
            padding: 7px 8px;
            text-align: left;
        }
        QPushButton#playlistItemTitle:hover { color: #34d399; text-decoration: underline; }
        QPushButton#playlistSecondaryButton {
            background-color: #161f2b;
            color: #cbd5e1;
            border: 1px solid #283648;
            border-radius: 8px;
            padding: 7px 14px;
            font-weight: bold;
        }
        QPushButton#playlistSecondaryButton:hover { border-color: #10b981; color: #f8fafc; background-color: #1c2836; }
        QPushButton#playlistConfirmButton {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #059669);
            color: #031a12;
            border: none;
            border-radius: 8px;
            padding: 10px 18px;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#playlistConfirmButton:hover { background-color: #34d399; }
        QPushButton#playlistConfirmButton:disabled { background-color: #19222e; color: #516175; }
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(24, 22, 24, 20);

    auto *header = new QFrame(this);
    header->setObjectName("playlistHeader");
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setSpacing(4);
    headerLayout->setContentsMargins(18, 14, 18, 14);

    auto *kicker = new QLabel("PLAYLIST ENCONTRADA", header);
    kicker->setObjectName("playlistKicker");
    auto *title = new QLabel("Escolha os vídeos que entrarão na fila", header);
    title->setObjectName("playlistTitle");
    auto *subtitle = new QLabel(
        QString("%1 vídeo(s) disponível(is). Clique em um nome para ver miniatura e duração.")
            .arg(m_items.size()),
        header);
    subtitle->setObjectName("playlistSubtitle");
    headerLayout->addWidget(kicker);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    layout->addWidget(header);

    auto *searchInput = new QLineEdit(this);
    searchInput->setObjectName("playlistSearch");
    searchInput->setPlaceholderText("Filtrar por título...");
    searchInput->setClearButtonEnabled(true);
    layout->addWidget(searchInput);

    auto *toolbar = new QHBoxLayout();
    m_selectionCount = new QLabel(this);
    m_selectionCount->setObjectName("playlistSelectionCount");
    auto *selectAllButton = new QPushButton("Selecionar todos", this);
    selectAllButton->setObjectName("playlistSecondaryButton");
    auto *clearButton = new QPushButton("Limpar seleção", this);
    clearButton->setObjectName("playlistSecondaryButton");
    toolbar->addWidget(m_selectionCount);
    toolbar->addStretch();
    toolbar->addWidget(selectAllButton);
    toolbar->addWidget(clearButton);
    layout->addLayout(toolbar);

    m_list = new QListWidget(this);
    m_list->setObjectName("playlistItems");
    m_list->setAlternatingRowColors(false);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setSpacing(1);
    for (int index = 0; index < m_items.size(); ++index) {
        const PlaylistItem item = m_items.at(index);
        auto *listItem = new QListWidgetItem(m_list);
        listItem->setToolTip(item.url.toString());
        listItem->setSizeHint(QSize(0, 44));

        auto *row = new QWidget(m_list);
        row->setObjectName("playlistItemRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 0, 8, 0);
        rowLayout->setSpacing(4);

        auto *check = new QCheckBox(row);
        check->setObjectName("playlistItemCheck");
        check->setChecked(true);
        auto *itemTitle = new QPushButton(QString("%1. %2").arg(index + 1).arg(item.title), row);
        itemTitle->setObjectName("playlistItemTitle");
        itemTitle->setCursor(Qt::PointingHandCursor);
        itemTitle->setToolTip("Ver informações deste vídeo");
        rowLayout->addWidget(check);
        rowLayout->addWidget(itemTitle, 1);
        m_list->setItemWidget(listItem, row);
        m_itemChecks.append(check);

        connect(itemTitle, &QPushButton::clicked, this, [this, item]() {
            emit itemDetailsRequested(item);
        });
    }
    layout->addWidget(m_list, 1);

    auto *footer = new QHBoxLayout();
    auto *cancelButton = new QPushButton("Cancelar", this);
    cancelButton->setObjectName("playlistSecondaryButton");
    m_confirmButton = new QPushButton("ADICIONAR SELECIONADOS", this);
    m_confirmButton->setObjectName("playlistConfirmButton");
    m_confirmButton->setMinimumHeight(42);
    footer->addStretch();
    footer->addWidget(cancelButton);
    footer->addWidget(m_confirmButton);
    layout->addLayout(footer);

    connect(selectAllButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_itemChecks) {
            check->setChecked(true);
        }
        updateSelectionState();
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        for (QCheckBox *check : m_itemChecks) {
            check->setChecked(false);
        }
        updateSelectionState();
    });
    for (QCheckBox *check : m_itemChecks) {
        connect(check, &QCheckBox::toggled, this, &PlaylistSelectionDialog::updateSelectionState);
    }
    connect(searchInput, &QLineEdit::textChanged, this, [this](const QString &filter) {
        for (int index = 0; index < m_list->count(); ++index) {
            m_list->item(index)->setHidden(!m_items.at(index).title.contains(filter, Qt::CaseInsensitive));
        }
    });
    connect(m_confirmButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    updateSelectionState();
}

QList<PlaylistItem> PlaylistSelectionDialog::selectedItems() const
{
    QList<PlaylistItem> selected;
    for (int index = 0; index < m_itemChecks.size(); ++index) {
        if (m_itemChecks.at(index)->isChecked()) {
            selected.append(m_items.at(index));
        }
    }
    return selected;
}

void PlaylistSelectionDialog::updateSelectionState()
{
    int selectedCount = 0;
    for (QCheckBox *check : m_itemChecks) {
        if (check->isChecked()) {
            ++selectedCount;
        }
    }
    m_selectionCount->setText(QString("%1 de %2 selecionado(s)")
                                  .arg(selectedCount)
                                  .arg(m_items.size()));
    m_confirmButton->setEnabled(selectedCount > 0);
}
