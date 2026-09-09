// 主程序（负责人：李罡）— AI 自适应增强版
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>

// 跨平台清屏
#ifdef _WIN32
#include <windows.h>
#define CLEAR_SCREEN() system("cls") // windows
#else
#define CLEAR_SCREEN() system("clear") // linux、mac
#endif

#include "task_manager.h"
#include "pomodoro_timer.h"
#include "data_storage.h"

using namespace std;

// ---------- ANSI 颜色 ----------
static const string C_RESET  = "\033[0m";
static const string C_RED    = "\033[31m";
static const string C_GREEN  = "\033[32m";
static const string C_YELLOW = "\033[33m";
static const string C_BLUE   = "\033[34m";
static const string C_CYAN   = "\033[36m";
static const string C_BOLD   = "\033[1m";

// Windows 原生：仅设置后续文字的前景色/背景色（不做整屏填充、不移动光标，避免重叠）
static void setConsoleTextColor(int fg, int bg) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(h, &info)) {
        WORD attr = static_cast<WORD>((fg & 0x0F) | ((bg & 0x0F) << 4));
        SetConsoleTextAttribute(h, attr);
    }
#else
    // 非 Windows 用 ANSI 前景色
    switch (fg) {
    case 10: std::cout << "\033[32m"; break;  // 绿
    case 11: std::cout << "\033[36m"; break;  // 青
    case 12: std::cout << "\033[31m"; break;  // 红
    default: std::cout << "\033[37m"; break;  // 白
    }
    // 背景色
    switch (bg) {
    case 2: std::cout << "\033[42m"; break;  // 绿
    case 1: std::cout << "\033[44m"; break;  // 蓝
    case 4: std::cout << "\033[41m"; break;  // 红
    default: std::cout << "\033[40m"; break; // 黑
    }
#endif
}

// 全局变量
TaskManager taskManager;
PomodoroTimer pomodoro;
const string TASK_FILE = "tasks.txt";

// 等待用户按回车
void pause() {
    cout << "\n按回车键继续...";
    cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    cin.get();
}

// 显示主菜单（彩色）
void showMainMenu() {
    CLEAR_SCREEN();
    cout << C_BOLD << "========================================\n";
    cout << "       🍅 番茄钟任务管理系统 v2.0 (AI自适应)\n";
    cout << "========================================\n" << C_RESET;
    cout << "1. 查看所有任务\n";
    cout << "2. 智能添加任务（固定的自然语言）\n";
    cout << "3. 手动添加任务\n";
    cout << "4. 开始番茄钟\n";
    cout << "5. 标记任务完成\n";
    cout << "6. 删除任务\n";
    cout << "7. 查看统计信息\n";
    cout << C_CYAN << "8. 专注画像 / 时间利用分布\n";
    cout << C_YELLOW << "9. 设置反馈间隔 / 个性化时长\n";
    cout << C_RESET << "10. 保存并退出\n";
    cout << C_BOLD << "========================================\n" << C_RESET;
    cout << "当前任务数: " << taskManager.getTaskCount() << "\n";
    cout << "请选择操作 (1-10): ";
}

// 智能添加任务
void addTaskSmart() {
    CLEAR_SCREEN();
    cout << "========== 智能添加任务 ==========\n";
    cout << "请输入任务描述（支持自然语言）：\n";
    cout << "例如：明天完成高数作业，优先级高，预计2个番茄\n";
    cout << "输入（输入 0 取消）: ";

    string input;
    cin.ignore();
    getline(cin, input);

    if (input.empty()) {
        cout << "输入不能为空！\n";
        return;
    }
    if (input == "0" || input == "0 ") {
        cout << "已取消添加。\n";
        return;
    }

    int nextId = taskManager.getNextId();
    Task task = DataStorage::parseInput(input, nextId);
    taskManager.addTask(task);

    cout << "\n✅ 任务已添加：\n";
    task.display();
}

// 手动添加任务
void addTaskManual() {
    CLEAR_SCREEN();
    cout << "========== 手动添加任务 ==========\n";

    string name;
    int priority, estimatedPomos;

    cout << "任务名称（输入 0 取消）：";
    cin.ignore();
    getline(cin, name);

    if (name == "0" || name == "0 ") {
        cout << "已取消添加。\n";
        return;
    }

    cout << "优先级 (1-低, 2-中, 3-高)：";
    cin >> priority;

    cout << "预估番茄数：";
    cin >> estimatedPomos;

    Task task(name, "", static_cast<Task::Priority>(priority - 1), estimatedPomos);
    taskManager.addTask(task);

    cout << "✅ 任务添加成功！\n";
}

// 反馈询问（AI 自适应核心）
void askFeedback() {
    cout << "\n" << C_YELLOW << C_BOLD << "────────── 专注度反馈 ──────────\n";
    cout << "现在感觉怎么样？\n";
    cout << "1. 非常专注 (状态很好)\n";
    cout << "2. 状态一般 (保持节奏)\n";
    cout << "3. 容易分心 (想休息)\n";
    cout << "请选择 (1-3): " << C_RESET;

    int choice = 0;
    if (!(cin >> choice)) {
        cin.clear();
        cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        choice = 2;
    }

    FocusFeedback level = FocusFeedback::NORMAL;
    if (choice == 1) level = FocusFeedback::VERY_FOCUSED;
    else if (choice == 3) level = FocusFeedback::DISTRACTED;

    pomodoro.applyFeedback(level);
}

// 开始番茄钟
void startPomodoro() {
    CLEAR_SCREEN();

    cout << "========== 选择任务 ==========\n";
    taskManager.showUncompletedTasks();

    if (!taskManager.hasTasks()) {
        cout << "当前没有未完成任务，请先添加任务。\n";
        pause();
        return;
    }

    int taskId;
    cout << "\n请输入任务ID (0: 不关联任务, -1: 取消): ";
    cin >> taskId;

    if (taskId == -1) {
        return;
    }

    string taskName = "";
    if (taskId > 0) {
        Task* task = taskManager.findTaskById(taskId);
        if (task) {
            taskName = task->getName();
        } else {
            cout << "任务ID不存在，将创建普通番茄钟\n";
        }
    }

    CLEAR_SCREEN();
    cout << "========== 选择专注时长 ==========\n";
    cout << "1. 标准番茄钟 (25分钟)\n";
    cout << "2. 短休息番茄钟 (5分钟)\n";
    cout << "3. 长休息番茄钟 (15分钟)\n";
    cout << C_CYAN << "4. AI 推荐时长 (基于历史，" << DataStorage::getRecommendedFocusDuration() << "分钟)\n";
    cout << C_RESET << "5. 自定义时间\n";
    cout << "0. 返回主菜单\n";
    cout << "================================\n";
    cout << "请选择: ";

    int choice;
    cin >> choice;

    if (choice == 0) {
        cout << "已返回。\n";
        return;
    }

    int minutes = 25;
    switch (choice) {
    case 1: minutes = 25; break;
    case 2: minutes = 5; break;
    case 3: minutes = 15; break;
    case 4: minutes = DataStorage::getRecommendedFocusDuration(); break;
    case 5:
        cout << "请输入分钟数: ";
        cin >> minutes;
        break;
    default:
        cout << "无效选择，使用默认25分钟\n";
    }

    CLEAR_SCREEN();

    // 反馈系统（决定是否开启自动询问）
    bool useFeedback = true;
    if (pomodoro.getFeedbackInterval() <= 0) {
        useFeedback = false;
    }

    // 记录专注的开始时间
    string startTime = DataStorage::getCurrentDateTime();

    // 进入专注前设置为绿色文字（专注状态），不做整屏填充避免重叠
    setConsoleTextColor(10, 0);   // 绿字

    pomodoro.startWork(taskName, minutes);

    // 倒计时循环 + 反馈询问（计时行文字颜色随专注状态变化，不做整屏填充避免重叠）
    int fgColor = 10;  // 默认绿字（专注开始）
    int lastFg = 10;
    while (pomodoro.isRunning() || pomodoro.isOnBreak()) {
        if (pomodoro.isRunning() && useFeedback && pomodoro.shouldAskFeedback()) {
            pomodoro.markFeedbackAsked();
            askFeedback();
        }

        // 根据当前专注状态设置文字颜色（1=非常专注→绿 2=一般→青 3=分心→红）
        string bg = pomodoro.getFocusBgColor();
        if (bg.find("42") != string::npos) fgColor = 10;       // 绿（专注）
        else if (bg.find("44") != string::npos) fgColor = 11;  // 青（平稳）
        else if (bg.find("41") != string::npos) fgColor = 12;  // 红（分心）

        if (fgColor != lastFg) {
            lastFg = fgColor;
            setConsoleTextColor(fgColor, 0);  // 只改文字前景色，不动光标
        }

        pomodoro.displayTime();
        cout << flush;
        this_thread::sleep_for(chrono::seconds(1));
    }
    // 结束后恢复默认文字颜色（白）
    setConsoleTextColor(7, 0);

    // 记录专注会话
    int actualMinutes = minutes;
    // 用真实累计的专注秒数计算实际时长（延长/提前结束都准确）
    int focusSeconds = pomodoro.getActualFocusSeconds();
    if (focusSeconds > 0) {
        int calc = focusSeconds / 60;
        actualMinutes = (calc > 0) ? calc : 0;
    }
    if (actualMinutes < 0) actualMinutes = 0;
    // 反馈等级：取本轮占比最高的等级（1=非常专注 2=一般 3=分心），无反馈则为 0
    int feedbackLevel = 0;
    if (pomodoro.getVeryFocusedTotal() >= pomodoro.getNormalTotal()
        && pomodoro.getVeryFocusedTotal() >= pomodoro.getDistractedTotal()
        && pomodoro.getVeryFocusedTotal() > 0) {
        feedbackLevel = 1;
    } else if (pomodoro.getDistractedTotal() > pomodoro.getVeryFocusedTotal()
        && pomodoro.getDistractedTotal() > pomodoro.getNormalTotal()) {
        feedbackLevel = 3;
    } else if (pomodoro.getNormalTotal() > 0) {
        feedbackLevel = 2;
    }
    DataStorage::recordFocusSession(taskName, focusSeconds > 0 ? focusSeconds / 60 : minutes,
        actualMinutes, feedbackLevel, pomodoro.wasEarlyEnded(), startTime);

    // 结束后彻底清屏，清掉计时过程中的所有残留
    CLEAR_SCREEN();

    // 静默结束：不显示任何提示，自动标记任务完成，直接返回主菜单
    if (taskId > 0 && !pomodoro.wasEarlyEnded()) {
        taskManager.markTaskCompleted(taskId);
    }
}

// 标记任务完成
void markTaskComplete() {
    CLEAR_SCREEN();
    taskManager.showUncompletedTasks();

    if (taskManager.hasTasks()) {
        int taskId;
        cout << "\n输入要标记完成的任务ID（输入 0 取消）: ";
        cin >> taskId;
        if (taskId == 0) {
            cout << "已取消。\n";
        } else {
            taskManager.markTaskCompleted(taskId);
        }
    }
    pause();
}

// 删除任务
void deleteTask() {
    CLEAR_SCREEN();
    taskManager.showAllTasks();

    if (taskManager.hasTasks()) {
        int taskId;
        cout << "\n输入要删除的任务ID（输入 0 取消）: ";
        cin >> taskId;
        if (taskId == 0) {
            cout << "已取消删除。\n";
        } else {
            taskManager.deleteTask(taskId);
        }
    }
    pause();
}

// 菜单 8：专注画像 / 时间分布
void showAnalytics() {
    CLEAR_SCREEN();
    cout << "========== AI 数据分析 ==========\n";
    DataStorage::showFocusProfile();
    DataStorage::showTimeDistribution();
    pause();
}

// 菜单 9：设置反馈间隔 / 个性化时长
void showSettings() {
    CLEAR_SCREEN();
    cout << "========== 反馈设置 ==========\n";
    cout << "当前反馈间隔: " << pomodoro.getFeedbackInterval() << " 分钟 (0=关闭)\n";
    cout << "AI 推荐专注时长: " << DataStorage::getRecommendedFocusDuration() << " 分钟\n";
    cout << "\n请输入新的反馈间隔（分钟，0=关闭，输入 -1 返回）: ";
    int minutes;
    if (cin >> minutes) {
        if (minutes < 0) {
            cout << "已返回，未更改设置。\n";
        } else {
            pomodoro.setFeedbackInterval(minutes);
            cout << C_GREEN << "✅ 反馈间隔已设为 " << minutes << " 分钟\n" << C_RESET;
        }
    } else {
        cin.clear();
        cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        cout << "输入无效，保持原设置。\n";
    }
    pause();
}

int main() {
    // Windows下设置UTF-8编码，解决中文乱码
#ifdef _WIN32
    system("chcp 65001 > nul");
    SetConsoleOutputCP(CP_UTF8);
    // 启动时彻底清屏，清掉上次运行的残留
    system("cls");
#endif

    cout << "系统初始化中...\n";

    // 加载任务
    vector<Task> tasks;
    DataStorage::loadTasksFromFile(TASK_FILE, tasks);
    for (const auto& task : tasks) {
        taskManager.addTask(task);
    }

    cout << "系统就绪！\n";
    this_thread::sleep_for(chrono::milliseconds(500));

    int choice = 0;
    do {
        showMainMenu();
        if (!(cin >> choice)) {
            // 输入流结束或无有效输入（如 Ctrl+C、重定向EOF），防止死循环
            cin.clear();
            cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            cout << "\n输入已结束，正在保存并退出...\n";
            DataStorage::saveTasksToFile(TASK_FILE, taskManager.getTasks());
            break;
        }

        switch (choice) {
        case 1:
            taskManager.showAllTasks();
            pause();
            break;

        case 2:
            addTaskSmart();
            pause();
            break;

        case 3:
            addTaskManual();
            pause();
            break;

        case 4:
            startPomodoro();
            break;

        case 5:
            markTaskComplete();
            break;

        case 6:
            deleteTask();
            break;

        case 7:
            DataStorage::showStatistics();
            pause();
            break;

        case 8:
            showAnalytics();
            break;

        case 9:
            showSettings();
            break;

        case 10:
            DataStorage::saveTasksToFile(TASK_FILE, taskManager.getTasks());
            cout << "\n💾 数据已保存！\n";
            cout << "感谢使用，再见！🍅\n";
            this_thread::sleep_for(chrono::seconds(1));
            break;

        default:
            cout << "无效选择，请重新输入！\n";
            this_thread::sleep_for(chrono::milliseconds(500));
        }

    } while (choice != 10);

    return 0;
}
