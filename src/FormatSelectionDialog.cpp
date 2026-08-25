#include "FormatSelectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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
      m_metadata(metadata)
{
    setWindowTitle(QStringLiteral("Selecione o formato da fonte - Prism Studio Suite"));
    resize(980, 650);
    setStyleSheet(baseStyleSheet + QStringLiteral("QDialog { background-color: #1a1a1a; }"));

    auto *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setSpacing(16);
    dialogLayout->setContentsMargins(24, 24, 24, 24);

    auto *titleLabel = new QLabel(
        QStringLiteral("Selecione o formato da fonte e opções do download:"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 18px; color: #ffffff;"));
    dialogLayout->addWidget(titleLabel);

    auto *metadataLabel = new QLabel(this);
    const QString sourceTitle = m_metadata.title.isEmpty()
        ? QStringLiteral("Título não identificado") : m_metadata.title;
    const QString sourceDuration = m_metadata.durationText.isEmpty()
        ? QStringLiteral("duração não informada") : m_metadata.durationText;
    QString metadataText = QStringLiteral("Fonte: %1  •  Duração: %2")
        .arg(sourceTitle, sourceDuration);
    if (itemCount > 1) {
        metadataText += QStringLiteral("  •  Estimativas baseadas no primeiro de %1 itens")
            .arg(itemCount);
    }
    if (!m_metadata.error.isEmpty()) {
        metadataText += QStringLiteral("\nAviso: %1").arg(m_metadata.error);
    }
    metadataLabel->setText(metadataText);
    metadataLabel->setWordWrap(true);
    metadataLabel->setStyleSheet(m_metadata.error.isEmpty()
                                      ? QStringLiteral("color: #a7f3d0; font-size: 12px;")
                                      : QStringLiteral("color: #fcd34d; font-size: 12px;"));
    dialogLayout->addWidget(metadataLabel);

    m_table = new QTableWidget(4, 4, this);
    QStringList headers;
    headers << QStringLiteral("Qualidade real")
            << QStringLiteral("Formato da fonte / Codec")
            << QStringLiteral("Resolução real / Modo")
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

    const QStringList fallbackQuality{
        QStringLiteral("Melhor disponível"),
        QStringLiteral("Full HD"),
        QStringLiteral("HD"),
        QStringLiteral("Áudio MP3")};
    const QStringList fallbackFormat{
        QStringLiteral("Formato de vídeo"),
        QStringLiteral("Formato de vídeo"),
        QStringLiteral("Formato de vídeo"),
        QStringLiteral("Formato de áudio")};
    const QStringList fallbackMode{
        QStringLiteral("Maior qualidade encontrada"),
        QStringLiteral("Até 1080p"),
        QStringLiteral("Até 720p"),
        QStringLiteral("Somente áudio")};

    for (int i = 0; i < 4; ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(fallbackQuality.at(i)));
        m_table->setItem(i, 1, new QTableWidgetItem(fallbackFormat.at(i)));
        m_table->setItem(i, 2, new QTableWidgetItem(fallbackMode.at(i)));
        m_table->setItem(i, 3, new QTableWidgetItem(MediaMetadataParser::readableBytes(0)));
    }

    for (int i = 0; i < 4; ++i) {
        const MediaFormatOption option = m_metadata.options.value(i);
        if (option.available) {
            const QString outputFormat = i == 3 ? QStringLiteral("MP3") : QStringLiteral("MP4");
            const QString actualQuality = i == 3
                ? QStringLiteral("Áudio")
                : MediaMetadataParser::actualQualityLabel(option.actualHeight);
            m_table->item(i, 0)->setText(actualQuality);
            m_table->item(i, 0)->setToolTip(QStringLiteral(
                "Qualidade real: %1\nFormato real encontrado: %2\nSaída final: %3")
                .arg(actualQuality, option.formatCodec, outputFormat));
            m_table->item(i, 1)->setText(option.formatCodec);
            m_table->item(i, 2)->setText(option.resolutionMode);
            m_table->item(i, 3)->setText(MediaMetadataParser::readableBytes(option.estimatedBytes));
        } else if (!m_metadata.options.isEmpty()) {
            m_table->item(i, 0)->setText(QStringLiteral("Qualidade indisponível"));
            m_table->item(i, 0)->setToolTip(QStringLiteral(
                "Este vídeo não possui uma fonte correspondente a este perfil."));
            m_table->item(i, 1)->setText(option.formatCodec.isEmpty()
                                             ? QStringLiteral("Não disponível neste vídeo")
                                             : option.formatCodec);
            m_table->item(i, 2)->setText(option.resolutionMode.isEmpty()
                                             ? QStringLiteral("Não informado")
                                             : option.resolutionMode);
        } else if (!m_metadata.error.isEmpty()) {
            m_table->item(i, 1)->setText(QStringLiteral("Metadados indisponíveis"));
            m_table->item(i, 2)->setText(QStringLiteral("Não informado"));
        }
    }

    if (!m_metadata.options.isEmpty()) {
        for (int i = 0; i < 3; ++i) {
            const MediaFormatOption option = m_metadata.options.value(i);
            bool hideRow = !option.available;
            int nextIndex = i + 1;
            while (!hideRow && nextIndex < 3
                   && !m_metadata.options.value(nextIndex).available) {
                ++nextIndex;
            }
            if (!hideRow && nextIndex < 3
                && option.actualHeight > 0
                && option.actualHeight == m_metadata.options.value(nextIndex).actualHeight) {
                hideRow = true;
            }
            m_table->setRowHidden(i, hideRow);
        }
    }

    if (currentQualityIndex >= 0 && currentQualityIndex < 4
        && !m_table->isRowHidden(currentQualityIndex)) {
        m_table->selectRow(currentQualityIndex);
    } else {
        for (int i = 0; i < m_table->rowCount(); ++i) {
            if (!m_table->isRowHidden(i)) {
                m_table->selectRow(i);
                break;
            }
        }
    }
    dialogLayout->addWidget(m_table);

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
    dialogLayout->addStretch();

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
