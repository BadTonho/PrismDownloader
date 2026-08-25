#include "LibraryView.h"

#include "MediaToolResolver.h"

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QButtonGroup>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace {
constexpr qsizetype kMaximumThumbnailBytes = 3 * 1024 * 1024;
constexpr int kMaximumConcurrentThumbnails = 3;
}

LibraryView::LibraryView(QWidget *parent)
    : QWidget(parent)
{
    auto *libraryLayout = new QVBoxLayout(this);
    libraryLayout->setSpacing(14);
    libraryLayout->setContentsMargins(24, 20, 24, 20);

    auto *topLayout = new QHBoxLayout();
    auto *title = new QLabel(
        QStringLiteral("Biblioteca de Mídias (Arquivos na Pasta de Destino):"), this);
    title->setStyleSheet(QStringLiteral(
        "font-weight: bold; color: #10b981; font-size: 15px;"));
    topLayout->addWidget(title);
    topLayout->addStretch();

    auto *refreshButton = new QPushButton(QStringLiteral("Atualizar Lista"), this);
    refreshButton->setObjectName(QStringLiteral("browseBtn"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    refreshButton->setMinimumHeight(32);
    connect(refreshButton, &QPushButton::clicked, this, &LibraryView::refreshCurrentFolder);
    topLayout->addWidget(refreshButton);

    m_listViewButton = new QPushButton(QStringLiteral("☷ Lista"), this);
    m_listViewButton->setObjectName(QStringLiteral("libraryViewBtn"));
    m_listViewButton->setCheckable(true);
    m_listViewButton->setChecked(true);
    m_listViewButton->setCursor(Qt::PointingHandCursor);
    m_listViewButton->setMinimumHeight(32);
    connect(m_listViewButton, &QPushButton::clicked, this, &LibraryView::setListView);

    m_blocksViewButton = new QPushButton(QStringLiteral("▦ Blocos"), this);
    m_blocksViewButton->setObjectName(QStringLiteral("libraryViewBtn"));
    m_blocksViewButton->setCheckable(true);
    m_blocksViewButton->setCursor(Qt::PointingHandCursor);
    m_blocksViewButton->setMinimumHeight(32);
    connect(m_blocksViewButton, &QPushButton::clicked, this, &LibraryView::setBlocksView);

    auto *viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    viewGroup->addButton(m_listViewButton);
    viewGroup->addButton(m_blocksViewButton);
    topLayout->addWidget(m_listViewButton);
    topLayout->addWidget(m_blocksViewButton);
    libraryLayout->addLayout(topLayout);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Arquivo de Mídia"), QStringLiteral("Formato"), QStringLiteral("Tamanho")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setObjectName(QStringLiteral("libraryTable"));
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &LibraryView::handleTableDoubleClick);

    m_blocks = new QListWidget(this);
    m_blocks->setObjectName(QStringLiteral("libraryBlocks"));
    m_blocks->setViewMode(QListView::IconMode);
    m_blocks->setFlow(QListView::LeftToRight);
    m_blocks->setResizeMode(QListView::Adjust);
    m_blocks->setMovement(QListView::Static);
    m_blocks->setWrapping(true);
    m_blocks->setSpacing(12);
    m_blocks->setGridSize(QSize(208, 204));
    m_blocks->setSelectionMode(QAbstractItemView::SingleSelection);
    m_blocks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_blocks->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_blocks->installEventFilter(this);
    m_blocks->viewport()->installEventFilter(this);
    connect(m_blocks, &QListWidget::itemDoubleClicked,
            this, &LibraryView::handleBlockDoubleClick);
    connect(m_blocks->verticalScrollBar(), &QAbstractSlider::valueChanged,
            this, [this](int) { scheduleThumbnailLoading(); });

    m_thumbnailTimer = new QTimer(this);
    m_thumbnailTimer->setSingleShot(true);
    m_thumbnailTimer->setInterval(50);
    connect(m_thumbnailTimer, &QTimer::timeout,
            this, &LibraryView::loadVisibleThumbnails);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_table);
    m_viewStack->addWidget(m_blocks);
    m_viewStack->setCurrentIndex(0);
    libraryLayout->addWidget(m_viewStack);

    auto *bottomLayout = new QHBoxLayout();
    auto *playButton = new QPushButton(
        QStringLiteral("REPRODUZIR / ABRIR SELECIONADO"), this);
    playButton->setObjectName(QStringLiteral("startBtn"));
    playButton->setCursor(Qt::PointingHandCursor);
    playButton->setMinimumHeight(40);
    connect(playButton, &QPushButton::clicked, this, &LibraryView::playSelected);

    auto *openFolderButton = new QPushButton(QStringLiteral("ABRIR NO EXPLORER"), this);
    openFolderButton->setObjectName(QStringLiteral("openFolderSideBtn"));
    openFolderButton->setCursor(Qt::PointingHandCursor);
    openFolderButton->setMinimumHeight(40);
    connect(openFolderButton, &QPushButton::clicked,
            this, &LibraryView::openFolderRequested);

    bottomLayout->addWidget(playButton, 2);
    bottomLayout->addWidget(openFolderButton, 1);
    libraryLayout->addLayout(bottomLayout);
}

LibraryView::~LibraryView()
{
    stopThumbnailProcesses();
}

void LibraryView::refresh(const QString &folder)
{
    m_currentFolder = folder;
    stopThumbnailProcesses();
    m_table->setRowCount(0);
    m_blocks->clear();

    const QDir directory(folder);
    if (!directory.exists()) {
        return;
    }

    const QStringList filters{
        QStringLiteral("*.mp4"), QStringLiteral("*.mp3"), QStringLiteral("*.mkv"),
        QStringLiteral("*.webm"), QStringLiteral("*.m4a"), QStringLiteral("*.avi"),
        QStringLiteral("*.flv"), QStringLiteral("*.wav")};
    const QFileInfoList fileList = directory.entryInfoList(
        filters, QDir::Files | QDir::NoSymLinks, QDir::Time);

    if (m_viewStack->currentIndex() == 1) {
        refreshBlocks(fileList);
    } else {
        m_table->setRowCount(fileList.size());
        for (int index = 0; index < fileList.size(); ++index) {
            const QFileInfo &info = fileList.at(index);
            auto *title = new QTableWidgetItem(info.fileName());
            title->setFlags(title->flags() ^ Qt::ItemIsEditable);
            title->setData(Qt::UserRole, info.absoluteFilePath());

            auto *extension = new QTableWidgetItem(info.suffix().toUpper());
            extension->setFlags(extension->flags() ^ Qt::ItemIsEditable);
            extension->setTextAlignment(Qt::AlignCenter);

            const double sizeMb = static_cast<double>(info.size()) / (1024.0 * 1024.0);
            auto *size = new QTableWidgetItem(
                QStringLiteral("%1 MB").arg(sizeMb, 0, 'f', 1));
            size->setFlags(size->flags() ^ Qt::ItemIsEditable);
            size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            m_table->setItem(index, 0, title);
            m_table->setItem(index, 1, extension);
            m_table->setItem(index, 2, size);
        }
    }
    emit logMessageRequested(
        QStringLiteral("[Biblioteca] Lista de mídias atualizada: %1 arquivo(s) encontrado(s).")
            .arg(fileList.size()));
}

void LibraryView::refreshCurrentFolder()
{
    refresh(m_currentFolder);
}

void LibraryView::setListView()
{
    setViewMode(false);
}

void LibraryView::setBlocksView()
{
    setViewMode(true);
}

void LibraryView::setViewMode(bool blocks)
{
    m_viewStack->setCurrentIndex(blocks ? 1 : 0);
    m_listViewButton->setChecked(!blocks);
    m_blocksViewButton->setChecked(blocks);
    refreshCurrentFolder();
}

void LibraryView::refreshBlocks(const QFileInfoList &fileList)
{
    for (const QFileInfo &info : fileList) {
        auto *item = new QListWidgetItem(m_blocks);
        item->setData(Qt::UserRole, info.absoluteFilePath());
        item->setSizeHint(QSize(208, 194));

        auto *card = new QWidget(m_blocks);
        card->setObjectName(QStringLiteral("libraryCard"));
        card->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(5, 5, 5, 5);
        cardLayout->setSpacing(5);

        auto *thumbnail = new QLabel(QStringLiteral("Gerando miniatura..."), card);
        thumbnail->setObjectName(QStringLiteral("libraryCardThumb"));
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setFixedSize(198, 112);
        thumbnail->setWordWrap(true);
        cardLayout->addWidget(thumbnail, 0, Qt::AlignHCenter);

        auto *title = new QLabel(info.fileName(), card);
        title->setObjectName(QStringLiteral("libraryCardTitle"));
        title->setWordWrap(true);
        title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        title->setMaximumHeight(38);
        title->setToolTip(info.fileName());
        cardLayout->addWidget(title);

        const double sizeMb = static_cast<double>(info.size()) / (1024.0 * 1024.0);
        auto *metadata = new QLabel(
            QStringLiteral("%1  •  %2 MB")
                .arg(info.suffix().toUpper())
                .arg(sizeMb, 0, 'f', 1), card);
        metadata->setObjectName(QStringLiteral("libraryCardMeta"));
        cardLayout->addWidget(metadata);
        m_blocks->setItemWidget(item, card);
    }
    updateBlockLayout();
    scheduleThumbnailLoading();
}

void LibraryView::updateBlockLayout()
{
    constexpr int spacing = 12;
    constexpr int minimumCardWidth = 208;
    constexpr int itemHeight = 204;
    const int viewportWidth = qMax(m_blocks->viewport()->width(), minimumCardWidth);
    const int usableWidth = qMax(minimumCardWidth, viewportWidth - 16);
    const int columns = qMax(1, (usableWidth + spacing) / (minimumCardWidth + spacing));
    const int cardWidth = qMax(minimumCardWidth,
                               (usableWidth - (columns - 1) * spacing) / columns);

    m_blocks->setGridSize(QSize(cardWidth, itemHeight));
    for (int index = 0; index < m_blocks->count(); ++index) {
        QListWidgetItem *item = m_blocks->item(index);
        if (!item) {
            continue;
        }
        item->setSizeHint(QSize(cardWidth, itemHeight));
        QWidget *card = m_blocks->itemWidget(item);
        if (!card) {
            continue;
        }
        card->setFixedWidth(qMax(194, cardWidth - 4));
        if (auto *thumbnail = card->findChild<QLabel *>(QStringLiteral("libraryCardThumb"))) {
            thumbnail->setFixedSize(qMax(184, cardWidth - 14), 112);
        }
    }
}

void LibraryView::scheduleThumbnailLoading()
{
    if (!m_thumbnailTimer || m_viewStack->currentIndex() != 1) {
        return;
    }
    if (!m_thumbnailTimer->isActive()) {
        m_thumbnailTimer->start();
    }
}

void LibraryView::loadVisibleThumbnails()
{
    if (m_viewStack->currentIndex() != 1) {
        return;
    }
    const QRect visibleRect = m_blocks->viewport()->rect().adjusted(0, -220, 0, 220);
    for (int index = 0; index < m_blocks->count(); ++index) {
        if (m_thumbnailProcesses.size() >= kMaximumConcurrentThumbnails) {
            break;
        }
        QListWidgetItem *item = m_blocks->item(index);
        if (!item || !m_blocks->visualItemRect(item).intersects(visibleRect)) {
            continue;
        }
        QWidget *card = m_blocks->itemWidget(item);
        auto *thumbnail = card
            ? card->findChild<QLabel *>(QStringLiteral("libraryCardThumb")) : nullptr;
        if (!thumbnail || thumbnail->property("thumbnailRequested").toBool()) {
            continue;
        }
        thumbnail->setProperty("thumbnailRequested", true);
        loadThumbnail(QFileInfo(item->data(Qt::UserRole).toString()), thumbnail);
    }
}

void LibraryView::loadThumbnail(const QFileInfo &fileInfo, QLabel *thumbnailLabel)
{
    if (!thumbnailLabel) {
        return;
    }
    const QString path = fileInfo.absoluteFilePath();
    const QString extension = fileInfo.suffix().toLower();
    const QStringList videoExtensions{
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi"), QStringLiteral("flv"), QStringLiteral("mov"),
        QStringLiteral("m4v"), QStringLiteral("wmv")};
    if (!videoExtensions.contains(extension)) {
        thumbnailLabel->setText(QStringLiteral("ÁUDIO\n%1").arg(extension.toUpper()));
        return;
    }

    const auto cached = m_thumbnailCache.constFind(path);
    if (cached != m_thumbnailCache.cend()) {
        thumbnailLabel->setPixmap(cached.value().scaled(
            thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    const QString ffmpeg = MediaToolResolver::resolve(MediaTool::Ffmpeg);
    if (ffmpeg.isEmpty() || !QFileInfo(ffmpeg).isFile()) {
        thumbnailLabel->setText(QStringLiteral("Miniatura indisponível\n(FFmpeg não encontrado)"));
        return;
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    m_thumbnailProcesses.insert(process);
    auto output = std::make_shared<QByteArray>();
    const QPointer<QLabel> labelGuard = thumbnailLabel;
    connect(process, &QProcess::readyReadStandardOutput, process, [process, output]() {
        const QByteArray chunk = process->readAllStandardOutput();
        if (output->size() > kMaximumThumbnailBytes - chunk.size()) {
            process->kill();
            return;
        }
        output->append(chunk);
    });
    connect(process, &QProcess::readyReadStandardError, process, [process]() {
        process->readAllStandardError();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, output, labelGuard, path](int exitCode, QProcess::ExitStatus status) {
        m_thumbnailProcesses.remove(process);
        scheduleThumbnailLoading();
        if (labelGuard && status == QProcess::NormalExit && exitCode == 0) {
            QPixmap image;
            if (image.loadFromData(*output)) {
                if (m_thumbnailCache.size() >= 64) {
                    m_thumbnailCache.erase(m_thumbnailCache.begin());
                }
                m_thumbnailCache.insert(path, image);
                labelGuard->setPixmap(image.scaled(
                    labelGuard->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                labelGuard->setText(QStringLiteral("Miniatura indisponível"));
            }
        } else if (labelGuard) {
            labelGuard->setText(QStringLiteral("Miniatura indisponível"));
        }
        process->deleteLater();
    });
    process->start(ffmpeg, {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), QStringLiteral("1"), QStringLiteral("-i"), path,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-vf"), QStringLiteral("scale=320:180:force_original_aspect_ratio=decrease"),
        QStringLiteral("-f"), QStringLiteral("image2pipe"),
        QStringLiteral("-vcodec"), QStringLiteral("mjpeg"), QStringLiteral("-q:v"), QStringLiteral("5"),
        QStringLiteral("pipe:1")});
}

void LibraryView::stopThumbnailProcesses()
{
    if (m_thumbnailTimer) {
        m_thumbnailTimer->stop();
    }
    const QSet<QProcess *> processes = m_thumbnailProcesses;
    m_thumbnailProcesses.clear();
    for (QProcess *process : processes) {
        if (!process) {
            continue;
        }
        disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->deleteLater();
    }
}

void LibraryView::playSelected()
{
    if (m_viewStack->currentIndex() == 1) {
        const QListWidgetItem *item = m_blocks->currentItem();
        if (!item) {
            QMessageBox::information(this, QStringLiteral("Biblioteca"),
                                     QStringLiteral("Selecione um bloco de mídia para reproduzir."));
            return;
        }
        requestOpenFile(item->data(Qt::UserRole).toString());
        return;
    }
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Biblioteca"),
                                 QStringLiteral("Selecione um arquivo de mídia da lista acima para reproduzir."));
        return;
    }
    handleTableDoubleClick(row, 0);
}

void LibraryView::handleTableDoubleClick(int row, int /*column*/)
{
    QTableWidgetItem *item = m_table->item(row, 0);
    if (!item) {
        return;
    }
    QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        filePath = QDir(m_currentFolder).absoluteFilePath(item->text());
    }
    requestOpenFile(filePath);
}

void LibraryView::handleBlockDoubleClick(QListWidgetItem *item)
{
    if (item) {
        requestOpenFile(item->data(Qt::UserRole).toString());
    }
}

void LibraryView::requestOpenFile(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        emit openFileRequested(filePath);
    }
}

bool LibraryView::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_blocks || watched == m_blocks->viewport())
        && event && event->type() == QEvent::Resize) {
        updateBlockLayout();
        scheduleThumbnailLoading();
    }
    return QWidget::eventFilter(watched, event);
}
