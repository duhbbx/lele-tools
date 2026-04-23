#include "terminalemulator.h"

TerminalEmulator::TerminalEmulator(int cols, int rows, QObject* parent)
    : QObject(parent)
    , m_cols(cols)
    , m_rows(rows)
    , m_scrollBottom(rows - 1)
{
    m_grid.resize(m_rows);
    for (int r = 0; r < m_rows; ++r)
        m_grid[r].resize(m_cols);
}

void TerminalEmulator::processData(const QByteArray& data)
{
    for (int i = 0; i < data.size(); ++i)
        processChar(data.at(i));
    emit screenChanged();
}

const TermCell& TerminalEmulator::cell(int row, int col) const
{
    static TermCell blank;
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
        return blank;
    return m_grid[row][col];
}

void TerminalEmulator::resize(int cols, int rows)
{
    if (cols == m_cols && rows == m_rows)
        return;

    QVector<QVector<TermCell>> newGrid(rows);
    for (int r = 0; r < rows; ++r) {
        newGrid[r].resize(cols);
        if (r < m_rows) {
            for (int c = 0; c < qMin(cols, m_cols); ++c)
                newGrid[r][c] = m_grid[r][c];
        }
    }

    m_grid = newGrid;
    m_cols = cols;
    m_rows = rows;
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;

    if (m_cursorRow >= m_rows) m_cursorRow = m_rows - 1;
    if (m_cursorCol >= m_cols) m_cursorCol = m_cols - 1;

    emit screenChanged();
}

void TerminalEmulator::reset()
{
    m_state = Normal;
    m_escBuffer.clear();
    m_oscBuffer.clear();
    m_cursorRow = 0;
    m_cursorCol = 0;
    m_savedRow = 0;
    m_savedCol = 0;
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
    m_currentAttr = TermCell();
    m_alternateScreen = false;
    m_savedGrid.clear();

    for (int r = 0; r < m_rows; ++r)
        for (int c = 0; c < m_cols; ++c)
            m_grid[r][c] = TermCell();

    emit screenChanged();
}

// ---- State machine ----

void TerminalEmulator::processChar(char c)
{
    unsigned char uc = static_cast<unsigned char>(c);

    switch (m_state) {
    case Normal:
        processNormalChar(c);
        break;
    case Escape:
        processEscape(c);
        break;
    case CSI:
        processCSI(c);
        break;
    case OSC:
        processOSC(c);
        break;
    case EscapeCharset:
        // Consume one character for charset designation (e.g. ESC ( B)
        m_state = Normal;
        break;
    }
}

void TerminalEmulator::processNormalChar(char c)
{
    unsigned char uc = static_cast<unsigned char>(c);

    if (uc >= 0x20 && uc != 0x7f) {
        // Printable character (including UTF-8 lead/continuation bytes)
        putChar(QChar::fromLatin1(c));
    } else {
        switch (c) {
        case '\x1b': // ESC
            m_state = Escape;
            m_escBuffer.clear();
            break;
        case '\r':
            carriageReturn();
            break;
        case '\n':
        case '\x0b': // VT
        case '\x0c': // FF
            newline();
            break;
        case '\t':
            tab();
            break;
        case '\b':
            backspace();
            break;
        case '\x07': // BEL
            break;
        default:
            break;
        }
    }
}

void TerminalEmulator::processEscape(char c)
{
    switch (c) {
    case '[':
        m_state = CSI;
        m_escBuffer.clear();
        break;
    case ']':
        m_state = OSC;
        m_oscBuffer.clear();
        break;
    case '(':
    case ')':
    case '*':
    case '+':
        // Charset designation - consume next char
        m_state = EscapeCharset;
        break;
    case '7':
        saveCursor();
        m_state = Normal;
        break;
    case '8':
        restoreCursor();
        m_state = Normal;
        break;
    case 'M': // Reverse index
        if (m_cursorRow == m_scrollTop)
            scrollDown(1);
        else if (m_cursorRow > 0)
            m_cursorRow--;
        m_state = Normal;
        break;
    case 'c': // Full reset
        reset();
        m_state = Normal;
        break;
    case 'D': // Index (move down, scroll if needed)
        newline();
        m_state = Normal;
        break;
    case 'E': // Next line
        carriageReturn();
        newline();
        m_state = Normal;
        break;
    case '=': // Application keypad
    case '>': // Normal keypad
        m_state = Normal;
        break;
    default:
        m_state = Normal;
        break;
    }
}

void TerminalEmulator::processCSI(char c)
{
    if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '!' || c == ' ') {
        m_escBuffer.append(c);
        return;
    }
    // c is the final command character
    csiDispatch(c);
    m_state = Normal;
}

void TerminalEmulator::processOSC(char c)
{
    if (c == '\x07') {
        // BEL terminates OSC
        // Parse OSC: "0;title" or "2;title"
        int semi = m_oscBuffer.indexOf(';');
        if (semi >= 0) {
            int code = m_oscBuffer.left(semi).toInt();
            QString payload = m_oscBuffer.mid(semi + 1);
            if (code == 0 || code == 2) {
                emit titleChanged(payload);
            }
        }
        m_state = Normal;
    } else if (c == '\x1b') {
        // Could be ST (ESC \) - handle next char
        // For simplicity, treat as end of OSC
        m_state = Normal;
    } else {
        m_oscBuffer.append(QChar::fromLatin1(c));
    }
}

// ---- CSI dispatch ----

QVector<int> TerminalEmulator::parseParams(int defaultVal) const
{
    QVector<int> params;
    if (m_escBuffer.isEmpty()) {
        params.append(defaultVal);
        return params;
    }

    // Remove leading '?' or '>' if present
    QByteArray buf = m_escBuffer;
    if (!buf.isEmpty() && (buf[0] == '?' || buf[0] == '>'))
        buf = buf.mid(1);

    const auto parts = buf.split(';');
    for (const QByteArray& part : parts) {
        bool ok;
        int val = part.toInt(&ok);
        params.append(ok ? val : defaultVal);
    }
    if (params.isEmpty())
        params.append(defaultVal);
    return params;
}

void TerminalEmulator::csiDispatch(char cmd)
{
    QVector<int> params = parseParams(0);
    bool isPrivate = !m_escBuffer.isEmpty() && m_escBuffer[0] == '?';

    switch (cmd) {
    case 'm': // SGR
        sgr();
        break;

    case 'H': // Cursor position
    case 'f': {
        int row = (params.size() > 0 && params[0] > 0) ? params[0] - 1 : 0;
        int col = (params.size() > 1 && params[1] > 0) ? params[1] - 1 : 0;
        setCursorPos(row, col);
        break;
    }

    case 'A': { // Cursor up
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(-n, 0);
        break;
    }
    case 'B': { // Cursor down
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(n, 0);
        break;
    }
    case 'C': { // Cursor right
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(0, n);
        break;
    }
    case 'D': { // Cursor left
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(0, -n);
        break;
    }

    case 'G': { // Cursor horizontal absolute
        int col = (params[0] > 0) ? params[0] - 1 : 0;
        setCursorPos(m_cursorRow, col);
        break;
    }

    case 'd': { // Cursor vertical absolute
        int row = (params[0] > 0) ? params[0] - 1 : 0;
        setCursorPos(row, m_cursorCol);
        break;
    }

    case 'J': // Erase display
        eraseDisplay(params[0]);
        break;

    case 'K': // Erase line
        eraseLine(params[0]);
        break;

    case 'L': { // Insert lines
        int n = (params[0] > 0) ? params[0] : 1;
        insertLines(n);
        break;
    }

    case 'M': { // Delete lines
        int n = (params[0] > 0) ? params[0] : 1;
        deleteLines(n);
        break;
    }

    case 'P': { // Delete chars
        int n = (params[0] > 0) ? params[0] : 1;
        deleteChars(n);
        break;
    }

    case '@': { // Insert chars
        int n = (params[0] > 0) ? params[0] : 1;
        insertChars(n);
        break;
    }

    case 'X': { // Erase chars
        int n = (params[0] > 0) ? params[0] : 1;
        eraseChars(n);
        break;
    }

    case 'S': { // Scroll up
        int n = (params[0] > 0) ? params[0] : 1;
        scrollUp(n);
        break;
    }
    case 'T': { // Scroll down
        int n = (params[0] > 0) ? params[0] : 1;
        scrollDown(n);
        break;
    }

    case 'r': { // Set scroll region
        int top = (params.size() > 0 && params[0] > 0) ? params[0] - 1 : 0;
        int bot = (params.size() > 1 && params[1] > 0) ? params[1] - 1 : m_rows - 1;
        if (top < bot && top >= 0 && bot < m_rows) {
            m_scrollTop = top;
            m_scrollBottom = bot;
        }
        setCursorPos(0, 0);
        break;
    }

    case 's': // Save cursor
        saveCursor();
        break;
    case 'u': // Restore cursor
        restoreCursor();
        break;

    case 'h': // Set mode
        if (isPrivate) {
            for (int p : params) {
                if (p == 1049 || p == 47 || p == 1047) {
                    // Switch to alternate screen
                    if (!m_alternateScreen) {
                        m_savedGrid = m_grid;
                        m_alternateScreen = true;
                        eraseDisplay(2);
                    }
                } else if (p == 25) {
                    // Show cursor (handled by widget)
                }
            }
        }
        break;

    case 'l': // Reset mode
        if (isPrivate) {
            for (int p : params) {
                if (p == 1049 || p == 47 || p == 1047) {
                    // Switch back from alternate screen
                    if (m_alternateScreen) {
                        m_grid = m_savedGrid;
                        m_savedGrid.clear();
                        m_alternateScreen = false;
                    }
                } else if (p == 25) {
                    // Hide cursor (handled by widget)
                }
            }
        }
        break;

    case 'E': { // Cursor next line
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(n, 0);
        m_cursorCol = 0;
        break;
    }
    case 'F': { // Cursor previous line
        int n = (params[0] > 0) ? params[0] : 1;
        moveCursor(-n, 0);
        m_cursorCol = 0;
        break;
    }

    case 'n': // Device status report
        // We don't send responses in this simple implementation
        break;

    default:
        break;
    }
}

// ---- SGR (Select Graphic Rendition) ----

void TerminalEmulator::sgr()
{
    QVector<int> params = parseParams(0);

    for (int i = 0; i < params.size(); ++i) {
        int p = params[i];

        if (p == 0) {
            // Reset
            m_currentAttr = TermCell();
        } else if (p == 1) {
            m_currentAttr.bold = true;
        } else if (p == 4) {
            m_currentAttr.underline = true;
        } else if (p == 7) {
            m_currentAttr.inverse = true;
        } else if (p == 22) {
            m_currentAttr.bold = false;
        } else if (p == 24) {
            m_currentAttr.underline = false;
        } else if (p == 27) {
            m_currentAttr.inverse = false;
        } else if (p >= 30 && p <= 37) {
            m_currentAttr.fg = ansiColor(p - 30);
        } else if (p == 38) {
            // Extended foreground
            if (i + 1 < params.size()) {
                if (params[i + 1] == 5 && i + 2 < params.size()) {
                    // 256-color: 38;5;N
                    int idx = params[i + 2];
                    if (idx >= 0 && idx <= 15) {
                        m_currentAttr.fg = ansiColor(idx);
                    } else if (idx >= 16 && idx <= 231) {
                        // 6x6x6 color cube
                        idx -= 16;
                        int r = (idx / 36) * 51;
                        int g = ((idx / 6) % 6) * 51;
                        int b = (idx % 6) * 51;
                        m_currentAttr.fg = QColor(r, g, b);
                    } else if (idx >= 232 && idx <= 255) {
                        int gray = 8 + (idx - 232) * 10;
                        m_currentAttr.fg = QColor(gray, gray, gray);
                    }
                    i += 2;
                } else if (params[i + 1] == 2 && i + 4 < params.size()) {
                    // Truecolor: 38;2;r;g;b
                    m_currentAttr.fg = QColor(params[i + 2], params[i + 3], params[i + 4]);
                    i += 4;
                }
            }
        } else if (p == 39) {
            m_currentAttr.fg = QColor(0xd4, 0xd4, 0xd4); // default fg
        } else if (p >= 40 && p <= 47) {
            m_currentAttr.bg = ansiColor(p - 40);
        } else if (p == 48) {
            // Extended background
            if (i + 1 < params.size()) {
                if (params[i + 1] == 5 && i + 2 < params.size()) {
                    int idx = params[i + 2];
                    if (idx >= 0 && idx <= 15) {
                        m_currentAttr.bg = ansiColor(idx);
                    } else if (idx >= 16 && idx <= 231) {
                        idx -= 16;
                        int r = (idx / 36) * 51;
                        int g = ((idx / 6) % 6) * 51;
                        int b = (idx % 6) * 51;
                        m_currentAttr.bg = QColor(r, g, b);
                    } else if (idx >= 232 && idx <= 255) {
                        int gray = 8 + (idx - 232) * 10;
                        m_currentAttr.bg = QColor(gray, gray, gray);
                    }
                    i += 2;
                } else if (params[i + 1] == 2 && i + 4 < params.size()) {
                    m_currentAttr.bg = QColor(params[i + 2], params[i + 3], params[i + 4]);
                    i += 4;
                }
            }
        } else if (p == 49) {
            m_currentAttr.bg = QColor(0x1e, 0x1e, 0x1e); // default bg
        } else if (p >= 90 && p <= 97) {
            m_currentAttr.fg = ansiColor(p - 90 + 8);
        } else if (p >= 100 && p <= 107) {
            m_currentAttr.bg = ansiColor(p - 100 + 8);
        }
    }
}

QColor TerminalEmulator::ansiColor(int code) const
{
    static const QColor colors[16] = {
        QColor(0x00, 0x00, 0x00), // 0 black
        QColor(0xcd, 0x31, 0x31), // 1 red
        QColor(0x0d, 0xbc, 0x79), // 2 green
        QColor(0xe5, 0xe5, 0x10), // 3 yellow
        QColor(0x24, 0x72, 0xc8), // 4 blue
        QColor(0xbc, 0x3f, 0xbc), // 5 magenta
        QColor(0x11, 0xa8, 0xcd), // 6 cyan
        QColor(0xe5, 0xe5, 0xe5), // 7 white
        QColor(0x66, 0x66, 0x66), // 8 bright black (gray)
        QColor(0xf1, 0x4c, 0x4c), // 9 bright red
        QColor(0x23, 0xd1, 0x8b), // 10 bright green
        QColor(0xf5, 0xf5, 0x43), // 11 bright yellow
        QColor(0x3b, 0x8e, 0xea), // 12 bright blue
        QColor(0xd6, 0x70, 0xd6), // 13 bright magenta
        QColor(0x29, 0xb8, 0xdb), // 14 bright cyan
        QColor(0xff, 0xff, 0xff), // 15 bright white
    };
    if (code >= 0 && code < 16)
        return colors[code];
    return QColor(0xd4, 0xd4, 0xd4);
}

// ---- Grid operations ----

void TerminalEmulator::putChar(QChar ch)
{
    if (m_cursorCol >= m_cols) {
        carriageReturn();
        newline();
    }
    m_grid[m_cursorRow][m_cursorCol] = m_currentAttr;
    m_grid[m_cursorRow][m_cursorCol].ch = ch;
    m_cursorCol++;
}

void TerminalEmulator::newline()
{
    if (m_cursorRow == m_scrollBottom) {
        scrollUp(1);
    } else if (m_cursorRow < m_rows - 1) {
        m_cursorRow++;
    }
}

void TerminalEmulator::carriageReturn()
{
    m_cursorCol = 0;
}

void TerminalEmulator::backspace()
{
    if (m_cursorCol > 0)
        m_cursorCol--;
}

void TerminalEmulator::tab()
{
    int next = ((m_cursorCol / 8) + 1) * 8;
    m_cursorCol = qMin(next, m_cols - 1);
}

void TerminalEmulator::eraseDisplay(int mode)
{
    TermCell blank;
    blank.bg = m_currentAttr.bg;

    if (mode == 0) {
        // Erase below (including current position)
        for (int c = m_cursorCol; c < m_cols; ++c)
            m_grid[m_cursorRow][c] = blank;
        for (int r = m_cursorRow + 1; r < m_rows; ++r)
            for (int c = 0; c < m_cols; ++c)
                m_grid[r][c] = blank;
    } else if (mode == 1) {
        // Erase above (including current position)
        for (int r = 0; r < m_cursorRow; ++r)
            for (int c = 0; c < m_cols; ++c)
                m_grid[r][c] = blank;
        for (int c = 0; c <= m_cursorCol && c < m_cols; ++c)
            m_grid[m_cursorRow][c] = blank;
    } else if (mode == 2 || mode == 3) {
        // Erase all
        for (int r = 0; r < m_rows; ++r)
            for (int c = 0; c < m_cols; ++c)
                m_grid[r][c] = blank;
    }
}

void TerminalEmulator::eraseLine(int mode)
{
    TermCell blank;
    blank.bg = m_currentAttr.bg;

    if (mode == 0) {
        for (int c = m_cursorCol; c < m_cols; ++c)
            m_grid[m_cursorRow][c] = blank;
    } else if (mode == 1) {
        for (int c = 0; c <= m_cursorCol && c < m_cols; ++c)
            m_grid[m_cursorRow][c] = blank;
    } else if (mode == 2) {
        for (int c = 0; c < m_cols; ++c)
            m_grid[m_cursorRow][c] = blank;
    }
}

void TerminalEmulator::insertLines(int n)
{
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom)
        return;

    for (int i = 0; i < n; ++i) {
        if (m_scrollBottom < m_rows)
            m_grid.remove(m_scrollBottom);
        m_grid.insert(m_cursorRow, QVector<TermCell>(m_cols));
    }

    // Ensure grid size is correct
    while (m_grid.size() < m_rows)
        m_grid.append(QVector<TermCell>(m_cols));
    while (m_grid.size() > m_rows)
        m_grid.removeLast();
}

void TerminalEmulator::deleteLines(int n)
{
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom)
        return;

    for (int i = 0; i < n; ++i) {
        if (m_cursorRow < m_grid.size())
            m_grid.remove(m_cursorRow);
        m_grid.insert(m_scrollBottom, QVector<TermCell>(m_cols));
    }

    while (m_grid.size() < m_rows)
        m_grid.append(QVector<TermCell>(m_cols));
    while (m_grid.size() > m_rows)
        m_grid.removeLast();
}

void TerminalEmulator::deleteChars(int n)
{
    auto& row = m_grid[m_cursorRow];
    for (int i = 0; i < n && m_cursorCol + i < m_cols; ++i) {
        row.remove(m_cursorCol);
        row.append(TermCell());
    }
}

void TerminalEmulator::insertChars(int n)
{
    auto& row = m_grid[m_cursorRow];
    for (int i = 0; i < n; ++i) {
        row.insert(m_cursorCol, TermCell());
        if (row.size() > m_cols)
            row.removeLast();
    }
}

void TerminalEmulator::eraseChars(int n)
{
    TermCell blank;
    blank.bg = m_currentAttr.bg;
    for (int i = 0; i < n && m_cursorCol + i < m_cols; ++i)
        m_grid[m_cursorRow][m_cursorCol + i] = blank;
}

void TerminalEmulator::scrollUp(int n)
{
    for (int i = 0; i < n; ++i) {
        m_grid.remove(m_scrollTop);
        m_grid.insert(m_scrollBottom, QVector<TermCell>(m_cols));
    }
}

void TerminalEmulator::scrollDown(int n)
{
    for (int i = 0; i < n; ++i) {
        m_grid.remove(m_scrollBottom);
        m_grid.insert(m_scrollTop, QVector<TermCell>(m_cols));
    }
}

void TerminalEmulator::setCursorPos(int row, int col)
{
    m_cursorRow = qBound(0, row, m_rows - 1);
    m_cursorCol = qBound(0, col, m_cols - 1);
}

void TerminalEmulator::moveCursor(int drow, int dcol)
{
    setCursorPos(m_cursorRow + drow, m_cursorCol + dcol);
}

void TerminalEmulator::saveCursor()
{
    m_savedRow = m_cursorRow;
    m_savedCol = m_cursorCol;
}

void TerminalEmulator::restoreCursor()
{
    m_cursorRow = m_savedRow;
    m_cursorCol = m_savedCol;
}
