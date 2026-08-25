#include "PlaylistItemDetailsDialog.h"

#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

PlaylistItemDetailsDialog::PlaylistItemDetailsDialog(const PlaylistItem &item,
                                                     const QString &baseStyleSheet,
                                                     QNetworkAccessManager *network,
                                                     QWidget *parent)
    : QDialog(parent)
{
    setObjectName("playlistDetailsDialog");
    setWindowTitle("Informações do vídeo - Prism Studio Suite");
    resize(580, 520);
    setMinimumSize(580, 500);
    setStyleSheet(baseStyleSheet + R"(
        QDialog#playlistDetailsDialog { background-color: #151515; }
        QLabel#playlistThumbnail {
            background-color: #202020;
            border: 1px solid #31483d;
            border-radius: 9px;
            color: #a3a3a3;
        }
        QLabel#playlistDetailsTitle { color: #ffffff; font-size: 18px; font-weight: bold; }
        QLabel#playlistDetailsMeta { color: #10b981; font-size: 14px; font-weight: bold; }
        QLabel#playlistDetailsUrl { color: #a3a3a3; font-size: 11px; }
        QPushButton#playlistDetailsClose {
            background-color: #10b981;
            color: #021810;
            border: none;
            border-radius: 6px;
            padding: 10px 18px;
            font-weight: bold;
        }
        QPushButton#playlistDetailsClose:hover { background-color: #059669; }
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(22, 20, 22, 20);

    auto *thumbnail = new QLabel("Carregando miniatura...", this);
    thumbnail->setObjectName("playlistThumbnail");
    thumbnail->setAlignment(Qt::AlignCenter);
    thumbnail->setFixedSize(536, 300);
    layout->addWidget(thumbnail, 0, Qt::AlignHCenter);

    auto *title = new QLabel(item.title, this);
    title->setObjectName("playlistDetailsTitle");
    title->setWordWrap(true);
    layout->addWidget(title);

    const QString duration = item.duration.isEmpty() || item.duration == "NA"
        ? "Duração não informada pela playlist"
        : item.duration;
    auto *durationLabel = new QLabel("Duração: " + duration, this);
    durationLabel->setObjectName("playlistDetailsMeta");
    layout->addWidget(durationLabel);

    auto *urlLabel = new QLabel(item.url.toString(QUrl::FullyEncoded), this);
    urlLabel->setObjectName("playlistDetailsUrl");
    urlLabel->setWordWrap(true);
    layout->addWidget(urlLabel);

    auto *closeButton = new QPushButton("FECHAR", this);
    closeButton->setObjectName("playlistDetailsClose");
    closeButton->setMinimumHeight(40);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    if (!item.thumbnailUrl.isValid() || item.thumbnailUrl.host().isEmpty()
        || (item.thumbnailUrl.scheme() != "http" && item.thumbnailUrl.scheme() != "https")) {
        thumbnail->setText("Miniatura não disponível para este vídeo.");
    } else if (network) {
        QNetworkRequest request(item.thumbnailUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          "PrismDownloader/1.1 (playlist details)");
        QNetworkReply *reply = network->get(request);
        auto imageData = std::make_shared<QByteArray>();
        const QPointer<QLabel> thumbnailGuard = thumbnail;
        connect(reply, &QIODevice::readyRead, reply, [reply, imageData]() {
            constexpr qsizetype kMaximumThumbnailBytes = 5 * 1024 * 1024;
            const QByteArray chunk = reply->readAll();
            if (imageData->size() > kMaximumThumbnailBytes - chunk.size()) {
                reply->abort();
                return;
            }
            imageData->append(chunk);
        });
        connect(reply, &QNetworkReply::finished, reply,
                [reply, imageData, thumbnailGuard]() {
            QPixmap image;
            if (thumbnailGuard) {
                if (reply->error() == QNetworkReply::NoError && image.loadFromData(*imageData)) {
                    thumbnailGuard->setPixmap(image.scaled(thumbnailGuard->size(), Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation));
                } else {
                    thumbnailGuard->setText("Não foi possível carregar a miniatura.");
                }
            }
            reply->deleteLater();
        });
        connect(this, &QDialog::finished, reply, &QNetworkReply::abort);
        QTimer::singleShot(10000, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
    } else {
        thumbnail->setText("Miniatura não disponível para este vídeo.");
    }
}
