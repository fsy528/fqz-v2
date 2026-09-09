// 番茄钟头文件（负责人：闫成举）— AI 自适应增强版
#ifndef POMODORO_TIMER_H
#define POMODORO_TIMER_H

#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>
#include <iomanip>

// 专注反馈等级
enum class FocusFeedback { NONE = 0, VERY_FOCUSED = 1, NORMAL = 2, DISTRACTED = 3 };

class PomodoroTimer {
private:
    // 计时器状态
    enum class TimerState { STOPPED, RUNNING, PAUSED, BREAK };

    TimerState state;                      // 当前状态
    std::chrono::seconds workDuration;     // 工作时间
    std::chrono::seconds breakDuration;    // 休息时间
    std::chrono::seconds remainingTime;    // 剩余时间
    std::atomic<bool> stopFlag;            // 停止标志
    std::atomic<bool> pausedFlag;          // 暂停标志
    std::thread timerThread;               // 倒计时线程
    mutable std::mutex timeMutex;          // 保护剩余时间/结束点

    std::chrono::steady_clock::time_point endTime; // 本轮结束点（可被延长/提前）
    std::string currentTask;               // 当前任务名称
    std::atomic<bool> earlyStop;           // 提前结束标志（分心时由主线程设置，后台线程自行退出）

    // ---- AI 自适应反馈相关（新增） ----
    int feedbackIntervalMinutes;                           // 反馈询问间隔（分钟），0 表示不询问
    std::chrono::steady_clock::time_point lastFeedbackTime; // 上次询问时间
    std::chrono::steady_clock::time_point nextFeedbackTime; // 下一次应询问的绝对时刻（固定节奏，不受回答耗时影响）
    bool feedbackPending;                  // 本次询问是否等待回答
    int veryFocusedCount;                  // 本轮“非常专注”次数
    int normalCount;                       // 本轮“状态一般”次数
    int distractedCount;                   // 本轮“分心”次数
    bool earlyEnded;                       // 是否因分心提前结束
    int extendedCount;                     // 是否因专注被延长
    int totalFeedbackGiven;                // 本轮收集到的反馈总次数
    int actualFocusSeconds;                // 本轮真实专注秒数（随经过时间累加，延长不影响）
    std::string focusBgColor;              // 当前反馈对应的背景色（ANSI 背景色码，随专注状态变化）

    // 私有方法
    void runCountdown(std::chrono::seconds duration);
    void workPhase();
    void breakPhase();
    void initialize();
    void cleanup();

public:
    // 构造函数
    PomodoroTimer();
    PomodoroTimer(int workMinutes, int breakMinutes);
    ~PomodoroTimer();  // 析构函数，确保线程安全结束

    // 核心控制方法
    void startWork(const std::string& taskName = "", int minutes = 25);
    void startBreak(int minutes = 5);
    void pause();
    void resume();
    void reset();
    void update();

    // 查询方法
    bool isRunning() const;
    bool isOnBreak() const;
    bool isPaused() const;
    bool isStopped() const;
    std::string getStateString() const;

    // 时间相关
    int getRemainingMinutes() const;
    int getRemainingSeconds() const;
    int getElapsedMinutes() const;
    int getElapsedSeconds() const;
    int getTotalWorkSeconds() const;
    int getTotalBreakSeconds() const;
    int getActualFocusSeconds() const;   // 本轮真实专注秒数

    // 任务相关
    void setTask(const std::string& taskName);
    std::string getTaskName() const;

    // 显示方法
    void displayTime() const;
    void displayProgressBar() const;
    void displaySummary() const;

    // 配置方法
    void setWorkDuration(int minutes);
    void setBreakDuration(int minutes);
    int getWorkDuration() const;
    int getBreakDuration() const;

    // 统计方法
    int getCompletedPomodoros() const;
    void resetStatistics();

    // ---- AI 自适应反馈相关接口（新增） ----
    void setFeedbackInterval(int minutes);          // 设置反馈询问间隔（0=关闭）
    int  getFeedbackInterval() const;               // 获取当前反馈间隔
    bool shouldAskFeedback() const;                 // 是否到了询问时刻
    void markFeedbackAsked();                       // 标记已询问
    bool isFeedbackPending() const;                 // 是否等待回答
    void applyFeedback(FocusFeedback level);        // 应用用户反馈（实时调整时长）
    void extendCurrentSession(std::chrono::seconds extra); // 延长本轮
    void endSessionEarly();                          // 提前结束本轮
    double getFocusRatio() const;                    // 本轮专注度（0.0-1.0）
    int getFeedbackTotal() const;                    // 反馈总次数
    int getVeryFocusedTotal() const;
    int getNormalTotal() const;
    int getDistractedTotal() const;
    bool wasEarlyEnded() const;                      // 是否提前结束
    bool wasExtended() const;                        // 是否被延长
    std::string getFocusBgColor() const;             // 当前专注状态对应的背景色（ANSI 码）
};

#endif
