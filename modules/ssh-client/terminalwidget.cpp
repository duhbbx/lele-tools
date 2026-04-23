#include "terminalwidget.h"
#include <QPainter>
#include <QFontDatabase>
#include <QApplication>
#include <QClipboard>

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Choose a monospace font
#ifdef Q_OS_MAC
    m_font = QFont("Menlo", 12);
#elif defined(Q_OS_WIN)
    m_font = QFont("Consolas", 10);
#else
    m_font = QFont("DejaVu Sans Mono", 10);
#endif
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);

    updateCellSize();

    // Cursor blink timer
    m_cursorTimer = new QTimer(this);
    m_cursorTimer->setInterval(530);
    connect(m_cursorTimer, &QTimer::timeout, this, [this]() {
        m_cursorVisible = !m_cursorVisible;
        update();
    });
    m_cursorTimer->start();

    // Minimum size
    setMinimumSize(m_cellWidth * 40, m_cellHeight * 10);

    // Dark background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    setPalette(pal);
    setAutoFillBackground(true);
}

void TerminalWidget::setEmulator(TerminalEmulator* emu)
{
    m_emu = emu;
    m_needsFullRedraw = true;
    update();
}

QSize TerminalWidget::gridSize() const
{
    if (m_cellWidth <= 0 || m_cellHeight <= 0)
        return QSize(80, 24);
    int cols = qMax(1, static_cast<int>(width() / m_cellWidth));
    int rows = qMax(1, static_cast<int>(height() / m_cellHeight));
    return QSize(cols, rows);
}

void TerminalWidget::updateCellSize()
{
    QFontMetricsF fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_baselineOffset = fm.ascent();
}

void TerminalWidget::paintEvent(QPaintEvent* /*event*/)
{
    if (!m_emu)
        return;

    // Double-buffer: paint to pixmap then blit
    if (m_backBuffer.size() != size() * devicePixelRatioF()) {
        m_backBuffer = QPixmap(size() * devicePixelRatioF());
        m_backBuffer.setDevicePixelRatio(devicePixelRatioF());
        m_needsFullRedraw = true;
    }

    QPainter bp(&m_backBuffer);
    bp.setFont(m_font);
    bp.setRenderHint(QPainter::TextAntialiasing, true);

    int rows = m_emu->rows();
    int cols = m_emu->cols();

    // Clear background
    bp.fillRect(rect(), QColor(0x1e, 0x1e, 0x1e));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const TermCell& cell = m_emu->cell(r, c);

            // 跳过宽字符的尾部单元格（已在前一列绘制）
            if (cell.wideTrail) continue;

            qreal x = c * m_cellWidth;
            qreal y = r * m_cellHeight;
            int cellSpan = cell.wide ? 2 : 1; // 宽字符占 2 列
            QRectF cellRect(x, y, m_cellWidth * cellSpan, m_cellHeight);

            QColor fg = cell.fg;
            QColor bg = cell.bg;

            if (cell.inverse) {
                qSwap(fg, bg);
            }

            // Draw background
            if (bg != QColor(0x1e, 0x1e, 0x1e)) {
                bp.fillRect(cellRect, bg);
            }

            // Draw cursor
            bool isCursor = (r == m_emu->cursorRow() && c == m_emu->cursorCol());
            if (isCursor && m_cursorVisible && hasFocus()) {
                bp.fillRect(cellRect, QColor(0xd4, 0xd4, 0xd4));
                fg = QColor(0x1e, 0x1e, 0x1e);
            }

            // Draw character
            if (cell.ch != ' ' && !cell.ch.isNull()) {
                QFont drawFont = m_font;
                if (cell.bold)
                    drawFont.setBold(true);
                bp.setFont(drawFont);
                bp.setPen(fg);
                bp.drawText(QPointF(x, y + m_baselineOffset), QString(cell.ch));
                if (cell.bold)
                    bp.setFont(m_font);
            }

            // Draw underline
            if (cell.underline) {
                bp.setPen(fg);
                qreal uy = y + m_cellHeight - 1;
                bp.drawLine(QPointF(x, uy), QPointF(x + m_cellWidth * cellSpan, uy));
            }
        }
    }

    // Draw cursor outline when not focused
    if (!hasFocus() && m_emu) {
        int cr = m_emu->cursorRow();
        int cc = m_emu->cursorCol();
        if (cr >= 0 && cr < rows && cc >= 0 && cc < cols) {
            QRectF cursorRect(cc * m_cellWidth, cr * m_cellHeight, m_cellWidth, m_cellHeight);
            bp.setPen(QColor(0xd4, 0xd4, 0xd4));
            bp.setBrush(Qt::NoBrush);
            bp.drawRect(cursorRect.adjusted(0.5, 0.5, -0.5, -0.5));
        }
    }

    bp.end();

    // Blit to widget
    QPainter p(this);
    p.drawPixmap(0, 0, m_backBuffer);
}

void TerminalWidget::keyPressEvent(QKeyEvent* event)
{
    QByteArray data;

    // Handle Ctrl+Shift+C / Ctrl+Shift+V for copy/paste
    if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_V) {
            QString text = QApplication::clipboard()->text();
            if (!text.isEmpty()) {
                emit keyPressed(text.toUtf8());
            }
            return;
        }
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        data = "\r";
    } else if (event->key() == Qt::Key_Backspace) {
        data = "\x08"; // BS (more compatible than DEL 0x7f)
    } else if (event->key() == Qt::Key_Delete) {
        data = "\x1b[3~";
    } else if (event->key() == Qt::Key_Tab) {
        data = "\t";
    } else if (event->key() == Qt::Key_Escape) {
        data = "\x1b";
    } else if (event->key() == Qt::Key_Up) {
        data = "\x1b[A";
    } else if (event->key() == Qt::Key_Down) {
        data = "\x1b[B";
    } else if (event->key() == Qt::Key_Right) {
        data = "\x1b[C";
    } else if (event->key() == Qt::Key_Left) {
        data = "\x1b[D";
    } else if (event->key() == Qt::Key_Home) {
        data = "\x1b[H";
    } else if (event->key() == Qt::Key_End) {
        data = "\x1b[F";
    } else if (event->key() == Qt::Key_PageUp) {
        data = "\x1b[5~";
    } else if (event->key() == Qt::Key_PageDown) {
        data = "\x1b[6~";
    } else if (event->key() == Qt::Key_Insert) {
        data = "\x1b[2~";
    } else if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
        // F1-F12
        static const char* fkeys[] = {
            "\x1bOP", "\x1bOQ", "\x1bOR", "\x1bOS",
            "\x1b[15~", "\x1b[17~", "\x1b[18~", "\x1b[19~",
            "\x1b[20~", "\x1b[21~", "\x1b[23~", "\x1b[24~"
        };
        int idx = event->key() - Qt::Key_F1;
        if (idx >= 0 && idx < 12)
            data = QByteArray(fkeys[idx]);
    } else if (event->modifiers() & Qt::ControlModifier) {
        int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            char c = key - Qt::Key_A + 1;
            data = QByteArray(1, c);
        } else if (key == Qt::Key_BracketLeft) {
            data = "\x1b";
        } else if (key == Qt::Key_Backslash) {
            data = "\x1c";
        } else if (key == Qt::Key_BracketRight) {
            data = "\x1d";
        }
    } else if (event->modifiers() & Qt::AltModifier) {
        QString text = event->text();
        if (!text.isEmpty()) {
            data = "\x1b";
            data.append(text.toUtf8());
        }
    } else {
        QString text = event->text();
        if (!text.isEmpty()) {
            data = text.toUtf8();
        }
    }

    if (!data.isEmpty()) {
        emit keyPressed(data);
        // Reset cursor blink on keypress
        m_cursorVisible = true;
        m_cursorTimer->start();
    }
}

void TerminalWidget::resizeEvent(QResizeEvent* /*event*/)
{
    QSize gs = gridSize();
    m_needsFullRedraw = true;
    emit sizeChanged(gs.width(), gs.height());
}

void TerminalWidget::focusInEvent(QFocusEvent* event)
{
    m_cursorVisible = true;
    m_cursorTimer->start();
    QWidget::focusInEvent(event);
    update();
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent* event)
{
    QString commitString = event->commitString();
    if (!commitString.isEmpty()) {
        emit keyPressed(commitString.toUtf8());
    }
    event->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (!m_emu)
        return QWidget::inputMethodQuery(query);

    switch (query) {
    case Qt::ImCursorRectangle: {
        qreal x = m_emu->cursorCol() * m_cellWidth;
        qreal y = m_emu->cursorRow() * m_cellHeight;
        return QRectF(x, y, m_cellWidth, m_cellHeight).toRect();
    }
    default:
        return QWidget::inputMethodQuery(query);
    }
}
