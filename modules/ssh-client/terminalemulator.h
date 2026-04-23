#pragma once
#include <QObject>
#include <QColor>
#include <QVector>
#include <QString>

struct TermCell {
    QChar ch = ' ';
    QColor fg = QColor(0xd4, 0xd4, 0xd4); // light gray
    QColor bg = QColor(0x1e, 0x1e, 0x1e); // dark
    bool bold = false;
    bool underline = false;
    bool inverse = false;
};

class TerminalEmulator : public QObject {
    Q_OBJECT
public:
    explicit TerminalEmulator(int cols = 80, int rows = 24, QObject* parent = nullptr);

    // Process incoming data from SSH
    void processData(const QByteArray& data);

    // Grid access
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    const TermCell& cell(int row, int col) const;
    int cursorRow() const { return m_cursorRow; }
    int cursorCol() const { return m_cursorCol; }

    // Resize
    void resize(int cols, int rows);

    // Reset
    void reset();

signals:
    void screenChanged(); // Grid content changed, widget should repaint
    void titleChanged(const QString& title); // xterm title change

private:
    // State machine
    enum State { Normal, Escape, CSI, OSC, EscapeCharset };
    void processChar(char c);
    void processNormalChar(char c);
    void processEscape(char c);
    void processCSI(char c);
    void processOSC(char c);

    // CSI command handlers
    void csiDispatch(char cmd);
    void sgr(); // Select Graphic Rendition (colors)

    // Grid operations
    void putChar(QChar ch);
    void newline();
    void carriageReturn();
    void backspace();
    void tab();
    void eraseDisplay(int mode); // 0=below, 1=above, 2=all
    void eraseLine(int mode);
    void insertLines(int n);
    void deleteLines(int n);
    void deleteChars(int n);
    void insertChars(int n);
    void eraseChars(int n);
    void scrollUp(int n = 1);
    void scrollDown(int n = 1);
    void setCursorPos(int row, int col);
    void moveCursor(int drow, int dcol);
    void saveCursor();
    void restoreCursor();

    // ANSI color helpers
    QColor ansiColor(int code) const;

    // Parse CSI parameter list
    QVector<int> parseParams(int defaultVal = 0) const;

    // Grid
    int m_cols, m_rows;
    QVector<QVector<TermCell>> m_grid;
    int m_cursorRow = 0, m_cursorCol = 0;
    int m_savedRow = 0, m_savedCol = 0;

    // Current text attributes
    TermCell m_currentAttr;

    // Parser state
    State m_state = Normal;
    QByteArray m_escBuffer; // accumulate CSI parameters
    QString m_oscBuffer;

    // Scroll region
    int m_scrollTop = 0;
    int m_scrollBottom; // = m_rows - 1

    // Flags
    bool m_alternateScreen = false;
    QVector<QVector<TermCell>> m_savedGrid; // for alternate screen
};
