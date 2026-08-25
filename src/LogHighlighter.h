#ifndef PRISM_LOG_HIGHLIGHTER_H
#define PRISM_LOG_HIGHLIGHTER_H

#include <QSyntaxHighlighter>

class LogHighlighter final : public QSyntaxHighlighter {
public:
    explicit LogHighlighter(QTextDocument *document);

protected:
    void highlightBlock(const QString &text) override;
};

#endif // PRISM_LOG_HIGHLIGHTER_H
