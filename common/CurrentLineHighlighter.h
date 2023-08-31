#ifndef CURRENTLINEHIGHLIGHTER_H
#define CURRENTLINEHIGHLIGHTER_H

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QSyntaxHighlighter>
#include <QColor>

class CurrentLineHighlighter : public QSyntaxHighlighter
{
public:
    CurrentLineHighlighter(QTextEdit* editor) : QSyntaxHighlighter(editor), editor(editor)
    {
        currentLineBackground.setForeground(QBrush(Qt::yellow)); // 设置当前行背景色
    }

protected:
    void highlightBlock(const QString &text) override
    {
        if (!editor)
            return;

        QTextCursor cursor = editor->textCursor();
        QTextBlock currentBlock = cursor.block();

        if (currentBlock.isValid() && currentBlock.isVisible())
        {
            int blockNumber = currentBlock.blockNumber();
            QTextEdit::ExtraSelection selection;

            // 设置当前行的选区
            selection.format = currentLineBackground;
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = cursor;
            selection.cursor.clearSelection();

            QList<QTextEdit::ExtraSelection> extraSelections;
            extraSelections.append(selection);

            editor->setExtraSelections(extraSelections);
        }
    }

private:
    QTextEdit* editor;
    QTextBlock currentBlock;
    QTextEdit::ExtraSelection selection;
    QTextCharFormat currentLineBackground;
};
#endif // CURRENTLINEHIGHLIGHTER_H
