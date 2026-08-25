#ifndef PLAYLIST_ITEM_H
#define PLAYLIST_ITEM_H

#include <QUrl>
#include <QString>

struct PlaylistItem {
    QString title;
    QUrl url;
    QString duration;
    QUrl thumbnailUrl;
};

#endif // PLAYLIST_ITEM_H
