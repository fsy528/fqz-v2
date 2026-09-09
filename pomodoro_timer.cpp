// 番茄钟实现（负责人：闫成举）— AI 自适应增强版
#include "pomodoro_timer.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <sstream>
#include <mutex>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

using namespace std;
using namespace chrono;

// 统计变量
static int completedPomodoros = 0;

// ---------- ANSI 颜色 ----------
static const string C_RESET  = "\033[0m";
static const string C_RED    = "\033[31m";
static const string C_GREEN  = "\033[32m";
static const string C_YELLOW = "\033[33m";
static const string C_BLUE   = "\033[34m";
static const string C_CYAN   = "\033[36m";
static const string C_BOLD   = "\033[1m";

// 构造函数和析构函数
PomodoroTimer::PomodoroTimer()
    : state(TimerState::STOPPED),
    workDuration(25min),
    breakDuration(5min),
    remainingTime(0s),
    stopFlag(false),
    pausedFlag(false),
    earlyStop(false),
    feedbackIntervalMinutes(0),
    feedbackPending(false),
    veryFocusedCount(0),
    normalCount(0),
    distractedCount(0),
    earlyEnded(false),
    extendedCount(0),
    totalFeedbackGiven(0),
    actualFocusSeconds(0) {
    initialize();
}

PomodoroTimer::PomodoroTimer(int workMinutes, int breakMinutes)
    : state(TimerState::STOPPED),
    workDuration(workMinutes * 60s),
    breakDuration(breakMinutes * 60s),
    remainingTime(0s),
    stopFlag(false),
    pausedFlag(false),
    earlyStop(false),
    feedbackIntervalMinutes(0),
    feedbackPending(false),
    veryFocusedCount(0),
    normalCount(0),
    distractedCount(0),
    earlyEnded(false),
    extendedCount(0),
    totalFeedbackGiven(0),
    actualFocusSeconds(0) {
    initialize();
}

PomodoroTimer::~PomodoroTimer() {
    cleanup();
}

void PomodoroTimer::initialize() {
    completedPomodoros = 0;
    lastFeedbackTime = steady_clock::now();
}

void PomodoroTimer::cleanup() {
    stopFlag = true;
    pausedFlag = false;

    // 如果当前就是计时线程自身，不能 join 自己（会死锁），只置标志即可
    if (timerThread.joinable() && timerThread.get_id() != std::this_thread::get_id()) {
        timerThread.join();
    }
}

// 倒计时运行：后台线程驱动
void PomodoroTimer::runCountdown(chrono::seconds duration) {
    {
        lock_guard<std::mutex> lock(timeMutex);
        endTime = steady_clock::now() + duration;
        remainingTime = duration;
    }

    while (!stopFlag) {
        if (earlyStop) break;   // 主线程设置了提前结束标志，自行退出

        if (pausedFlag) {
            SLEEP_MS(100);
            continue;
        }

        auto now = steady_clock::now();
        bool done = false;
        {
            lock_guard<std::mutex> lock(timeMutex);
            auto remaining = duration_cast<seconds>(endTime - now);
            // 累加本轮真实经过的专注秒数：用上一时刻的剩余时间与当前剩余时间之差
            int prevRemain = static_cast<int>(remainingTime.count());
            int newRemain = static_cast<int>(remaining.count());
            if (prevRemain > newRemain) {
                actualFocusSeconds += (prevRemain - newRemain);
            }
            remainingTime = remaining;
            if (remaining.count() <= 0) done = true;
        }
        if (done) break;

        SLEEP_MS(1000);
    }

    // 保证结束后 remainingTime 不为负
    {
        lock_guard<std::mutex> lock(timeMutex);
        if (remainingTime.count() < 0) remainingTime = 0s;
    }
}

// 工作阶段
void PomodoroTimer::workPhase() {
    state = TimerState::RUNNING;
    lastFeedbackTime = steady_clock::now();
    // 固定节奏：第一次询问时刻 = 开始时刻 + 间隔
    nextFeedbackTime = steady_clock::now() + std::chrono::minutes(feedbackIntervalMinutes);
    feedbackPending = false;
    earlyEnded = false;
    extendedCount = 0;
    veryFocusedCount = 0;
    normalCount = 0;
    distractedCount = 0;
    totalFeedbackGiven = 0;
    actualFocusSeconds = 0;
    focusBgColor = "\033[42m"; // 开始时默认绿底（专注）

    // 专注开始的界面由主循环 displayTime 独占显示，这里不再打印，避免叠行

    runCountdown(workDuration);

    if (!stopFlag) {
        if (earlyEnded) {
            completedPomodoros++;
            reset();
        } else {
            completedPomodoros++;
            if (breakDuration.count() > 0) {
                this_thread::sleep_for(1s);
                breakPhase();
            } else {
                reset();
            }
        }
    }
}

// 休息阶段
void PomodoroTimer::breakPhase() {
    state = TimerState::BREAK;

    runCountdown(breakDuration);

    if (!stopFlag) {
        reset();
    }
}

// 公共控制方法
void PomodoroTimer::startWork(const string& taskName, int minutes) {
    // 若已有番茄钟在运行，先安全停止旧线程，再启动新的（避免“已在运行中”卡住）
    if (state == TimerState::RUNNING || state == TimerState::BREAK) {
        stopFlag = true;
        pausedFlag = false;
        if (timerThread.joinable()) {
            timerThread.join();
        }
    }

    cleanup();

    if (!taskName.empty()) {
        currentTask = taskName;
    }
    if (minutes > 0) {
        workDuration = seconds(minutes * 60);
    }

    stopFlag = false;
    pausedFlag = false;
    earlyStop = false;

    // 关键：先同步设置状态为 RUNNING（主线程），保证主循环立即检测到已开始，
    // 否则后台线程尚未执行到设置状态时，主循环会误判"未运行"而直接退出
    state = TimerState::RUNNING;

    timerThread = thread(&PomodoroTimer::workPhase, this);
}

void PomodoroTimer::startBreak(int minutes) {
    if (state == TimerState::RUNNING) {
        stopFlag = true;
        if (timerThread.joinable()) {
            timerThread.join();
        }
    }

    if (minutes > 0) {
        breakDuration = seconds(minutes * 60);
    }

    stopFlag = false;
    pausedFlag = false;
    timerThread = thread(&PomodoroTimer::breakPhase, this);
}

void PomodoroTimer::pause() {
    if (state == TimerState::RUNNING || state == TimerState::BREAK) {
        pausedFlag = true;
        state = TimerState::PAUSED;
        cout << "\n⏸️ 已暂停" << endl;
    }
}

void PomodoroTimer::resume() {
    if (state == TimerState::PAUSED) {
        pausedFlag = false;
        state = TimerState::RUNNING;
        cout << "\n▶️ 已继续" << endl;
    }
}

// 番茄钟重置
void PomodoroTimer::reset() {
    cleanup();
    state = TimerState::STOPPED;
    {
        lock_guard<std::mutex> lock(timeMutex);
        remainingTime = 0s;
    }
}

// 倒计时逻辑（兼容性保留：从后台线程驱动起，此函数主要用于状态判断）
void PomodoroTimer::update() {
    // 主循环不断调用 displayTime 与反馈检查，计时由后台 runCountdown 线程驱动
}

// ---------- 查询方法 -----------
bool PomodoroTimer::isRunning() const {
    return state == TimerState::RUNNING;
}

bool PomodoroTimer::isOnBreak() const {
    return state == TimerState::BREAK;
}

bool PomodoroTimer::isPaused() const {
    return state == TimerState::PAUSED;
}

bool PomodoroTimer::isStopped() const {
    return state == TimerState::STOPPED;
}

string PomodoroTimer::getStateString() const {
    switch (state) {
    case TimerState::STOPPED: return "已停止";
    case TimerState::RUNNING: return "工作中";
    case TimerState::PAUSED:  return "已暂停";
    case TimerState::BREAK:   return "休息中";
    default: return "未知";
    }
}

// ---------- 时间相关方法 ----------
int PomodoroTimer::getRemainingMinutes() const {
    lock_guard<std::mutex> lock(timeMutex);
    if (state == TimerState::RUNNING || state == TimerState::BREAK || state == TimerState::PAUSED) {
        return static_cast<int>(remainingTime.count() / 60);
    }
    return 0;
}

int PomodoroTimer::getRemainingSeconds() const {
    lock_guard<std::mutex> lock(timeMutex);
    if (state == TimerState::RUNNING || state == TimerState::BREAK || state == TimerState::PAUSED) {
        return static_cast<int>(remainingTime.count() % 60);
    }
    return 0;
}

int PomodoroTimer::getElapsedMinutes() const {
    lock_guard<std::mutex> lock(timeMutex);
    if (state == TimerState::RUNNING || state == TimerState::BREAK) {
        int total = workDuration.count();
        int remain = static_cast<int>(remainingTime.count());
        return (total - remain) / 60;
    }
    return 0;
}

int PomodoroTimer::getElapsedSeconds() const {
    lock_guard<std::mutex> lock(timeMutex);
    if (state == TimerState::RUNNING || state == TimerState::BREAK) {
        int total = workDuration.count();
        int remain = static_cast<int>(remainingTime.count());
        return (total - remain) % 60;
    }
    return 0;
}

int PomodoroTimer::getActualFocusSeconds() const {
    lock_guard<std::mutex> lock(timeMutex);
    return actualFocusSeconds;
}

int PomodoroTimer::getTotalWorkSeconds() const {
    return static_cast<int>(workDuration.count());
}

int PomodoroTimer::getTotalBreakSeconds() const {
    return static_cast<int>(breakDuration.count());
}

// ---------- 任务相关方法 ----------
void PomodoroTimer::setTask(const string& taskName) {
    currentTask = taskName;
}

string PomodoroTimer::getTaskName() const {
    return currentTask;
}

// ---------- 显示方法（彩色界面） ----------
void PomodoroTimer::displayTime() const {
    int minutes = getRemainingMinutes();
    int seconds = getRemainingSeconds();

    // \r 回行首 + \033[2K 清除整行，避免旧字符残留
    cout << "\r\033[2K";

    // 状态图标
    string color;
    switch (state) {
    case TimerState::RUNNING:
        color = C_GREEN;
        cout << C_GREEN << "⏳ 专注中";
        break;
    case TimerState::BREAK:
        color = C_CYAN;
        cout << C_CYAN << "☕ 休息中";
        break;
    case TimerState::PAUSED:
        color = C_YELLOW;
        cout << C_YELLOW << "⏸️ 已暂停";
        break;
    default:
        color = C_RESET;
        cout << C_RESET << "⏹️ 已停止";
    }

    cout << " [" << setw(2) << setfill('0') << minutes
        << ":" << setw(2) << setfill('0') << seconds << "]";

    if (!currentTask.empty() && (state == TimerState::RUNNING || state == TimerState::PAUSED)) {
        cout << C_RESET << " 任务: " << currentTask;
    }

    cout << C_RESET << flush;
}

// 进度显示
void PomodoroTimer::displayProgressBar() const {
    int total = 0;
    int elapsed = 0;

    lock_guard<std::mutex> lock(timeMutex);
    int remain = static_cast<int>(remainingTime.count());
    if (state == TimerState::RUNNING || state == TimerState::PAUSED) {
        total = workDuration.count();
        elapsed = total - remain;
    } else if (state == TimerState::BREAK) {
        total = breakDuration.count();
        elapsed = total - remain;
    }

    if (total <= 0) return;

    int width = 30;
    int progress = (elapsed * width) / total;

    cout << "\r[";
    for (int i = 0; i < width; i++) {
        if (i < progress) {
            cout << C_GREEN << "█";
        } else {
            cout << C_RESET << "░";
        }
    }
    cout << C_RESET << "] " << C_BOLD << (progress * 100 / width) << "%" << C_RESET << flush;
}

// 统计显示
void PomodoroTimer::displaySummary() const {
    cout << "\n" << C_BOLD << "========== 🍅 番茄钟统计 ==========" << C_RESET << endl;
    cout << "当前状态: " << getStateString() << endl;
    cout << "当前任务: " << (currentTask.empty() ? "无" : currentTask) << endl;
    cout << "工作时间: " << workDuration.count() / 60 << "分钟" << endl;
    cout << "休息时间: " << breakDuration.count() / 60 << "分钟" << endl;
    cout << "已完成番茄钟: " << completedPomodoros << " 个" << endl;

    if (state == TimerState::RUNNING || state == TimerState::BREAK || state == TimerState::PAUSED) {
        cout << "剩余时间: " << getRemainingMinutes() << "分" << getRemainingSeconds() << "秒" << endl;
    }
    cout << C_BOLD << "==================================" << C_RESET << endl;
}

// 设置工作时长
void PomodoroTimer::setWorkDuration(int minutes) {
    if (minutes > 0) {
        workDuration = seconds(minutes * 60);
    }
}

void PomodoroTimer::setBreakDuration(int minutes) {
    if (minutes > 0) {
        breakDuration = seconds(minutes * 60);
    }
}

int PomodoroTimer::getWorkDuration() const {
    return static_cast<int>(workDuration.count() / 60);
}

int PomodoroTimer::getBreakDuration() const {
    return static_cast<int>(breakDuration.count() / 60);
}

int PomodoroTimer::getCompletedPomodoros() const {
    return completedPomodoros;
}

void PomodoroTimer::resetStatistics() {
    completedPomodoros = 0;
}

// ---------- AI 自适应反馈相关接口 ----------
void PomodoroTimer::setFeedbackInterval(int minutes) {
    feedbackIntervalMinutes = minutes;
    lastFeedbackTime = steady_clock::now();
    nextFeedbackTime = steady_clock::now() + std::chrono::minutes(minutes);
}

int PomodoroTimer::getFeedbackInterval() const {
    return feedbackIntervalMinutes;
}

// 是否到了询问时刻（固定节奏：相比 nextFeedbackTime 判断，不受回答耗时影响）
bool PomodoroTimer::shouldAskFeedback() const {
    if (feedbackIntervalMinutes <= 0) return false;
    if (!(state == TimerState::RUNNING)) return false;
    if (feedbackPending) return false;

    auto now = steady_clock::now();
    return now >= nextFeedbackTime;
}

void PomodoroTimer::markFeedbackAsked() {
    // 固定节奏：下次询问时刻 = 本次应问时刻 + 间隔（不是"现在 + 间隔"）
    nextFeedbackTime += std::chrono::minutes(feedbackIntervalMinutes);
}

bool PomodoroTimer::isFeedbackPending() const {
    return feedbackPending;
}

// 设置反馈等待状态
// feedbackPending 默认由 applyFeedback 清除，此处为兼容其它调用
void PomodoroTimer::applyFeedback(FocusFeedback level) {
    // 清空等待状态并记录
    feedbackPending = false;
    lastFeedbackTime = steady_clock::now();
    if (!(state == TimerState::RUNNING)) return;

    totalFeedbackGiven++;

    if (level == FocusFeedback::VERY_FOCUSED) {
        veryFocusedCount++;
        extendedCount++;
        // 非常专注：延长本轮 5 分钟，背景变绿（专注）
        focusBgColor = "\033[42m"; // 绿底
        extendCurrentSession(5min);
        cout << "\n" << C_GREEN << C_BOLD << "✨ 状态很好！本轮自动延长 5 分钟。" << C_RESET << endl;
    } else if (level == FocusFeedback::NORMAL) {
        normalCount++;
        // 状态一般：背景变蓝（平稳）
        focusBgColor = "\033[44m"; // 蓝底
        cout << "\n" << C_YELLOW << "💪 保持节奏，继续专注。" << C_RESET << endl;
    } else if (level == FocusFeedback::DISTRACTED) {
        distractedCount++;
        earlyEnded = true;
        // 分心：提前结束本轮，背景变红（需要休息）
        focusBgColor = "\033[41m"; // 红底
        cout << "\n" << C_RED << C_BOLD << "🍃 有点分心，提前结束本轮，休息一下。" << C_RESET << endl;
        endSessionEarly();
    }
}

// 延长本轮
void PomodoroTimer::extendCurrentSession(chrono::seconds extra) {
    lock_guard<std::mutex> lock(timeMutex);
    if (state == TimerState::RUNNING) {
        endTime += extra;
        if (remainingTime.count() > 0) {
            remainingTime += extra;
        }
    }
}

// 提前结束本轮
void PomodoroTimer::endSessionEarly() {
    // 设置原子标志，后台倒计时线程检测后自行退出，避免主线程与后台线程抢同一把锁
    earlyStop = true;
}

// 本轮专注度（0.0 - 1.0），基于反馈分布
double PomodoroTimer::getFocusRatio() const {
    int total = veryFocusedCount + normalCount + distractedCount;
    if (total == 0) return 0.0;
    // 非常专注算 1.0，一般算 0.5，分心算 0.0
    return (veryFocusedCount * 1.0 + normalCount * 0.5) / total;
}

// 当前专注状态对应的背景色（ANSI 码）
std::string PomodoroTimer::getFocusBgColor() const {
    return focusBgColor;
}

int PomodoroTimer::getFeedbackTotal() const {
    return totalFeedbackGiven;
}

int PomodoroTimer::getVeryFocusedTotal() const {
    return veryFocusedCount;
}

int PomodoroTimer::getNormalTotal() const {
    return normalCount;
}

int PomodoroTimer::getDistractedTotal() const {
    return distractedCount;
}

bool PomodoroTimer::wasEarlyEnded() const {
    return earlyEnded;
}

bool PomodoroTimer::wasExtended() const {
    return extendedCount > 0;
}
