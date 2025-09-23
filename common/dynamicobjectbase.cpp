#include "dynamicobjectbase.h"
#include <QUuid>
#include <QMetaObject>
#include <typeinfo>


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

// ============= DynamicObjectBase 使用跟踪相关方法实现 =============

void DynamicObjectBase::recordToolUsage(ToolUsageTracker::ActionType actionType)
{
    QString toolName = getToolName();
    QString displayName = getToolDisplayName();

    // 如果是第一次记录，生成会话ID
    if (m_sessionId.isEmpty()) {
        m_sessionId = QUuid::createUuid().toString();
    }

    ToolUsageTracker::instance()->recordUsage(
        toolName,
        displayName,
        actionType,
        m_sessionId
    );
}

void DynamicObjectBase::recordToolAction()
{
    recordToolUsage(ToolUsageTracker::Action);
}

void DynamicObjectBase::setToolDisplayName(const QString& displayName)
{
    m_toolDisplayName = displayName;
}

QString DynamicObjectBase::getToolDisplayName() const
{
    if (!m_toolDisplayName.isEmpty()) {
        return m_toolDisplayName;
    }
    return getToolName();
}

QString DynamicObjectBase::getToolName() const
{
    // 默认使用类名作为工具名称
    // 子类可以重写这个方法来提供自定义名称
    const char* className = typeid(*this).name();
    QString name = QString::fromUtf8(className);

    // 移除可能的编译器前缀（如 "6", "class " 等）
    if (name.contains("class ")) {
        name = name.split("class ").last();
    }
    // 去掉数字前缀（一些编译器会添加）
    if (!name.isEmpty() && name.at(0).isDigit()) {
        int i = 0;
        while (i < name.length() && name.at(i).isDigit()) {
            i++;
        }
        name = name.mid(i);
    }

    return name;
}
