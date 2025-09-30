#ifndef CUSTOMTABLEWIDGET_H
#define CUSTOMTABLEWIDGET_H

#include <QTableWidget>

/**
 * @brief 自定义TableWidget，解决header、item和表格边框重叠问题
 *
 * 特点：
 * - Header、Item和表格边框不会重叠
 * - 统一的视觉效果
 * - 支持交替行颜色
 */
class CustomTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit CustomTableWidget(QWidget *parent = nullptr);
    explicit CustomTableWidget(int rows, int columns, QWidget *parent = nullptr);

private:
    void initStyle();
};

#endif // CUSTOMTABLEWIDGET_H