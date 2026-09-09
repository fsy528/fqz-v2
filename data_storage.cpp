// 数据存储实现源文件（负责人；李罡）
#include "data_storage.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace chrono;

// 常量定义
const string DataStorage::TASK_FILE_HEADER = "# Task Data File v1.0";
const string DataStorage::LOG_FILE_NAME = "pomodoro_log.txt";

// 辅助函数
vector<string> DataStorage::split(const string& str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

string DataStorage::trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == string::npos) return "";

    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// 日期格式校验
bool DataStorage::isValidDate(const string& dateStr) {
    if (dateStr.empty()) return false;

    // 简单检查格式：YYYY-MM-DD HH:MM:SS
    if (dateStr.length() < 19) return false;

    // 检查分隔符
    if (dateStr[4] != '-' || dateStr[7] != '-' ||
        dateStr[10] != ' ' || dateStr[13] != ':' || dateStr[16] != ':') {
        return false;
    }

    return true;
}

// 获取输出时间
string DataStorage::getCurrentDate() {
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    tm tmStruct = {};

#ifdef _WIN32
    localtime_s(&tmStruct, &time);
#else
    localtime_r(&time, &tmStruct);
#endif

    stringstream ss;
    ss << put_time(&tmStruct, "%Y-%m-%d");
    return ss.str();
}

// 格式化输出日志
string DataStorage::getCurrentDateTime() {
    auto now = system_clock::now();
    auto time = system_clock::to_time_t(now);
    tm tmStruct = {};

#ifdef _WIN32
    localtime_s(&tmStruct, &time);
#else
    localtime_r(&time, &tmStruct);
#endif

    stringstream ss;
    ss << put_time(&tmStruct, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 时间点转字符串
string DataStorage::timeToString(const chrono::system_clock::time_point& time) {
    auto timeT = system_clock::to_time_t(time);
    tm tmStruct = {};

#ifdef _WIN32
    localtime_s(&tmStruct, &timeT);
#else
    localtime_r(&timeT, &tmStruct);
#endif

    stringstream ss;
    ss << put_time(&tmStruct, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 字符串转时间点
chrono::system_clock::time_point DataStorage::stringToTime(const string& timeStr) {
    if (timeStr.empty() || timeStr == "0") {
        return system_clock::now();
    }

    tm tmStruct = {};
    stringstream ss(timeStr);
    ss >> get_time(&tmStruct, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        // 尝试只解析日期
        ss.clear();
        ss.str(timeStr);
        ss >> get_time(&tmStruct, "%Y-%m-%d");

        if (ss.fail()) {
            return system_clock::now(); // 解析失败，返回当前时间
        }
    }

    auto timeT = mktime(&tmStruct);
    return system_clock::from_time_t(timeT);
}

// ----------- 任务文件操作 ----------
bool DataStorage::loadTasksFromFile(const string& filename, vector<Task>& tasks) {
    ifstream file(filename);
    if (!file.is_open()) {
        return false; // 文件不存在是正常情况
    }

    string line;
    int loadedCount = 0;
    int maxId = 0;

    // 检查文件头
    if (getline(file, line)) {
        if (line.find("# Task Data File") == string::npos) {
            // 不是我们的格式，重置文件指针
            file.seekg(0);
        }
    }
    else {
        file.seekg(0);
    }

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue; // 跳过空行和注释
        }

        vector<string> tokens = split(line, ',');
        if (tokens.size() < 8) {
            cerr << "警告：跳过格式错误的行: " << line << endl;
            continue;
        }

        try {
            // 解析字段
            int id = stoi(tokens[0]);
            string name = tokens[1];
            string description = tokens[2];
            Task::Priority priority = static_cast<Task::Priority>(stoi(tokens[3]));
            int estimatedMinutes = stoi(tokens[4]);
            string deadlineStr = tokens[5];
            Task::TaskStatus status = static_cast<Task::TaskStatus>(stoi(tokens[6]));
            bool isCompleted = (tokens[7] == "1");

            // 创建任务对象
            Task task(name, description, priority, estimatedMinutes);
            task.setId(id);

            // 设置截止日期
            if (!deadlineStr.empty() && deadlineStr != "0") {
                auto deadlineTime = stringToTime(deadlineStr);
                task.setDeadline(deadlineTime);
            }

            task.setStatus(status);
            task.setIsCompleted(isCompleted);

            tasks.push_back(task);
            loadedCount++;

            if (id > maxId) {
                maxId = id;
            }

        }
        catch (const exception& e) {
            cerr << "警告：解析任务行时出错: " << line << endl;
            cerr << "错误信息: " << e.what() << endl;
            continue;
        }
    }

    file.close();

    if (loadedCount > 0) {
        cout << "✅ 从文件加载了 " << loadedCount << " 个任务" << endl;
    }

    return true;
}

bool DataStorage::saveTasksToFile(const string& filename, const vector<Task>& tasks) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "❌ 错误：无法保存文件 " << filename << endl;
        return false;
    }

    // 写入文件头
    file << TASK_FILE_HEADER << endl;
    file << "# 格式: id,name,description,priority,estimatedMinutes,deadline,status,isCompleted" << endl;
    file << "# 保存时间: " << getCurrentDateTime() << endl;
    file << "# ========================================" << endl;

    for (const auto& task : tasks) {
        file << task.getId() << ","
            << task.getName() << ","
            << task.getDescription() << ","
            << static_cast<int>(task.getPriority()) << ","
            << task.getEstimatedMinutes() << ","
            << timeToString(task.getDeadline()) << ","
            << static_cast<int>(task.getStatus()) << ","
            << (task.getIsCompleted() ? "1" : "0") << endl;
    }

    file.close();
    cout << "💾 任务已保存到文件: " << filename << endl;
    return true;
}

// ---------- 日志文件操作 ----------
void DataStorage::recordPomodoro(const string& taskName, int duration) {
    ofstream file(LOG_FILE_NAME, ios::app);
    if (!file.is_open()) {
        cerr << "❌ 无法打开日志文件" << endl;
        return;
    }

    string now = getCurrentDateTime();

    file << now << " | "
        << "任务: " << (taskName.empty() ? "无任务" : taskName) << " | "
        << "时长: " << duration << "分钟" << endl;

    file.close();
}

// 增强版：记录完整专注会话
void DataStorage::recordFocusSession(const string& taskName,
                                     int plannedMinutes,
                                     int actualMinutes,
                                     int feedbackLevel,
                                     bool earlyEnded,
                                     const string& startTime) {
    ofstream file(LOG_FILE_NAME, ios::app);
    if (!file.is_open()) {
        cerr << "❌ 无法打开日志文件" << endl;
        return;
    }

    string endTime = getCurrentDateTime();
    // 实际时长：没记录到则用计划时长
    if (actualMinutes <= 0) actualMinutes = plannedMinutes;

    file << "开始: " << startTime << " | "
        << "结束: " << endTime << " | "
        << "任务: " << (taskName.empty() ? "无任务" : taskName) << " | "
        << "计划: " << plannedMinutes << "分钟 | "
        << "实际: " << actualMinutes << "分钟 | "
        << "反馈: " << feedbackLevel << " | "
        << "提前: " << (earlyEnded ? "1" : "0") << endl;

    file.close();
}

void DataStorage::showStatistics() {
    ifstream file(LOG_FILE_NAME);
    if (!file.is_open()) {
        cout << "\n📊 暂无统计数据" << endl;
        cout << "提示：开始使用番茄钟后，数据会自动记录" << endl;
        return;
    }

    int totalPomodoros = 0;
    int totalMinutes = 0;
    string today = getCurrentDate();
    int todayPomodoros = 0;
    int todayMinutes = 0;
    string line;

    // 读取所有记录
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        totalPomodoros++;

        // 解析时长：优先新格式 "实际: X分钟"，回退旧格式 "时长: X分钟"
        size_t pos = line.find("实际: ");
        std::string lenMark = "实际: ";
        if (pos == string::npos) {
            pos = line.find("时长: ");
            lenMark = "时长: ";
        }
        if (pos != string::npos) {
            pos += lenMark.size();
            size_t endPos = line.find("分钟", pos);
            if (endPos != string::npos) {
                string minutesStr = line.substr(pos, endPos - pos);
                try {
                    int minutes = stoi(minutesStr);
                    totalMinutes += minutes;

                    // 检查是否是今天的记录
                    if (line.find(today) != string::npos) {
                        todayPomodoros++;
                        todayMinutes += minutes;
                    }
                }
                catch (...) {
                    // 忽略转换错误
                }
            }
        }
    }

    file.close();

    // 显示统计信息
    cout << "\n========== 📈 番茄钟统计 ==========" << endl;
    cout << "📅 今日统计 (" << today << "):" << endl;
    cout << "   完成番茄钟: " << todayPomodoros << " 个" << endl;
    cout << "   累计时长: " << todayMinutes << " 分钟" << endl;

    if (todayPomodoros > 0) {
        double avgToday = static_cast<double>(todayMinutes) / todayPomodoros;
        cout << "   平均时长: " << fixed << setprecision(1) << avgToday << " 分钟/个" << endl;
    }

    cout << "\n📊 历史总统计:" << endl;
    cout << "   总番茄钟: " << totalPomodoros << " 个" << endl;
    cout << "   总时长: " << totalMinutes << " 分钟 ("
        << fixed << setprecision(1) << (totalMinutes / 60.0) << " 小时)" << endl;

    if (totalPomodoros > 0) {
        double avgTotal = static_cast<double>(totalMinutes) / totalPomodoros;
        cout << "   平均时长: " << fixed << setprecision(1) << avgTotal << " 分钟/个" << endl;
    }

    cout << "=======================================" << endl;
}

// ---------- 智能解析 -----------
Task DataStorage::parseInput(const string& input, int& nextId) {
    // 默认任务
    string name = input;
    if (name.length() > 50) {
        name = name.substr(0, 50) + "...";
    }

    Task task(name, "");

    // 转换为小写以便比较
    string lowerInput = input;
    transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

    // 解析优先级
    Task::Priority priority = Task::Priority::MEDIUM;
    if (lowerInput.find("高") != string::npos ||
        lowerInput.find("重要") != string::npos ||
        lowerInput.find("紧急") != string::npos ||
        lowerInput.find("urgent") != string::npos ||
        lowerInput.find("important") != string::npos) {
        priority = Task::Priority::HIGH;
    }
    else if (lowerInput.find("低") != string::npos ||
        lowerInput.find("不着急") != string::npos ||
        lowerInput.find("low") != string::npos ||
        lowerInput.find("不重要") != string::npos) {
        priority = Task::Priority::LOW;
    }
    task.setPriority(priority);

    // 解析预估时间
    int estimatedMinutes = 25; // 默认一个番茄钟
    if (lowerInput.find("半小时") != string::npos ||
        lowerInput.find("30分钟") != string::npos ||
        lowerInput.find("30min") != string::npos) {
        estimatedMinutes = 30;
    }
    else if (lowerInput.find("1小时") != string::npos ||
        lowerInput.find("60分钟") != string::npos ||
        lowerInput.find("60min") != string::npos) {
        estimatedMinutes = 60;
    }
    else if (lowerInput.find("2小时") != string::npos ||
        lowerInput.find("120分钟") != string::npos) {
        estimatedMinutes = 120;
    }
    else if (lowerInput.find("3小时") != string::npos) {
        estimatedMinutes = 180;
    }
    else if (lowerInput.find("4小时") != string::npos) {
        estimatedMinutes = 240;
    }
    task.setEstimatedMinutes(estimatedMinutes);

    // 解析截止日期
    auto now = system_clock::now();
    if (lowerInput.find("今天") != string::npos ||
        lowerInput.find("today") != string::npos) {
        task.setDeadline(now);
    }
    else if (lowerInput.find("明天") != string::npos ||
        lowerInput.find("tomorrow") != string::npos) {
        task.setDeadline(now + hours(24));
    }
    else if (lowerInput.find("后天") != string::npos ||
        lowerInput.find("day after tomorrow") != string::npos) {
        task.setDeadline(now + hours(48));
    }
    else if (lowerInput.find("这周") != string::npos ||
        lowerInput.find("本周") != string::npos ||
        lowerInput.find("this week") != string::npos) {
        task.setDeadline(now + hours(24 * 7));
    }
    else if (lowerInput.find("下周") != string::npos ||
        lowerInput.find("next week") != string::npos) {
        task.setDeadline(now + hours(24 * 14));
    }
    else {
        // 默认设置为明天
        task.setDeadline(now + hours(24));
    }

    // 设置ID
    task.setId(nextId++);

    // 设置默认状态
    task.setStatus(Task::TaskStatus::PENDING);
    task.setIsCompleted(false);

    return task;
}

// ---------- 统计信息 -----------
// 统计现在番茄钟数量
int DataStorage::getTodayPomodoros() {
    ifstream file(LOG_FILE_NAME);
    if (!file.is_open()) return 0;

    int count = 0;
    string today = getCurrentDate();
    string line;

    while (getline(file, line)) {
        if (line.find(today) != string::npos) {
            count++;
        }
    }

    file.close();
    return count;
}

// 统计历史总数量
int DataStorage::getTotalPomodoros() {
    ifstream file(LOG_FILE_NAME);
    if (!file.is_open()) return 0;

    int count = 0;
    string line;

    while (getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            count++;
        }
    }

    file.close();
    return count;
}

// 统计平均专注的时间
double DataStorage::getAverageFocusTime() {
    ifstream file(LOG_FILE_NAME);
    if (!file.is_open()) return 0.0;

    int totalPomodoros = 0;
    int totalMinutes = 0;
    string line;

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        totalPomodoros++;

        size_t pos = line.find("时长: ");
        if (pos != string::npos) {
            pos += std::string("时长: ").size();
            size_t endPos = line.find("分钟", pos);
            if (endPos != string::npos) {
                string minutesStr = line.substr(pos, endPos - pos);
                try {
                    totalMinutes += stoi(minutesStr);
                }
                catch (...) {
                    // 忽略错误
                }
            }
        }
    }

    file.close();

    if (totalPomodoros == 0) return 0.0;
    return static_cast<double>(totalMinutes) / totalPomodoros;
}

// ---------- AI 自适应：加载专注记录 ----------
// 安全整数解析辅助
static bool safeToInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    try {
        out = std::stoi(s);
        return true;
    } catch (...) {
        return false;
    }
}

// 从一个标记字段中取出整数值（标记与下一个 " | " 之间）
static int extractFieldInt(const std::string& line, const std::string& marker, int defaultValue) {
    size_t pos = line.find(marker);
    if (pos == std::string::npos) return defaultValue;
    pos += marker.size();
    size_t endPos = line.find("分钟", pos);
    if (endPos == std::string::npos) endPos = line.find(" | ", pos);
    if (endPos == std::string::npos) endPos = line.size();
    std::string sub = line.substr(pos, endPos - pos);
    int v = defaultValue;
    safeToInt(sub, v);
    return v;
}

vector<FocusRecord> DataStorage::loadFocusRecords() {
    vector<FocusRecord> records;
    ifstream file(LOG_FILE_NAME);
    if (!file.is_open()) return records;

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        // 只解析新格式：以 "开始: " 开头
        if (line.find("开始: ") != 0) continue;

        FocusRecord rec;
        rec.actualMinutes = 0;
        rec.feedbackLevel = 0;
        rec.earlyEnded = false;

        // 从 "开始:" 到 " | 结束:" 截取 startTime
        size_t startPos = line.find("开始: ") + 5;
        size_t endPos = line.find(" | 结束: ", startPos);
        if (endPos != string::npos) {
            rec.startTime = trim(line.substr(startPos, endPos - startPos));
        }

        size_t taskPos = line.find("任务: ");
        if (taskPos != string::npos) {
            taskPos += 5;
            size_t taskEnd = line.find(" | ", taskPos);
            rec.taskName = (taskEnd != string::npos)
                ? trim(line.substr(taskPos, taskEnd - taskPos))
                : trim(line.substr(taskPos));
        }

        rec.plannedMinutes = extractFieldInt(line, "计划: ", 25);
        rec.actualMinutes = extractFieldInt(line, "实际: ", rec.plannedMinutes);
        rec.feedbackLevel = extractFieldInt(line, "反馈: ", 0);
        // 提前结束标记
        size_t earlyPos = line.find("提前: ");
        if (earlyPos != string::npos) {
            earlyPos += 5;
            string tail = trim(line.substr(earlyPos));
            rec.earlyEnded = (tail.find("1") != string::npos);
        }

        if (rec.startTime.empty()) continue;
        records.push_back(rec);
    }

    file.close();
    return records;
}

// ---------- AI 自适应：个性化推荐时长 ----------
int DataStorage::getRecommendedFocusDuration() {
    vector<FocusRecord> records = loadFocusRecords();
    if (records.empty()) return 25; // 无历史，默认 25 分钟

    int total = 0;
    int count = 0;
    for (const auto& r : records) {
        if (r.actualMinutes > 0) {
            total += r.actualMinutes;
            count++;
        }
    }
    if (count == 0) return 25;

    int avg = static_cast<int>(static_cast<double>(total) / count + 0.5);
    // 推荐值取平均，限制在 15 - 60 分钟之间
    if (avg < 15) avg = 15;
    if (avg > 60) avg = 60;
    return avg;
}

// ---------- AI 自适应：时间利用画像 ----------
void DataStorage::showFocusProfile() {
    vector<FocusRecord> records = loadFocusRecords();
    cout << "\n========== " << "\033[1m" << "🧠 专注画像" << "\033[0m" << " ==========" << endl;
    if (records.empty()) {
        cout << "暂无专注数据，先完成一个番茄钟吧。" << endl;
        cout << "=========================================" << endl;
        return;
    }

    int total = (int)records.size();
    int very = 0, normal = 0, dist = 0, early = 0, actualSum = 0;
    for (const auto& r : records) {
        if (r.feedbackLevel == 1) very++;
        else if (r.feedbackLevel == 2) normal++;
        else if (r.feedbackLevel == 3) dist++;
        if (r.earlyEnded) early++;
        actualSum += r.actualMinutes;
    }

    cout << "总专注次数: " << total << " 次，累计 " << actualSum << " 分钟 ("
        << fixed << setprecision(1) << (actualSum / 60.0) << " 小时)" << endl;

    if (very + normal + dist > 0) {
        double focusScore = (very * 1.0 + normal * 0.5) / (very + normal + dist);
        cout << "综合专注度: " << fixed << setprecision(0) << (focusScore * 100) << "%" << endl;
    }
    if (early > 0) {
        cout << "提前结束率: " << fixed << setprecision(0) << (early * 100.0 / total) << "%" << endl;
    }

    if (records.size() > 1) cout << "平均每次专注: " << (actualSum / total) << " 分钟" << endl;

    cout << "=========================================" << endl;
}

// ---------- AI 自适应：时间分布（何时工作/休息） ----------
void DataStorage::showTimeDistribution() {
    vector<FocusRecord> records = loadFocusRecords();
    cout << "\n========== " << "\033[1m" << "⏰ 时间利用分布" << "\033[0m" << " ==========" << endl;
    if (records.empty()) {
        cout << "暂无专注数据。" << endl;
        cout << "=========================================" << endl;
        return;
    }

    // 分 6 个时段（每 4 小时一个），统计专注次数与时长
    // 0-4, 4-8, 8-12, 12-16, 16-20, 20-24
    int buckets[6] = {0, 0, 0, 0, 0, 0};
    int minutes[6] = {0, 0, 0, 0, 0, 0};
    const char* labels[6] = {"00-04", "04-08", "08-12", "12-16", "16-20", "20-24"};

    for (const auto& r : records) {
        if (r.startTime.size() < 11) continue;
        int hour = atoi(r.startTime.substr(11, 2).c_str());
        int idx = hour / 4;
        if (idx < 0) idx = 0;
        if (idx > 5) idx = 5;
        buckets[idx]++;
        minutes[idx] += r.actualMinutes;
    }

    for (int i = 0; i < 6; i++) {
        if (buckets[i] == 0) continue;
        cout << labels[i] << "时段 : " << buckets[i] << " 次专注, "
            << minutes[i] << " 分钟" << endl;
    }

    // 找出最专注时段
    int best = 0;
    for (int i = 1; i < 6; i++) if (minutes[i] > minutes[best]) best = i;
    if (minutes[best] > 0) {
        cout << "\n你最专注的时段是 " << labels[best] << " 点区间。" << endl;
    }
    cout << "=========================================" << endl;
}
