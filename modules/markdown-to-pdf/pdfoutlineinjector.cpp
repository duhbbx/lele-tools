#include "pdfoutlineinjector.h"

#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>

namespace pdfoutline {

namespace {

// 末尾的 startxref <off> %%EOF 中的偏移
qint64 findStartxref(const QByteArray& data)
{
    int idx = data.lastIndexOf("startxref");
    if (idx < 0) return -1;
    int p = idx + 9;
    while (p < data.size() && (data[p] == ' ' || data[p] == '\r' || data[p] == '\n')) p++;
    int q = p;
    while (q < data.size() && data[q] >= '0' && data[q] <= '9') q++;
    return data.mid(p, q - p).toLongLong();
}

// 从 trailer 字典中找 /Root 的对象 ID
int findRootId(const QByteArray& data)
{
    int idx = data.lastIndexOf("trailer");
    if (idx < 0) return -1;
    int sx = data.indexOf("startxref", idx);
    QByteArray trailer = data.mid(idx, sx - idx);
    QRegularExpression rx(R"(/Root\s+(\d+)\s+\d+\s+R)");
    auto m = rx.match(QString::fromLatin1(trailer));
    return m.hasMatch() ? m.captured(1).toInt() : -1;
}

// 解析 xref：id → 字节偏移
QHash<int, qint64> parseXref(const QByteArray& data, qint64 xrefOffset)
{
    QHash<int, qint64> map;
    int p = static_cast<int>(xrefOffset);
    if (p < 0 || p + 4 > data.size() || data.mid(p, 4) != "xref") return map;
    p += 4;
    while (p < data.size() && (data[p] == '\r' || data[p] == '\n')) p++;

    while (p < data.size()) {
        int eol = data.indexOf('\n', p);
        if (eol < 0) break;
        QByteArray hdr = data.mid(p, eol - p).trimmed();
        if (hdr.startsWith("trailer")) break;
        QList<QByteArray> parts = hdr.split(' ');
        if (parts.size() != 2) break;
        int first = parts[0].toInt();
        int count = parts[1].toInt();
        p = eol + 1;
        for (int i = 0; i < count; ++i) {
            if (p + 20 > data.size()) return map;
            QByteArray entry = data.mid(p, 20);
            qint64 off = entry.left(10).toLongLong();
            char status = entry.at(17);
            if (status == 'n') map[first + i] = off;
            p += 20;
        }
    }
    return map;
}

// 读取 obj 内部体（"<id> 0 obj"~"endobj" 之间）
QByteArray readObjectBody(const QByteArray& data, qint64 offset)
{
    int start = data.indexOf("obj", static_cast<int>(offset));
    if (start < 0) return QByteArray();
    start += 3;
    while (start < data.size() &&
           (data[start] == ' ' || data[start] == '\n' || data[start] == '\r')) start++;
    int end = data.indexOf("endobj", start);
    if (end < 0) return QByteArray();
    return data.mid(start, end - start).trimmed();
}

// 将 /Pages 树展开为顺序的页对象 ID 列表
void flattenPages(const QByteArray& data, const QHash<int, qint64>& xref,
                  int nodeId, QList<int>& out)
{
    qint64 off = xref.value(nodeId, -1);
    if (off < 0) return;
    QByteArray body = readObjectBody(data, off);
    QString s = QString::fromLatin1(body);

    // 是 /Page 叶节点？
    if (s.contains(QRegularExpression(R"(/Type\s*/Page\b(?!s))"))) {
        out.append(nodeId);
        return;
    }

    int kidsIdx = s.indexOf("/Kids");
    if (kidsIdx < 0) return;
    int lb = s.indexOf('[', kidsIdx);
    int rb = s.indexOf(']', lb);
    if (lb < 0 || rb < 0) return;
    QString kids = s.mid(lb + 1, rb - lb - 1);
    QRegularExpression rx(R"((\d+)\s+\d+\s+R)");
    auto it = rx.globalMatch(kids);
    while (it.hasNext()) {
        auto m = it.next();
        flattenPages(data, xref, m.captured(1).toInt(), out);
    }
}

// PDF 16-bit hex string，UTF-16BE BOM，安全支持中文
QByteArray pdfHexString(const QString& s)
{
    QByteArray ba;
    ba.append(static_cast<char>(0xFE));
    ba.append(static_cast<char>(0xFF));
    for (QChar c : s) {
        ushort u = c.unicode();
        ba.append(static_cast<char>((u >> 8) & 0xFF));
        ba.append(static_cast<char>(u & 0xFF));
    }
    return "<" + ba.toHex() + ">";
}

// 内部表示：扁平化的 outline 节点，便于设置 Prev/Next/First/Last
struct Flat {
    QString title;
    int page0 = 0;
    int level = 0;          // 0 = 顶层
    int id = 0;
    int parentId = 0;
    int prevId = 0;
    int nextId = 0;
    int firstChildId = 0;
    int lastChildId = 0;
    int directChildren = 0;
    int totalDescendants = 0;  // 用于 /Count
};

// 第一次遍历：分配 ID + 建扁平表
void assignIds(const QList<Item>& items, int parentId, int level,
               QList<Flat>& flat, int& nextId, QList<int>& siblingIds)
{
    QList<int> myIds;
    QList<int> myFlatIdxs;

    for (const auto& it : items) {
        Flat f;
        f.title = it.title;
        f.page0 = it.page0;
        f.level = level;
        f.id = nextId++;
        f.parentId = parentId;
        f.directChildren = it.children.size();
        flat.append(f);
        myIds.append(f.id);
        myFlatIdxs.append(flat.size() - 1);
        siblingIds.append(f.id);
    }

    for (int i = 0; i < items.size(); ++i) {
        QList<int> childIds;
        assignIds(items[i].children, myIds[i], level + 1, flat, nextId, childIds);
        if (!childIds.isEmpty()) {
            flat[myFlatIdxs[i]].firstChildId = childIds.first();
            flat[myFlatIdxs[i]].lastChildId = childIds.last();
        }
    }

    // 同级兄弟 prev/next
    for (int i = 0; i < myFlatIdxs.size(); ++i) {
        if (i > 0) flat[myFlatIdxs[i]].prevId = myIds[i - 1];
        if (i < myIds.size() - 1) flat[myFlatIdxs[i]].nextId = myIds[i + 1];
    }
}

// 计算每个节点的 totalDescendants（即子树大小，用于 /Count 取负还是正取决于是否折叠；展开默认为正）
int computeDescendants(QList<Flat>& flat, int idx)
{
    int total = flat[idx].directChildren;
    // 找直接子节点：紧随其后的 level == self.level+1 的连续片段
    int myLevel = flat[idx].level;
    for (int j = idx + 1; j < flat.size(); ++j) {
        if (flat[j].level <= myLevel) break;
        if (flat[j].level == myLevel + 1) {
            total += computeDescendants(flat, j);
        }
    }
    flat[idx].totalDescendants = total;
    return total;
}

} // anonymous namespace

bool inject(const QString& pdfPath, const QList<Item>& items, QString* err)
{
    if (items.isEmpty()) return true;  // 没有书签直接成功

    QFile f(pdfPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法读取 PDF: %1").arg(pdfPath);
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    qint64 oldStartxref = findStartxref(data);
    if (oldStartxref < 0) {
        if (err) *err = QStringLiteral("找不到 startxref");
        return false;
    }

    int rootId = findRootId(data);
    if (rootId < 0) {
        if (err) *err = QStringLiteral("找不到 /Root");
        return false;
    }

    QHash<int, qint64> xref = parseXref(data, oldStartxref);
    if (xref.isEmpty()) {
        if (err) *err = QStringLiteral("解析 xref 失败（可能 PDF 用了 ObjectStreams）");
        return false;
    }

    qint64 catOff = xref.value(rootId, -1);
    if (catOff < 0) {
        if (err) *err = QStringLiteral("找不到 Catalog 对象");
        return false;
    }
    QByteArray catBody = readObjectBody(data, catOff);

    QRegularExpression rxPages(R"(/Pages\s+(\d+)\s+\d+\s+R)");
    auto m = rxPages.match(QString::fromLatin1(catBody));
    if (!m.hasMatch()) {
        if (err) *err = QStringLiteral("Catalog 没有 /Pages");
        return false;
    }
    int pagesId = m.captured(1).toInt();

    QList<int> pageIds;
    flattenPages(data, xref, pagesId, pageIds);
    if (pageIds.isEmpty()) {
        if (err) *err = QStringLiteral("解析 Pages 失败");
        return false;
    }

    int maxId = 0;
    for (auto it = xref.constBegin(); it != xref.constEnd(); ++it) {
        if (it.key() > maxId) maxId = it.key();
    }

    int newCatalogId = maxId + 1;
    int outlineRootId = maxId + 2;
    int nextItemId = maxId + 3;

    // 建扁平节点表
    QList<Flat> flat;
    QList<int> topIds;
    assignIds(items, outlineRootId, 0, flat, nextItemId, topIds);

    // 计算 totalDescendants（每个节点 + 根）
    int rootTotalDescendants = 0;
    for (int i = 0; i < flat.size(); ++i) {
        if (flat[i].level == 0) {
            rootTotalDescendants += 1 + computeDescendants(flat, i);
        }
    }

    // 追加增量更新到 PDF 末尾
    QByteArray output = data;
    if (!output.endsWith('\n')) output.append('\n');

    QHash<int, qint64> newOffsets;
    auto appendObject = [&](int id, const QByteArray& body) {
        newOffsets[id] = output.size();
        output.append(QByteArray::number(id) + " 0 obj\n");
        output.append(body);
        output.append("\nendobj\n");
    };

    // 每个 outline item
    for (const auto& nd : flat) {
        int p = qBound(0, nd.page0, pageIds.size() - 1);
        QByteArray pageRef = QByteArray::number(pageIds.at(p)) + " 0 R";

        QByteArray body = "<< /Title ";
        body.append(pdfHexString(nd.title));
        body.append(" /Parent " + QByteArray::number(nd.parentId) + " 0 R");
        if (nd.prevId)        body.append(" /Prev "  + QByteArray::number(nd.prevId) + " 0 R");
        if (nd.nextId)        body.append(" /Next "  + QByteArray::number(nd.nextId) + " 0 R");
        if (nd.firstChildId)  body.append(" /First " + QByteArray::number(nd.firstChildId) + " 0 R");
        if (nd.lastChildId)   body.append(" /Last "  + QByteArray::number(nd.lastChildId) + " 0 R");
        if (nd.directChildren > 0) {
            // 正数 = 默认展开
            body.append(" /Count " + QByteArray::number(nd.totalDescendants));
        }
        body.append(" /Dest [" + pageRef + " /Fit]");
        body.append(" >>");
        appendObject(nd.id, body);
    }

    // outline 根
    {
        int firstId = 0, lastId = 0;
        for (const auto& nd : flat) {
            if (nd.level == 0) {
                if (firstId == 0) firstId = nd.id;
                lastId = nd.id;
            }
        }
        QByteArray body = "<< /Type /Outlines";
        if (firstId) body.append(" /First " + QByteArray::number(firstId) + " 0 R");
        if (lastId)  body.append(" /Last "  + QByteArray::number(lastId)  + " 0 R");
        body.append(" /Count " + QByteArray::number(rootTotalDescendants));
        body.append(" >>");
        appendObject(outlineRootId, body);
    }

    // 新 Catalog：复制旧 Catalog 内容并追加 /Outlines
    {
        QString s = QString::fromLatin1(catBody);
        int closeIdx = s.lastIndexOf(QLatin1String(">>"));
        if (closeIdx >= 0) {
            s.insert(closeIdx, QString(" /Outlines %1 0 R /PageMode /UseOutlines ").arg(outlineRootId));
        }
        appendObject(newCatalogId, s.toLatin1());
    }

    // 新 xref
    qint64 newXrefOffset = output.size();
    output.append("xref\n");

    QList<int> newIds = newOffsets.keys();
    std::sort(newIds.begin(), newIds.end());

    int i = 0;
    while (i < newIds.size()) {
        int first = newIds.at(i);
        int count = 1;
        while (i + count < newIds.size() && newIds.at(i + count) == first + count) ++count;
        output.append(QByteArray::number(first) + " " + QByteArray::number(count) + "\n");
        for (int j = 0; j < count; ++j) {
            QByteArray off = QByteArray::number(newOffsets.value(first + j))
                                 .rightJustified(10, '0');
            output.append(off + " 00000 n \n");
        }
        i += count;
    }

    // 新 trailer，/Prev 指向旧 xref
    int newSize = nextItemId;  // 上一个分配 + 1 = nextItemId
    output.append("trailer\n");
    output.append(QString("<< /Size %1 /Root %2 0 R /Prev %3 >>\n")
                      .arg(newSize).arg(newCatalogId).arg(oldStartxref).toLatin1());
    output.append("startxref\n");
    output.append(QByteArray::number(newXrefOffset) + "\n");
    output.append("%%EOF\n");

    QFile out(pdfPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("无法写入 PDF");
        return false;
    }
    out.write(output);
    out.close();
    return true;
}

} // namespace pdfoutline
