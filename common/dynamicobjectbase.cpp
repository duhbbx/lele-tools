#include "dynamicobjectbase.h"


DynamicObjectFactory::DynamicObjectFactory() {

}


DynamicObjectFactory::~DynamicObjectFactory() {

}


DynamicObjectFactory* DynamicObjectFactory::Instance() {

    static DynamicObjectFactory Ins;

    return &Ins;
}


/**
 * 根据对象的名字创建对象的实例
 * @brief DynamicObjectFactory::CreateObject
 * @param ObjectName 对象的名字
 * @return 对象的实例
 */
DynamicObjectBase* DynamicObjectFactory::CreateObject(std::string ObjectName) {

    if (mCreateDynamicObjectFuncMap.find(ObjectName) != mCreateDynamicObjectFuncMap.end()) {

        DynamicCreateObject Func = mCreateDynamicObjectFuncMap[ObjectName];

        return Func();
    }

    return nullptr;
}

/**
 * 注册对象
 * @brief DynamicObjectFactory::Register
 * @param ObjectName 对象的名字
 * @param Func 创建对象实例的方法
 */
void DynamicObjectFactory::Register(std::string ObjectName, DynamicCreateObject Func) {

    if (mCreateDynamicObjectFuncMap.find(ObjectName) == mCreateDynamicObjectFuncMap.end()) {
        mCreateDynamicObjectFuncMap[ObjectName] = Func;
    }
}

/**
 * 取消注册
 * @brief DynamicObjectFactory::UnRegister
 * @param ObjectName 对象的名字
 */
void DynamicObjectFactory::UnRegister(std::string ObjectName) {
    if (mCreateDynamicObjectFuncMap.find(ObjectName) != mCreateDynamicObjectFuncMap.end()) {
        mCreateDynamicObjectFuncMap.erase(ObjectName);
    }
}
