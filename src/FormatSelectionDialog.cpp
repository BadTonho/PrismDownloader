#include "FormatSelectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString audioTrackDisplayText(const MediaAudioTrack &track)
{
    QString label = track.language.isEmpty()
        ? (track.formatNote.isEmpty() ? QStringLiteral("Idioma não informado")
                                      : track.formatNote)
        : track.language;
    if (!track.formatNote.isEmpty() && track.formatNote.compare(label, Qt::CaseInsensitive) != 0) {
        label += QStringLiteral(" — ") + track.formatNote;
    }

    QStringList technicalDetails;
    const QString codec = track.audioCodec.isEmpty() ? QString() : track.audioCodec;
    const QString extension = track.ext.isEmpty() ? QString() : track.ext.toUpper();
    if (!extension.isEmpty() || !codec.isEmpty()) {
        technicalDetails.append(extension.isEmpty() ? codec
            : codec.isEmpty() ? extension : extension + QStringLiteral("/") + codec);
    }
    if (track.bitrateKbps > 0.0) {
        technicalDetails.append(QStringLiteral("%1 kb/s").arg(qRound(track.bitrateKbps)));
    }
    if (!technicalDetails.isEmpty()) {
        label += QStringLiteral(" — ") + technicalDetails.join(QStringLiteral(", "));
    }
    if (!track.formatId.isEmpty()) {
        label += QStringLiteral(" [%1]").arg(track.formatId);
    }
    return label;
}

QString audioTrackCodecText(const MediaAudioTrack &track)
{
    const QString extension = track.ext.isEmpty() ? QString() : track.ext.toUpper();
    if (extension.isEmpty()) {
        return track.audioCodec;
    }
    return track.audioCodec.isEmpty()
        ? extension : extension + QStringLiteral("/") + track.audioCodec;
}

}

FormatSelectionDialog::FormatSelectionDialog(const MediaMetadata &metadata,
                                             int itemCount,
                                             int currentQualityIndex,
                                             const QString &currentTimeRange,
                                             const QString &defaultOutputDir,
                                             bool hardwareAcceleration,
                                             const QString &hardwareCodec,
                                             const QString &baseStyleSheet,
                                             QNetworkAccessManager *network,
                                             QWidget *parent)
    : QDialog(parent),
      m_metadata(metadata),
      m_networkManager(network ? network : new QNetworkAccessManager(this)),
      m_ownsNetworkManager(network == nullptr)
{
    setWindowTitle(QStringLiteral("Selecione o formato da fonte - Prism Studio Suite"));
    resize(1020, 680);
    setStyleSheet(baseStyleSheet + QStringLiteral("QDialog { background-color: #1a1a1a; }"));

    auto *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setSpacing(14);
    dialogLayout->setContentsMargins(22, 20, 22, 20);

    auto *titleLabel = new QLabel(
        QStringLiteral("Selecione o formato da fonte e opções do download:"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 17px; color: #ffffff;"));
    dialogLayout->addWidget(titleLabel);

    // ==========================================
    // CARD DE CABEÇALHO COM PREVIEW DE MINIATURA
    // ==========================================
    auto *headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("headerCard"));
    headerCard->setStyleSheet(QStringLiteral(
        "QFrame#headerCard {"
        "  background-color: #212121;"
        "  border: 1px solid #333333;"
        "  border-radius: 8px;"
        "}"));
    auto *cardLayout = new QHBoxLayout(headerCard);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(16);

    m_thumbnailLabel = new QLabel(headerCard);
    m_thumbnailLabel->setFixedSize(176, 99);
    m_thumbnailLabel->setAlignment(Qt::AlignCenter);
    m_thumbnailLabel->setStyleSheet(QStringLiteral(
        "background-color: #121212;"
        "border: 1px solid #3a3a3a;"
        "border-radius: 6px;"
        "color: #777777;"
        "font-size: 11px;"
        "font-weight: bold;"));
    m_thumbnailLabel->setText(QStringLiteral("Carregando\nminiatura..."));
    cardLayout->addWidget(m_thumbnailLabel, 0);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(6);
    infoLayout->setContentsMargins(0, 2, 0, 2);

    const QString sourceTitle = m_metadata.title.isEmpty()
        ? QStringLiteral("Título não identificado") : m_metadata.title;
    auto *mediaTitleLabel = new QLabel(sourceTitle, headerCard);
    mediaTitleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px; color: #ffffff;"));
    mediaTitleLabel->setWordWrap(true);
    infoLayout->addWidget(mediaTitleLabel);

    const QString sourceDuration = m_metadata.durationText.isEmpty()
        ? QStringLiteral("desconhecida") : m_metadata.durationText;
    const QString uploaderText = m_metadata.uploader.isEmpty()
        ? QString() : QStringLiteral("Canal: %1  •  ").arg(m_metadata.uploader);

    const int videoCount = static_cast<int>(std::count_if(
        m_metadata.options.begin(), m_metadata.options.end(),
        [](const MediaFormatOption &opt) { return !opt.isAudio; }));
    const int audioCount = m_metadata.options.size() - videoCount;

    QString details = QStringLiteral("%1Duração: %2  •  %3 resolução(ões) de vídeo, %4 formato(s) de áudio")
        .arg(uploaderText, sourceDuration)
        .arg(videoCount)
        .arg(audioCount);
    if (itemCount > 1) {
        details += QStringLiteral("  •  Lote: 1 de %1 itens").arg(itemCount);
    }

    auto *detailsLabel = new QLabel(details, headerCard);
    detailsLabel->setStyleSheet(QStringLiteral("color: #10b981; font-size: 12px; font-weight: 500;"));
    detailsLabel->setWordWrap(true);
    infoLayout->addWidget(detailsLabel);

    if (!m_metadata.error.isEmpty()) {
        auto *warnLabel = new QLabel(QStringLiteral("Aviso: %1").arg(m_metadata.error), headerCard);
        warnLabel->setStyleSheet(QStringLiteral("color: #fcd34d; font-size: 11px;"));
        warnLabel->setWordWrap(true);
        infoLayout->addWidget(warnLabel);
    }
    infoLayout->addStretch();
    cardLayout->addLayout(infoLayout, 1);
    dialogLayout->addWidget(headerCard);

    const QStringList candidateUrls = m_metadata.thumbnailCandidates.isEmpty()
        ? QStringList{m_metadata.thumbnailUrl}
        : m_metadata.thumbnailCandidates;
    loadThumbnailAsync(candidateUrls, 0);

    // ==========================================
    // TABELA DE TODOS OS FORMATOS DISPONÍVEIS
    // ==========================================
    const int rowCount = qMax(1, m_metadata.options.size());
    m_table = new QTableWidget(rowCount, 4, this);
    QStringList headers;
    headers << QStringLiteral("Qualidade / Resolução")
            << QStringLiteral("Formato da fonte / Codec")
            << QStringLiteral("Resolução real / Taxa")
            << QStringLiteral("Estimativa");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setObjectName(QStringLiteral("libraryTable"));
    m_table->setMinimumHeight(210);

    if (m_metadata.options.isEmpty()) {
        m_table->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Melhor disponível")));
        m_table->setItem(0, 1, new QTableWidgetItem(QStringLiteral("Formato padrão do servidor")));
        m_table->setItem(0, 2, new QTableWidgetItem(QStringLiteral("Automático")));
        m_table->setItem(0, 3, new QTableWidgetItem(QStringLiteral("—")));
    } else {
        for (int i = 0; i < m_metadata.options.size(); ++i) {
            const MediaFormatOption &option = m_metadata.options.at(i);
            const QString quality = option.qualityLabel.isEmpty()
                ? (option.isAudio ? QStringLiteral("Áudio MP3") : MediaMetadataParser::actualQualityLabel(option.actualHeight))
                : option.qualityLabel;
            m_table->setItem(i, 0, new QTableWidgetItem(quality));
            m_table->setItem(i, 1, new QTableWidgetItem(option.formatCodec));
            m_table->setItem(i, 2, new QTableWidgetItem(option.resolutionMode));
            m_table->setItem(i, 3, new QTableWidgetItem(MediaMetadataParser::readableBytes(option.estimatedBytes)));
        }
    }

    int selectedRow = 0;
    if (currentQualityIndex >= 0 && currentQualityIndex < m_metadata.options.size()) {
        selectedRow = currentQualityIndex;
    }
    m_table->selectRow(selectedRow);
    dialogLayout->addWidget(m_table, 1);

    auto *audioTrackLayout = new QHBoxLayout();
    auto *audioTrackTitle = new QLabel(QStringLiteral("Faixa de áudio:"), this);
    audioTrackTitle->setStyleSheet(QStringLiteral("color: #a3a3a3; font-weight: bold;"));
    m_audioTrackCombo = new QComboBox(this);
    m_audioTrackCombo->setObjectName(QStringLiteral("audioTrackCombo"));
    m_audioTrackCombo->setMinimumHeight(32);
    for (int index = 0; index < m_metadata.audioTracks.size(); ++index) {
        const MediaAudioTrack &track = m_metadata.audioTracks.at(index);
        m_audioTrackCombo->addItem(audioTrackDisplayText(track), index);
        m_audioTrackCombo->setItemData(index,
            QStringLiteral("ID do formato: %1\nIdioma: %2\nNota: %3")
                .arg(track.formatId,
                     track.language.isEmpty() ? QStringLiteral("não informado") : track.language,
                     track.formatNote), Qt::ToolTipRole);
    }
    if (m_metadata.audioTracks.isEmpty()) {
        m_audioTrackCombo->addItem(QStringLiteral("Faixa padrão da fonte"), -1);
        m_audioTrackCombo->setEnabled(false);
    } else if (m_metadata.preferredAudioTrackIndex >= 0
               && m_metadata.preferredAudioTrackIndex < m_metadata.audioTracks.size()) {
        m_audioTrackCombo->setCurrentIndex(m_metadata.preferredAudioTrackIndex);
    }
    audioTrackLayout->addWidget(audioTrackTitle);
    audioTrackLayout->addWidget(m_audioTrackCombo, 1);
    dialogLayout->addLayout(audioTrackLayout);
    connect(m_audioTrackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { updateEstimates(m_editTime ? m_editTime->text() : QString()); });
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { updateAudioTrackAvailability(); });
    updateAudioTrackAvailability();

    // ==========================================
    // OPÇÕES ADICIONAIS (RECORTE, CONVERSÃO, PASTA)
    // ==========================================
    auto *optionsLayout = new QGridLayout();
    optionsLayout->setSpacing(12);

    auto *timeLabel = new QLabel(QStringLiteral("Recorte de Tempo (Opcional):"), this);
    timeLabel->setStyleSheet(QStringLiteral("color: #a3a3a3; font-weight: bold;"));
    m_editTime = new QLineEdit(this);
    m_editTime->setText(currentTimeRange);
    m_editTime->setPlaceholderText(
        QStringLiteral("Ex: 00:01:15-00:03:00 (Vazio = baixar completo)"));
    connect(m_editTime, &QLineEdit::textChanged, this,
            [this](const QString &value) { updateEstimates(value); });

    m_checkConversion = new QCheckBox(
        QStringLiteral("Converter para outro formato"), this);
    m_checkConversion->setStyleSheet(
        QStringLiteral("color: #38bdf8; font-weight: bold; font-size: 13px;"));
    m_checkConversion->setCursor(Qt::PointingHandCursor);

    m_conversionFormat = new QComboBox(this);
    m_conversionFormat->addItem(QStringLiteral("MP4 (H.264 / Aceleração quando disponível)"));
    m_conversionFormat->addItem(QStringLiteral("MP4 (HEVC / H.265 - Compressão de Alta Densidade)"));
    m_conversionFormat->addItem(QStringLiteral("MKV (Matroska - Container Sem Perdas)"));
    m_conversionFormat->addItem(QStringLiteral("MP3 (Áudio MP3 Alta Fidelidade - 320kbps)"));
    m_conversionFormat->addItem(QStringLiteral("WAV (Áudio Sem Compressão / Estúdios)"));
    m_conversionFormat->addItem(QStringLiteral("WEBM (Otimizado para Web e Redes Sociais)"));
    m_conversionFormat->setEnabled(false);
    connect(m_checkConversion, &QCheckBox::toggled,
            m_conversionFormat, &QComboBox::setEnabled);

    auto *folderLabel = new QLabel(QStringLiteral("Salvar este download em:"), this);
    folderLabel->setStyleSheet(QStringLiteral("color: #a3a3a3; font-weight: bold;"));
    auto *folderLayout = new QHBoxLayout();
    m_customOutputDir = new QLineEdit(this);
    m_customOutputDir->setText(defaultOutputDir);
    auto *changeFolderButton = new QPushButton(
        QStringLiteral("Mudar Pasta (Apenas Este)"), this);
    changeFolderButton->setObjectName(QStringLiteral("browseBtn"));
    changeFolderButton->setCursor(Qt::PointingHandCursor);
    changeFolderButton->setMinimumHeight(32);
    connect(changeFolderButton, &QPushButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Escolha a Pasta Exclusiva Para Este Download"),
            m_customOutputDir->text());
        if (!directory.isEmpty()) {
            m_customOutputDir->setText(directory);
        }
    });
    folderLayout->addWidget(m_customOutputDir, 1);
    folderLayout->addWidget(changeFolderButton, 0);

    optionsLayout->addWidget(timeLabel, 0, 0);
    optionsLayout->addWidget(m_editTime, 0, 1);
    optionsLayout->addWidget(m_checkConversion, 1, 0);
    optionsLayout->addWidget(m_conversionFormat, 1, 1);
    optionsLayout->addWidget(folderLabel, 2, 0);
    optionsLayout->addLayout(folderLayout, 2, 1);
    optionsLayout->setColumnStretch(1, 1);
    dialogLayout->addLayout(optionsLayout);

    // ==========================================
    // BOTÕES DE AÇÃO INFERIORES
    // ==========================================
    auto *buttonsLayout = new QHBoxLayout();
    auto *okButton = new QPushButton(QStringLiteral("ADICIONAR À FILA"), this);
    okButton->setObjectName(QStringLiteral("startBtn"));
    okButton->setCursor(Qt::PointingHandCursor);
    okButton->setMinimumHeight(42);
    okButton->setFixedWidth(185);
    okButton->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

    auto *cancelButton = new QPushButton(QStringLiteral("CANCELAR"), this);
    cancelButton->setObjectName(QStringLiteral("cancelBtn"));
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setMinimumHeight(42);
    cancelButton->setFixedWidth(140);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    const QString accelerationStatus = hardwareAcceleration
        ? QStringLiteral("Aceleração disponível: ") + hardwareCodec.toUpper()
        : QStringLiteral("Conversão será feita pela CPU");
    auto *accelerationLabel = new QLabel(accelerationStatus, this);
    accelerationLabel->setStyleSheet(hardwareAcceleration
        ? QStringLiteral("color: #10b981; font-weight: bold; font-size: 13px;")
        : QStringLiteral("color: #f59e0b; font-weight: bold; font-size: 13px;"));

    buttonsLayout->addWidget(okButton);
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(accelerationLabel);
    dialogLayout->addLayout(buttonsLayout);

    updateEstimates(m_editTime->text());
}

void FormatSelectionDialog::loadThumbnailAsync(const QStringList &candidateUrls, int candidateIndex)
{
    if (candidateIndex >= candidateUrls.size()) {
        if (m_thumbnailLabel) {
            m_thumbnailLabel->setText(QStringLiteral("Sem\nminiatura"));
        }
        return;
    }

    const QString url = candidateUrls.at(candidateIndex).trimmed();
    if (url.isEmpty()) {
        loadThumbnailAsync(candidateUrls, candidateIndex + 1);
        return;
    }

    const QUrl parsedUrl(url);
    if (!parsedUrl.isValid() || parsedUrl.host().isEmpty()) {
        loadThumbnailAsync(candidateUrls, candidateIndex + 1);
        return;
    }

    QNetworkRequest request(parsedUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"));
    request.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto imageData = std::make_shared<QByteArray>();
    QNetworkReply *reply = m_networkManager->get(request);
    const QPointer<QLabel> labelGuard = m_thumbnailLabel;
    const QStringList urls = candidateUrls;

    connect(reply, &QIODevice::readyRead, reply, [reply, imageData]() {
        constexpr qsizetype kMaximumThumbnailBytes = 8 * 1024 * 1024;
        const QByteArray chunk = reply->readAll();
        if (imageData->size() + chunk.size() > kMaximumThumbnailBytes) {
            reply->abort();
            return;
        }
        imageData->append(chunk);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, imageData, labelGuard, urls, candidateIndex]() {
        reply->deleteLater();
        if (!labelGuard) {
            return;
        }
        if (reply->error() == QNetworkReply::NoError && !imageData->isEmpty()) {
            QPixmap pixmap;
            if (pixmap.loadFromData(*imageData)) {
                labelGuard->setPixmap(pixmap.scaled(
                    labelGuard->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                return;
            }
        }
        loadThumbnailAsync(urls, candidateIndex + 1);
    });
}

void FormatSelectionDialog::updateEstimates(const QString &timeRange)
{
    if (!m_table) {
        return;
    }
    const double duration = MediaMetadataParser::selectedDurationSeconds(
        timeRange, m_metadata.durationSeconds);
    const int audioTrackIndex = m_audioTrackCombo ? m_audioTrackCombo->currentData().toInt() : -1;
    const MediaAudioTrack *selectedAudio = audioTrackIndex >= 0
        && audioTrackIndex < m_metadata.audioTracks.size()
        ? &m_metadata.audioTracks.at(audioTrackIndex) : nullptr;
    const double selectedAudioBytesPerSecond = selectedAudio
        ? (selectedAudio->estimatedBytes > 0 && m_metadata.durationSeconds > 0.0
               ? static_cast<double>(selectedAudio->estimatedBytes) / m_metadata.durationSeconds
               : selectedAudio->bitrateKbps * 1000.0 / 8.0)
        : 0.0;
    for (int index = 0; index < m_metadata.options.size() && index < m_table->rowCount(); ++index) {
        const MediaFormatOption &option = m_metadata.options.at(index);
        if (!option.available) {
            continue;
        }
        double bytesPerSecond = option.estimatedBytesPerSecond;
        QString formatCodec = option.formatCodec;
        if (option.isAudio && selectedAudio) {
            bytesPerSecond = selectedAudioBytesPerSecond;
            formatCodec = QStringLiteral("MP3 • origem %1")
                .arg(audioTrackCodecText(*selectedAudio));
        } else if (option.canSelectAudio && selectedAudio && selectedAudio->isAudioOnly) {
            bytesPerSecond = option.videoEstimatedBytesPerSecond + selectedAudioBytesPerSecond;
            formatCodec += QStringLiteral(" + %1").arg(audioTrackCodecText(*selectedAudio));
        }
        const qint64 estimate = bytesPerSecond > 0.0 && duration > 0.0
            ? qRound64(bytesPerSecond * duration) : option.estimatedBytes;
        if (m_table->item(index, 1)) {
            m_table->item(index, 1)->setText(formatCodec);
        }
        m_table->item(index, 3)->setText(MediaMetadataParser::readableBytes(estimate));
        m_table->item(index, 3)->setToolTip(QStringLiteral(
            "Estimativa baseada no tamanho/bitrate informado pelo servidor; o resultado pode variar."));
    }
}

void FormatSelectionDialog::updateAudioTrackAvailability()
{
    if (!m_audioTrackCombo || !m_table) {
        return;
    }
    const int qualityIndex = m_table->currentRow();
    const bool audioOnly = qualityIndex >= 0 && qualityIndex < m_metadata.options.size()
        && m_metadata.options.at(qualityIndex).isAudio;
    const bool canReplaceAudio = audioOnly
        || (qualityIndex >= 0 && qualityIndex < m_metadata.options.size()
            && m_metadata.options.at(qualityIndex).canSelectAudio);
    m_audioTrackCombo->setEnabled(!m_metadata.audioTracks.isEmpty() && canReplaceAudio);
    if (!canReplaceAudio && !audioOnly) {
        m_audioTrackCombo->setToolTip(QStringLiteral(
            "Este formato já inclui uma faixa de áudio embutida e não permite substituí-la."));
    } else {
        m_audioTrackCombo->setToolTip(QString());
    }
    updateEstimates(m_editTime ? m_editTime->text() : QString());
}

FormatSelectionResult FormatSelectionDialog::result() const
{
    FormatSelectionResult selection;
    selection.qualityIndex = m_table ? m_table->currentRow() : -1;
    selection.audioTrackIndex = m_audioTrackCombo ? m_audioTrackCombo->currentData().toInt() : -1;
    const MediaAudioTrack *audioTrack = selection.audioTrackIndex >= 0
        && selection.audioTrackIndex < m_metadata.audioTracks.size()
        ? &m_metadata.audioTracks.at(selection.audioTrackIndex) : nullptr;
    if (audioTrack) {
        selection.audioLanguage = audioTrack->language;
    }

    if (selection.qualityIndex >= 0 && selection.qualityIndex < m_metadata.options.size()) {
        const MediaFormatOption &option = m_metadata.options.at(selection.qualityIndex);
        if (option.isAudio) {
            selection.formatSelector = audioTrack ? audioTrack->formatId : option.formatSelector;
        } else if (option.canSelectAudio && audioTrack && audioTrack->isAudioOnly
                   && !option.videoFormatId.isEmpty() && !audioTrack->formatId.isEmpty()) {
            selection.formatSelector = option.videoFormatId + QStringLiteral("+") + audioTrack->formatId;
        } else {
            selection.formatSelector = option.videoFormatId.isEmpty()
                ? option.formatSelector : option.videoFormatId;
        }
    }
    selection.timeRange = m_editTime ? m_editTime->text().trimmed() : QString();
    selection.doConvert = m_checkConversion && m_checkConversion->isChecked();
    selection.convertFormat = m_conversionFormat ? m_conversionFormat->currentText() : QString();
    selection.customOutputDir = m_customOutputDir
        ? m_customOutputDir->text().trimmed() : QString();
    return selection;
}
