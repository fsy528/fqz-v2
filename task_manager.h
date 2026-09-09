// 任务管理头文件（负责人：王嘉明）
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

// Task 类前置声明
class Task;

class TaskManager {
private:
    std::vector<Task> tasks;
    int nextId = 1;

    // 辅助函数
    std::string priorityToString(int p) const;
    std::string getStatusSymbol(bool isCompleted) const;

public:
    // 构造函数
    TaskManager() = default;

    // 核心功能 ：添加、删除、更新任务
    bool addTask(const Task& task);
    bool deleteTask(int id);
    bool updateTask(int id, const Task& newTask);

    // 查询与显示
    bool hasTasks() const;
    int getTaskCount() const;
    void showAllTasks() const;
    void showUncompletedTasks() const;
    void showCompletedTasks() const;

    // 任务操作
    bool markTaskCompleted(int id);
    bool markTaskUncompleted(int id);
    Task* findTaskById(int id);
    const std::vector<Task>& getTasks() const;
    int getNextId() const;

    // 文件操作
    void saveToFile(const std::string& filename = "tasks.txt") const;
    void loadFromFile(const std::string& filename = "tasks.txt");
};

// ---------- Task 类定义 ----------
class Task {
public:
    // 枚举类
    enum class Priority { LOW, MEDIUM, HIGH };
    enum class TaskStatus { PENDING, IN_PROGRESS, COMPLETED };

private:
    int id;
    std::string name;
    std::string description;
    Priority priority;
    std::chrono::system_clock::time_point deadline;
    int estimatedMinutes;
    TaskStatus status;
    bool isCompleted;

public:
    // 构造函数
    Task();
    Task(const std::string& name,
        const std::string& desc = "",
        Priority prio = Priority::MEDIUM,
        int estMinutes = 25);

    // 获取器
    int getId() const;
    std::string getName() const;
    std::string getDescription() const;
    Priority getPriority() const;
    std::chrono::system_clock::time_point getDeadline() const;
    int getEstimatedMinutes() const;
    TaskStatus getStatus() const;
    bool getIsCompleted() const;

    // 设置器
    void setId(int newId);
    void setName(const std::string& newName);
    void setDescription(const std::string& desc);
    void setPriority(Priority prio);
    void setDeadline(std::chrono::system_clock::time_point dl);
    void setEstimatedMinutes(int minutes);
    void setStatus(TaskStatus s);
    void setIsCompleted(bool completed);

    // 状态切换
    void toggleComplete();
    void display() const;
    void displayBrief() const;

    // 静态解析函数
    static Priority parsePriority(const std::string& input);
    static int parseEstimatedTime(const std::string& input);
    static Task parseInput(const std::string& input);

    // 设置截止日期
    void setDeadlineFromString(const std::string& dateStr);
};

#endif 