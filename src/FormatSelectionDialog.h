#ifndef FORMAT_SELECTION_DIALOG_H
#define FORMAT_SELECTION_DIALOG_H

#include <QDialog>
#include <QString>

#include "MediaMetadata.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QTableWidget;

struct FormatSelectionResult {
    int qualityIndex{-1};
    QString timeRange;
    bool doConvert{false};
    QString convertFormat;
    QString customOutputDir;
};

class FormatSelectionDialog final : public QDialog {
    Q_OBJECT

public:
    FormatSelectionDialog(const MediaMetadata &metadata,
                          int itemCount,
                          int currentQualityIndex,
                          const QString &currentTimeRange,
                          const QString &defaultOutputDir,
                          bool hardwareAcceleration,
                          const QString &hardwareCodec,
                          const QString &baseStyleSheet,
                          QWidget *parent = nullptr);

    FormatSelectionResult result() const;

private:
    void loadThumbnailAsync(const QString &url);
    void updateEstimates(const QString &timeRange);

    const MediaMetadata m_metadata;
    QLabel *m_thumbnailLabel{nullptr};
    QTableWidget *m_table{nullptr};
    QLineEdit *m_editTime{nullptr};
    QCheckBox *m_checkConversion{nullptr};
    QComboBox *m_conversionFormat{nullptr};
    QLineEdit *m_customOutputDir{nullptr};
    QNetworkAccessManager *m_networkManager{nullptr};
};

#endif // FORMAT_SELECTION_DIALOG_H
