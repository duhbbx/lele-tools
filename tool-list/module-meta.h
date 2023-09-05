#ifndef MOUDULEMETA_H
#define MOUDULEMETA_H

#include <QString>

struct ModuleMeta {
    QString icon;
    QString title;
    QString className;
};


ModuleMeta moduleMetaArray[] = {
#include "module-meta-data.h"
};

#endif // MOUDULEMETA_H
