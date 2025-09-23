#ifndef MOUDULEMETA_H
#define MOUDULEMETA_H

#include <QString>
#include <QObject>

struct ModuleMeta {
    QString icon;
    QString titleKey;       // 翻译键值，而不是直接的标题
    QString className;

    // 获取翻译后的标题
    QString getLocalizedTitle() const {
        return QObject::tr(titleKey.toUtf8().constData());
    }
};


inline ModuleMeta moduleMetaArray[] = {
#include "module-meta-data.h"
};

#endif // MOUDULEMETA_H
