#ifndef PLAYLIST_SELECTION_DIALOG_H
#define PLAYLIST_SELECTION_DIALOG_H

#include <QDialog>
#include <QList>

#include "PlaylistItem.h"

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;

class PlaylistSelectionDialog final : public QDialog {
    Q_OBJECT

public:
    PlaylistSelectionDialog(const QList<PlaylistItem> &items,
                            const QString &baseStyleSheet,
                            QWidget *parent = nullptr);

    QList<PlaylistItem> selectedItems() const;

signals:
    void itemDetailsRequested(const PlaylistItem &item);

private:
    void updateSelectionState();

    const QList<PlaylistItem> m_items;
    QList<QCheckBox *> m_itemChecks;
    QListWidget *m_list{nullptr};
    QLabel *m_selectionCount{nullptr};
    QPushButton *m_confirmButton{nullptr};
};

#endif // PLAYLIST_SELECTION_DIALOG_H
