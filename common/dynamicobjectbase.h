#ifndef DYNAMICOBJECTBASE_H
#define DYNAMICOBJECTBASE_H


#include <string>
#include <map>

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

    DynamicObjectBase() {}

    virtual ~DynamicObjectBase(){ }

    virtual void Initialise(){}

    virtual void Release(){}

protected:
private:
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
