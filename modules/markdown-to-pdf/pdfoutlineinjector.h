#ifndef PDFOUTLINEINJECTOR_H
#define PDFOUTLINEINJECTOR_H

#include <QString>
#include <QList>

namespace pdfoutline {

struct Item {
    QString title;
    int page0;          // 0-based PDF page index
    QList<Item> children;
};

// 把 outline 树注入到已有 PDF（使用 incremental update）。
// 假定 PDF 是 Qt QPrinter/QPdfWriter 生成的简单结构（无 ObjectStreams、无加密）。
// 失败时把原因写入 err 并返回 false。
bool inject(const QString& pdfPath, const QList<Item>& items, QString* err = nullptr);

} // namespace pdfoutline

#endif // PDFOUTLINEINJECTOR_H
