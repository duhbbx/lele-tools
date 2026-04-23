#pragma once
#include <QWidget>
#include <QFont>
#include <QTimer>
#include <QKeyEvent>
#include <QPainter>
#include <QFontMetricsF>
#include <QResizeEvent>
#include <QInputMethodEvent>
#include <QPixmap>

#include "terminalemulator.h"

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);

    void setEmulator(TerminalEmulator* emu);
    TerminalEmulator* emulator() const { return m_emu; }

    // Calculate grid dimensions from widget size
    QSize gridSize() const; // returns cols, rows

signals:
    void keyPressed(const QByteArray& data);
    void sizeChanged(int cols, int rows);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void updateCellSize();

    TerminalEmulator* m_emu = nullptr;
    QFont m_font;
    qreal m_cellWidth = 0;
    qreal m_cellHeight = 0;
    qreal m_baselineOffset = 0;
    bool m_cursorVisible = true;
    QTimer* m_cursorTimer;
    QPixmap m_backBuffer;
    bool m_needsFullRedraw = true;
};
