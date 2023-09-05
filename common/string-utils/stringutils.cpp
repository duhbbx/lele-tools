#include "stringutils.h"


QString camelCaseToHyphen(const QString &input) {
    QString result;
    for (int i = 0; i < input.length(); ++i) {
        QChar ch = input.at(i);

        // 将大写字母转换为小写并在前面添加中划线
        if (ch.isUpper()) {
            if (i > 0) {
                result.append("-");
                result.append(ch.toLower());
            } else {
                result.append(ch.toLower());
            }

        } else {
            result.append(ch);
        }
    }
    return result;
}
