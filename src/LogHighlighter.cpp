#include "LogHighlighter.h"

#include <QColor>
#include <QTextCharFormat>

LogHighlighter::LogHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

void LogHighlighter::highlightBlock(const QString &text)
{
    QTextCharFormat format;
    if (text.contains(QStringLiteral("[ERROR]"))) {
        format.setForeground(QColor(QStringLiteral("#fca5a5")));
    } else if (text.contains(QStringLiteral("[WARN]"))) {
        format.setForeground(QColor(QStringLiteral("#fcd34d")));
    } else {
        format.setForeground(QColor(QStringLiteral("#a7f3d0")));
    }
    setFormat(0, text.size(), format);
}
