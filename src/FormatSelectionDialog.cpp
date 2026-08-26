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

FormatSelectionDialog::FormatSelectionDialog(const MediaMetadata &metadata,
                                             int itemCount,
                                             int currentQualityIndex,
                                             const QString &currentTimeRange,
                                             const QString &defaultOutputDir,
                                             bool hardwareAcceleration,
                                             const QString &hardwareCodec,
                                             const QString &baseStyleSheet,
                                             QWidget *parent)
    : QDialog(parent),
      m_metadata(metadata),
      m_networkManager(new QNetworkAccessManager(this))
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

    QString details = QStringLiteral("%1⏱ Duração: %2  •  📊 %3 resolução(ões) de vídeo, %4 formato(s) de áudio")
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
        auto *warnLabel = new QLabel(QStringLiteral("⚠️ %1").arg(m_metadata.error), headerCard);
        warnLabel->setStyleSheet(QStringLiteral("color: #fcd34d; font-size: 11px;"));
        warnLabel->setWordWrap(true);
        infoLayout->addWidget(warnLabel);
    }
    infoLayout->addStretch();
    cardLayout->addLayout(infoLayout, 1);
    dialogLayout->addWidget(headerCard);

    loadThumbnailAsync(m_metadata.thumbnailUrl);

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
        QStringLiteral("Converter para outro formato após concluir o download"), this);
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
        ? QStringLiteral("⚡ Aceleração disponível: ") + hardwareCodec.toUpper()
        : QStringLiteral("ℹ️ Conversão será feita pela CPU");
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

void FormatSelectionDialog::loadThumbnailAsync(const QString &url)
{
    if (url.isEmpty()) {
        if (m_thumbnailLabel) {
            m_thumbnailLabel->setText(QStringLiteral("Sem\nminiatura"));
        }
        return;
    }

    const QUrl parsedUrl(url);
    QNetworkRequest request(parsedUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_networkManager->get(request);
    const QPointer<QLabel> labelGuard = m_thumbnailLabel;
    connect(reply, &QNetworkReply::finished, this, [reply, labelGuard]() {
        reply->deleteLater();
        if (!labelGuard) {
            return;
        }
        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                labelGuard->setPixmap(pixmap.scaled(
                    labelGuard->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                return;
            }
        }
        labelGuard->setText(QStringLiteral("Miniatura\nindisponível"));
    });
}

void FormatSelectionDialog::updateEstimates(const QString &timeRange)
{
    if (!m_table) {
        return;
    }
    const double duration = MediaMetadataParser::selectedDurationSeconds(
        timeRange, m_metadata.durationSeconds);
    for (int index = 0; index < m_metadata.options.size() && index < m_table->rowCount(); ++index) {
        const MediaFormatOption &option = m_metadata.options.at(index);
        if (!option.available) {
            continue;
        }
        const qint64 estimate = option.estimatedBytesPerSecond > 0.0 && duration > 0.0
            ? qRound64(option.estimatedBytesPerSecond * duration)
            : option.estimatedBytes;
        m_table->item(index, 3)->setText(MediaMetadataParser::readableBytes(estimate));
        m_table->item(index, 3)->setToolTip(QStringLiteral(
            "Estimativa baseada no tamanho/bitrate informado pelo servidor; o resultado pode variar."));
    }
}

FormatSelectionResult FormatSelectionDialog::result() const
{
    FormatSelectionResult selection;
    selection.qualityIndex = m_table ? m_table->currentRow() : -1;
    selection.timeRange = m_editTime ? m_editTime->text().trimmed() : QString();
    selection.doConvert = m_checkConversion && m_checkConversion->isChecked();
    selection.convertFormat = m_conversionFormat ? m_conversionFormat->currentText() : QString();
    selection.customOutputDir = m_customOutputDir
        ? m_customOutputDir->text().trimmed() : QString();
    return selection;
}
