// 数据存储头文件（负责人：李罡）— AI 自适应增强版
#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include "task_manager.h"
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

// 专注记录结构（用于时间利用画像与统计分析）
struct FocusRecord {
    std::string startTime;      // 开始时间
    std::string endTime;        // 结束时间
    std::string taskName;       // 任务名称
    int plannedMinutes;         // 计划时长
    int actualMinutes;          // 实际时长（0 表示未记录）
    int feedbackLevel;          // 反馈等级：1=非常专注 2=一般 3=分心 0=未反馈
    bool earlyEnded;            // 是否提前结束
};

class DataStorage {
public:
    // 任务文件操作
    static bool loadTasksFromFile(const std::string& filename, std::vector<Task>& tasks);
    static bool saveTasksToFile(const std::string& filename, const std::vector<Task>& tasks);

    // 日志文件操作
    static void recordPomodoro(const std::string& taskName, int duration);
    // 增强版：记录完整专注会话（开始/结束时间、计划/实际时长、反馈等级、是否提前结束）
    static void recordFocusSession(const std::string& taskName,
                                   int plannedMinutes,
                                   int actualMinutes,
                                   int feedbackLevel,
                                   bool earlyEnded,
                                   const std::string& startTime);
    static void showStatistics();

    // 智能解析
    static Task parseInput(const std::string& input, int& nextId);

    // 辅助函数
    static std::string getCurrentDate();
    static std::string getCurrentDateTime();
    static std::string timeToString(const std::chrono::system_clock::time_point& time);
    static std::chrono::system_clock::time_point stringToTime(const std::string& timeStr);

    // 统计信息
    static int getTodayPomodoros();
    static int getTotalPomodoros();
    static double getAverageFocusTime();

    // ---- 新增：AI 自适应与时间利用画像 ----
    // 读取全部专注记录
    static std::vector<FocusRecord> loadFocusRecords();
    // 计算个性化推荐专注时长（基于历史平均实际专注时长）
    static int getRecommendedFocusDuration();
    // 显示时间利用画像（一天中专注时段分布、专注度、提前结束率）
    static void showFocusProfile();
    // 显示"专注 - 休息"时间分布（何时工作、何时休息）
    static void showTimeDistribution();

private:
    // 私有常量
    static const std::string TASK_FILE_HEADER;
    static const std::string LOG_FILE_NAME;

    // 解析辅助函数
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::string trim(const std::string& str);
    static bool isValidDate(const std::string& dateStr);
};

#endif
