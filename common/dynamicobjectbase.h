#ifndef DYNAMICOBJECTBASE_H
#define DYNAMICOBJECTBASE_H

#include <string>
#include <map>
#include <QString>
#include "toolusagetracker.h"

#define REGISTER_DYNAMICOBJECT(name)                                        \
DynamicObjectBase* _stdcall Create_##name() {                               \
    return new name;                                                        \
}											                                \
struct Register##name {											            \
    Register##name() {										                \
        DynamicObjectFactory::Instance()->Register(#name, Create_##name);	\
    }										                                \
} Register_##name

class DynamicObjectBase
{
public:

    DynamicObjectBase() = default;

    virtual ~DynamicObjectBase()= default;

    virtual void Initialise(){}

    virtual void Release(){}

    // 工具使用跟踪相关方法
    void recordToolUsage(ToolUsageTracker::ActionType actionType = ToolUsageTracker::Open);
    void recordToolAction(); // 简化的操作记录方法，子类可以直接调用

    // 设置工具显示名称（可选，默认使用类名）
    void setToolDisplayName(const QString& displayName);
    QString getToolDisplayName() const;

protected:
    virtual QString getToolName() const; // 子类可以重写来提供自定义工具名称

private:
    QString m_toolDisplayName;
    QString m_sessionId;
};


class DynamicObjectFactory
{
    typedef DynamicObjectBase*(_stdcall* DynamicCreateObject)();

public:
    static DynamicObjectFactory* Instance();

    DynamicObjectBase* CreateObject(std::string ObjectName);

    void Register(std::string ObjectName, DynamicCreateObject Func);

    void UnRegister(std::string ObjectName);
protected:
    DynamicObjectFactory();
    ~DynamicObjectFactory();

    std::map<std::string, DynamicCreateObject> mCreateDynamicObjectFuncMap;
};


#endif // DYNAMICOBJECTBASE_H
