#ifndef PLAYLIST_ITEM_DETAILS_DIALOG_H
#define PLAYLIST_ITEM_DETAILS_DIALOG_H

#include <QDialog>

#include "PlaylistItem.h"

class QNetworkAccessManager;

class PlaylistItemDetailsDialog final : public QDialog {
public:
    PlaylistItemDetailsDialog(const PlaylistItem &item,
                              const QString &baseStyleSheet,
                              QNetworkAccessManager *network,
                              QWidget *parent = nullptr);
};

#endif // PLAYLIST_ITEM_DETAILS_DIALOG_H
