// 任务管理实现源文件（负责人：王嘉明）
#include "task_manager.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>

using namespace std;
using namespace chrono;

// ---------- Task 实现 ------------

// 构造函数
Task::Task()
    : id(0), name("未命名任务"), description(""), priority(Priority::MEDIUM),
    deadline(system_clock::now() + hours(24)), estimatedMinutes(25),
    status(TaskStatus::PENDING), isCompleted(false) {
}

Task::Task(const string& name, const string& desc, Priority prio, int estMinutes)
    : id(0), name(name), description(desc), priority(prio),
    deadline(system_clock::now() + hours(24)),
    estimatedMinutes(estMinutes), status(TaskStatus::PENDING), isCompleted(false) {
}

// 获取器（只读访问）：只读取内部私有数据
int Task::getId() const { return id; }
string Task::getName() const { return name; }
string Task::getDescription() const { return description; }
Task::Priority Task::getPriority() const { return priority; }
chrono::system_clock::time_point Task::getDeadline() const { return deadline; }
int Task::getEstimatedMinutes() const { return estimatedMinutes; }
Task::TaskStatus Task::getStatus() const { return status; }
bool Task::getIsCompleted() const { return isCompleted; }

// 设置器（写入访问）：允许外部修改内部私有数据
void Task::setId(int newId) { id = newId; }
void Task::setName(const string& newName) { name = newName; }
void Task::setDescription(const string& desc) { description = desc; }
void Task::setPriority(Priority prio) { priority = prio; }
void Task::setDeadline(chrono::system_clock::time_point dl) { deadline = dl; }
void Task::setEstimatedMinutes(int minutes) { estimatedMinutes = minutes; }
void Task::setStatus(TaskStatus s) { status = s; }
void Task::setIsCompleted(bool completed) { isCompleted = completed; }

// 切换任务的完成状态
void Task::toggleComplete() {
    isCompleted = !isCompleted;
    status = isCompleted ? TaskStatus::COMPLETED : TaskStatus::PENDING;
}

void Task::display() const {
    // 转换时间
    time_t deadlineTime = chrono::system_clock::to_time_t(deadline);
    tm tmStruct = {};

#ifdef _WIN32
    localtime_s(&tmStruct, &deadlineTime);
#else
    localtime_r(&deadlineTime, &tmStruct);
#endif

    // 优先级字符串，为番茄钟的执行优先级排序
    string prioStr;
    switch (priority) {
    case Priority::LOW: prioStr = "低"; break;
    case Priority::MEDIUM: prioStr = "中"; break;
    case Priority::HIGH: prioStr = "高"; break;
    }

    // 状态字符串，展现任务状态
    string statusStr;
    switch (status) {
    case TaskStatus::PENDING: statusStr = "待办"; break;
    case TaskStatus::IN_PROGRESS: statusStr = "进行中"; break;
    case TaskStatus::COMPLETED: statusStr = "已完成"; break;
    }

    // 显示任务
    cout << (isCompleted ? "[✓] " : "[ ] ") << "ID: " << id << endl;
    cout << "   名称: " << name << endl;
    if (!description.empty()) {
        cout << "   描述: " << description << endl;
    }
    cout << "   优先级: " << prioStr
        << " | 预估: " << estimatedMinutes << "分钟" << endl;
    cout << "   截止: " << put_time(&tmStruct, "%Y-%m-%d %H:%M")
        << " | 状态: " << statusStr << endl;
    cout << "   --------------------" << endl;
}

// 输出格式化
void Task::displayBrief() const {
    cout << "[" << id << "] " << (isCompleted ? "[✓] " : "[ ] ") << name;
    if (estimatedMinutes > 0) {
        cout << " (" << estimatedMinutes << "分钟)";
    }
    cout << endl;
}

// 解析优先级函数
Task::Priority Task::parsePriority(const string& input) {
    string lower = input;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("高") != string::npos ||
        lower.find("important") != string::npos ||
        lower.find("urgent") != string::npos) {
        return Priority::HIGH;
    }
    if (lower.find("低") != string::npos ||
        lower.find("low") != string::npos) {
        return Priority::LOW;
    }
    return Priority::MEDIUM;
}

//解析预计时间函数
int Task::parseEstimatedTime(const string& input) {
    string lower = input;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("半小时") != string::npos ||
        lower.find("30分钟") != string::npos) {
        return 30;
    }
    if (lower.find("1小时") != string::npos ||
        lower.find("60分钟") != string::npos) {
        return 60;
    }
    if (lower.find("2小时") != string::npos) {
        return 120;
    }
    if (lower.find("3小时") != string::npos) {
        return 180;
    }
    if (lower.find("4小时") != string::npos) {
        return 240;
    }
    return 25; // 默认25分钟
}

//解析截止日期函数
void Task::setDeadlineFromString(const string& dateStr) {
    string lower = dateStr;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto now = system_clock::now();

    if (lower.find("今天") != string::npos) {
        deadline = now;
    }
    else if (lower.find("明天") != string::npos) {
        deadline = now + hours(24);
    }
    else if (lower.find("后天") != string::npos) {
        deadline = now + hours(48);
    }
    else if (lower.find("本周") != string::npos || lower.find("这周") != string::npos) {
        deadline = now + hours(24 * 7);
    }
    else if (lower.find("下周") != string::npos) {
        deadline = now + hours(24 * 14);
    }
    else {
        // 默认设置为明天
        deadline = now + hours(24);
    }
}

Task Task::parseInput(const string& input) {
    Task task(input, "");

    // 解析优先级
    task.setPriority(parsePriority(input));

    // 解析预估时间
    task.setEstimatedMinutes(parseEstimatedTime(input));

    // 解析截止日期
    task.setDeadlineFromString(input);

    return task;
}


// ----------- TaskManager 实现 -----------

// 添加任务
bool TaskManager::addTask(const Task& task) {
    Task newTask = task;
    newTask.setId(nextId++);
    tasks.push_back(newTask);
    cout << "✅ 任务添加成功！ID: " << newTask.getId() << endl;
    return true;
}

// 删除任务
bool TaskManager::deleteTask(int id) {
    auto it = find_if(tasks.begin(), tasks.end(),
        [id](const Task& task) { return task.getId() == id; });

    if (it != tasks.end()) {
        tasks.erase(it);
        cout << "🗑️ 任务删除成功！ID: " << id << endl;
        return true;
    }

    cout << "❌ 错误：找不到ID为 " << id << " 的任务。" << endl;
    return false;
}

// 更新任务
bool TaskManager::updateTask(int id, const Task& newTask) {
    Task* task = findTaskById(id);
    if (task) {
        *task = newTask;
        task->setId(id); // 保持原ID
        cout << "✏️ 任务更新成功！ID: " << id << endl;
        return true;
    }

    cout << "❌ 错误：找不到ID为 " << id << " 的任务。" << endl;
    return false;
}

// 检查是否任务
bool TaskManager::hasTasks() const {
    return !tasks.empty();
}

// 获取任务数量
int TaskManager::getTaskCount() const {
    return static_cast<int>(tasks.size());
}

// 输出显示
void TaskManager::showAllTasks() const {
    if (tasks.empty()) {
        cout << "\n📋 当前没有任务！" << endl;
        return;
    }

    cout << "\n========== 所有任务 (" << tasks.size() << ") ==========" << endl;
    for (const auto& task : tasks) {
        task.display();
    }
    cout << "========================================" << endl;
}

void TaskManager::showUncompletedTasks() const {
    int count = 0;
    cout << "\n========== 未完成任务 ==========" << endl;
    for (const auto& task : tasks) {
        if (!task.getIsCompleted()) {
            task.display();
            count++;
        }
    }
    if (count == 0) {
        cout << "🎉 恭喜！所有任务都已完成！" << endl;
    }
    else {
        cout << "剩余: " << count << " 个任务待完成" << endl;
    }
    cout << "==================================" << endl;
}

void TaskManager::showCompletedTasks() const {
    int count = 0;
    cout << "\n========== 已完成任务 ==========" << endl;
    for (const auto& task : tasks) {
        if (task.getIsCompleted()) {
            task.display();
            count++;
        }
    }
    if (count == 0) {
        cout << "还没有完成任务，继续努力！" << endl;
    }
    else {
        cout << "已完成: " << count << " 个任务" << endl;
    }
    cout << "==================================" << endl;
}

// 任务操作
bool TaskManager::markTaskCompleted(int id) {
    Task* task = findTaskById(id);
    if (task) {
        if (task->getIsCompleted()) {
            cout << "⚠️ 任务 [" << id << "] 已经是完成状态。" << endl;
            return true;
        }
        task->setIsCompleted(true);
        task->setStatus(Task::TaskStatus::COMPLETED);
        cout << "✅ 任务 [" << id << "] 已标记为完成！" << endl;
        return true;
    }

    cout << "❌ 错误：找不到ID为 " << id << " 的任务。" << endl;
    return false;
}

bool TaskManager::markTaskUncompleted(int id) {
    Task* task = findTaskById(id);
    if (task) {
        if (!task->getIsCompleted()) {
            cout << "⚠️ 任务 [" << id << "] 已经是未完成状态。" << endl;
            return true;
        }
        task->setIsCompleted(false);
        task->setStatus(Task::TaskStatus::PENDING);
        cout << "🔄 任务 [" << id << "] 已重新标记为未完成。" << endl;
        return true;
    }

    cout << "❌ 错误：找不到ID为 " << id << " 的任务。" << endl;
    return false;
}

Task* TaskManager::findTaskById(int id) {
    for (auto& task : tasks) {
        if (task.getId() == id) {
            return &task;
        }
    }
    return nullptr;
}

const vector<Task>& TaskManager::getTasks() const {
    return tasks;
}

int TaskManager::getNextId() const {
    if (tasks.empty()) {
        return 1;
    }

    int maxId = 0;
    for (const auto& task : tasks) {
        if (task.getId() > maxId) {
            maxId = task.getId();
        }
    }
    return maxId + 1;
}

// 辅助函数
string TaskManager::priorityToString(int p) const {
    switch (p) {
    case 0: return "低";
    case 1: return "中";
    case 2: return "高";
    default: return "未知";
    }
}

// 显示函数
string TaskManager::getStatusSymbol(bool isCompleted) const {
    return isCompleted ? "[✓]" : "[ ]";
}

// 文件操作，将当前内存中的所有任务保存至一个文本文件中
void TaskManager::saveToFile(const string& filename) const {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "❌ 错误：无法保存文件 " << filename << endl;
        return;
    }

    // 写入文件头
    file << "# 任务存储文件" << endl;
    file << "# 格式: id,name,description,priority,estimatedMinutes,deadline,status,isCompleted" << endl;

    for (const auto& task : tasks) {
        file << task.getId() << ","
            << task.getName() << ","
            << task.getDescription() << ","
            << static_cast<int>(task.getPriority()) << ","
            << task.getEstimatedMinutes() << ","
            << chrono::system_clock::to_time_t(task.getDeadline()) << ","
            << static_cast<int>(task.getStatus()) << ","
            << (task.getIsCompleted() ? "1" : "0") << endl;
    }

    file.close();
    cout << "💾 任务已保存到文件: " << filename << endl;
}

void TaskManager::loadFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "📂 未找到任务文件，将创建新文件。" << endl;
        return;
    }

    string line;
    int loadedCount = 0;

    // 跳过文件头
    while (getline(file, line) && line[0] == '#') {
        // 跳过注释行
    }

    do {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        stringstream ss(line);
        string token;
        vector<string> tokens;

        while (getline(ss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 8) {
            try {
                int id = stoi(tokens[0]);
                string name = tokens[1];
                string description = tokens[2];
                Task::Priority priority = static_cast<Task::Priority>(stoi(tokens[3]));
                int estimatedMinutes = stoi(tokens[4]);
                time_t deadlineTime = stoi(tokens[5]);
                Task::TaskStatus status = static_cast<Task::TaskStatus>(stoi(tokens[6]));
                bool isCompleted = (tokens[7] == "1");

                Task task(name, description, priority, estimatedMinutes);
                task.setId(id);
                task.setDeadline(system_clock::from_time_t(deadlineTime));
                task.setStatus(status);
                task.setIsCompleted(isCompleted);

                tasks.push_back(task);
                loadedCount++;

                if (id >= nextId) {
                    nextId = id + 1;
                }

            }
            catch (const exception& e) {
                cerr << "⚠️ 警告：解析任务行时出错: " << line << endl;
            }
        }
    } while (getline(file, line));

    file.close();

    if (loadedCount > 0) {
        cout << "📂 从文件加载了 " << loadedCount << " 个任务" << endl;
    }
}