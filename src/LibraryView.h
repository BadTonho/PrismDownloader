#ifndef LIBRARY_VIEW_H
#define LIBRARY_VIEW_H

#include <QHash>
#include <QFileInfo>
#include <QList>
#include <QPixmap>
#include <QSet>
#include <QWidget>

class QFileInfo;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QProcess;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

class LibraryView final : public QWidget {
    Q_OBJECT

public:
    explicit LibraryView(QWidget *parent = nullptr);
    ~LibraryView() override;

    void refresh(const QString &folder);

signals:
    void openFileRequested(const QString &filePath);
    void openFolderRequested();
    void logMessageRequested(const QString &message);

private slots:
    void refreshCurrentFolder();
    void setListView();
    void setBlocksView();
    void playSelected();
    void handleTableDoubleClick(int row, int column);
    void handleBlockDoubleClick(QListWidgetItem *item);
    void loadVisibleThumbnails();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setViewMode(bool blocks);
    void refreshBlocks(const QFileInfoList &fileList);
    void updateBlockLayout();
    void scheduleThumbnailLoading();
    void loadThumbnail(const QFileInfo &fileInfo, QLabel *thumbnailLabel);
    void stopThumbnailProcesses();
    void requestOpenFile(const QString &filePath);

    QString m_currentFolder;
    QTableWidget *m_table{nullptr};
    QStackedWidget *m_viewStack{nullptr};
    QListWidget *m_blocks{nullptr};
    QPushButton *m_listViewButton{nullptr};
    QPushButton *m_blocksViewButton{nullptr};
    QTimer *m_thumbnailTimer{nullptr};
    QHash<QString, QPixmap> m_thumbnailCache;
    QSet<QProcess *> m_thumbnailProcesses;
};

#endif // LIBRARY_VIEW_H
