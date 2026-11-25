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
        QDialog#playlistDetailsDialog { background-color: #0b0f14; }
        QLabel#playlistThumbnail {
            background-color: #121821;
            border: 1px solid #223042;
            border-radius: 10px;
            color: #94a3b8;
        }
        QLabel#playlistDetailsTitle { color: #f8fafc; font-size: 18px; font-weight: bold; }
        QLabel#playlistDetailsMeta { color: #34d399; font-size: 14px; font-weight: bold; }
        QLabel#playlistDetailsUrl { color: #94a3b8; font-size: 11px; }
        QPushButton#playlistDetailsClose {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #059669);
            color: #031a12;
            border: none;
            border-radius: 8px;
            padding: 10px 20px;
            font-weight: bold;
        }
        QPushButton#playlistDetailsClose:hover { background-color: #34d399; }
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
