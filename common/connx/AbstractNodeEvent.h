//
// Created by duhbb on 2025/9/17.
//

#ifndef LELE_TOOLS_ABSTRACTNODEEVENT_H
#define LELE_TOOLS_ABSTRACTNODEEVENT_H


/**
 *所以你需要帮我建枚举，以及优化目前的这个抽喜类，以及实现 redis 和 mysql 的 ExpandEvent 子类，来完成列表的展开
 *
 * > 我想设计一个抽象的 AbstractNodeEvent, 这个地方能接受 connection id, connect, 等连接信息，构造函数根据情况直接拿现成连接，
 * 还是打开新的数据库连接；然后会传入节点类型，节点类型是一个枚举，不用字符串，然后根据节点类型转发事件，会有一个泛型指定返回结果
 * 这里不涉及任何UI，只返回对应的数据
 * T 可以是字符串，也可以是里列表
 *
 * Event 可以是expand, delete, generate sql, 等等，这样就可以做一层抽象了，
 * 然后传入的参数就是所选中节点一直都溯源到连接节点的链表， 最后一层可以是多个或者的单个，对应的界面的选中单个或者多个，
 * 这里不要穿 Qt 相关的ui类型，而是抽出一层 NodeChain, 每个Node 记录 id，qt 元素的id，用于定位，如果可以的话；节点类型，节点名称，等等
 */
typename T
class AbstractNodeEvent {


public:
    /**
     * 构造函数能打开来接
     */
    AbstractNodeEvent();


    T connection() {

    }

    T table() {

    }

    T tableDir() {

    }

    T xxxDir() {

    }


    void execute() {
        switch (NodeType) {

        case TABLE_DIR:
            tableDir();
        case TABLE_OBJ:
        }
    }



    /**
     * 析构的地方关闭连接
     */
    virtual ~AbstractNodeEvent();

private :
    NodeTypeEnum nodeType;


};

#endif //LELE_TOOLS_ABSTRACTNODEEVENT_H