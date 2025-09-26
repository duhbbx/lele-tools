#ifndef GLOBAL_STYLES_H
#define GLOBAL_STYLES_H

#include <QString>

class GlobalStyles
{
public:
    // 获取全局CheckBox样式
    static QString getCheckBoxStyle();
    
    // 获取完整的全局样式
    static QString getGlobalStyle();
    
    // 获取按钮样式
    static QString getButtonStyle();
    
    // 获取输入框样式
    static QString getInputStyle();

    // 获取滚动条的样式
    static QString getScrollBarStyle();
};

#endif // GLOBAL_STYLES_H
