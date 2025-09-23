# 工具使用跟踪系统

## 概述

本系统为 lele-tools 应用程序提供了完整的工具使用跟踪功能，可以记录每个工具的使用情况，并根据最近一周的使用频率对工具列表进行智能排序。

## 数据库表结构

### tool_usage_records 表
- `id`: 主键，自增
- `tool_name`: 工具名称（类名）
- `tool_display_name`: 工具显示名称
- `usage_timestamp`: 使用时间戳
- `session_id`: 会话ID（用于区分不同的使用会话）
- `duration_seconds`: 使用时长（秒）
- `action_type`: 操作类型（open, close, action）
- `created_at`: 创建时间

### 统计视图
- `tool_usage_stats_week`: 最近7天使用统计
- `tool_usage_stats_all`: 所有时间使用统计

## 核心组件

### 1. ToolUsageTracker（工具使用跟踪器）
- 单例模式，线程安全
- 自动初始化SQLite数据库
- 提供使用记录和统计查询功能

### 2. DynamicObjectBase（动态对象基类扩展）
- 添加了使用跟踪相关方法
- 所有工具类都继承自此基类，可直接使用跟踪功能

### 3. ToolList（工具列表排序）
- 根据最近一周使用频率自动排序工具
- 显示使用次数（如有）

## 使用方法

### 对于工具开发者

#### 1. 基本用法（推荐）
在工具类中需要记录使用的地方调用：

```cpp
// 记录工具打开
this->recordToolUsage(ToolUsageTracker::Open);

// 记录用户操作（简化方法）
this->recordToolAction();

// 记录工具关闭
this->recordToolUsage(ToolUsageTracker::Close);
```

#### 2. 设置自定义显示名称
```cpp
// 在构造函数中设置
this->setToolDisplayName("JSON格式化工具");
```

#### 3. 重写工具名称获取方法
```cpp
QString getToolName() const override {
    return "CustomToolName";
}
```

### 典型使用场景

#### 在工具初始化时记录打开
```cpp
MyTool::MyTool() {
    setupUI();
    this->setToolDisplayName("我的工具");
    this->recordToolUsage(ToolUsageTracker::Open);
}
```

#### 在用户执行操作时记录
```cpp
void MyTool::onButtonClicked() {
    // 执行实际操作
    doSomething();

    // 记录用户操作
    this->recordToolAction();
}
```

#### 在工具关闭时记录
```cpp
MyTool::~MyTool() {
    this->recordToolUsage(ToolUsageTracker::Close);
}
```

## 工具列表排序逻辑

1. **按使用频率排序**：最近7天使用次数多的工具排在前面
2. **相同使用次数**：按工具名称字母序排序
3. **显示使用次数**：在工具标题后显示使用次数（如：`JSON格式化 (5次)`）

## 数据管理

### 自动清理
系统会自动清理90天前的使用记录，保持数据库大小合理。

### 数据库集成
- **共用现有数据库**：工具使用跟踪使用项目现有的 SqliteManager 系统
- **数据库位置**：与其他模块共享同一个数据库文件
- **表名**：`tool_usage_records`（在现有数据库中新增表）

## API 参考

### ToolUsageTracker 主要方法

```cpp
// 记录使用
void recordUsage(const QString& toolName,
                const QString& toolDisplayName = QString(),
                ActionType actionType = Open,
                const QString& sessionId = QString(),
                int durationSeconds = 0);

// 获取最近一周统计
QList<ToolUsageStats> getWeeklyUsageStats();

// 获取工具使用次数
int getWeeklyUsageCount(const QString& toolName);

// 清理旧记录
void cleanupOldRecords();
```

### DynamicObjectBase 新增方法

```cpp
// 记录工具使用
void recordToolUsage(ToolUsageTracker::ActionType actionType = ToolUsageTracker::Open);

// 记录操作（简化方法）
void recordToolAction();

// 设置/获取显示名称
void setToolDisplayName(const QString& displayName);
QString getToolDisplayName() const;

// 获取工具名称（可重写）
virtual QString getToolName() const;
```

## 注意事项

1. **线程安全**：ToolUsageTracker 是线程安全的
2. **性能影响**：使用跟踪对性能影响极小
3. **数据隐私**：所有数据都存储在本地，不会上传
4. **向后兼容**：现有工具无需修改即可工作，跟踪功能是可选的

## 示例实现

参考 `modules/telnet/telnet.cpp` 等文件查看实际使用示例。