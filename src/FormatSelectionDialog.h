#ifndef FORMAT_SELECTION_DIALOG_H
#define FORMAT_SELECTION_DIALOG_H

#include <QDialog>
#include <QString>

#include "MediaMetadata.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

struct FormatSelectionResult {
    int qualityIndex{-1};
    QString timeRange;
    bool doConvert{false};
    QString convertFormat;
    QString customOutputDir;
};

class FormatSelectionDialog final : public QDialog {
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
    void updateEstimates(const QString &timeRange);

    const MediaMetadata m_metadata;
    QTableWidget *m_table{nullptr};
    QLineEdit *m_editTime{nullptr};
    QCheckBox *m_checkConversion{nullptr};
    QComboBox *m_conversionFormat{nullptr};
    QLineEdit *m_customOutputDir{nullptr};
};

#endif // FORMAT_SELECTION_DIALOG_H
