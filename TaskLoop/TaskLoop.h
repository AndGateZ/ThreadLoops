#ifndef TASK_LOOP_H
#define TASK_LOOP_H

#include <memory>
#include <list>
#include <unordered_map>
#include <string>
#include <thread>
#include <mutex>

#include "TaskDef.h"

class TimeCmp final {
public:
    bool operator()(std::pair<std::shared_ptr<Task>,uint64_t> const& p1,std::pair<std::shared_ptr<Task>,uint64_t> const& p2) noexcept {
        if (p1.second <= p2.second) {
            return true;
        }
        return false;
    }
};

class TaskLoop final{
public:
    TaskLoop(std::string const& thread_name="");
    bool Start();
    void Stop();

public:
    void PostTask(std::shared_ptr<Task> const& task);
    void PostDelayTask(std::shared_ptr<Task> const& task, uint64_t delay_ms);
    void RemoveDelayTask(std::shared_ptr<Task> const& task);
    void PostRepeatTask(std::shared_ptr<Task> const& task, uint64_t repeat_ms);
    void RemoveRepeatTask(std::shared_ptr<Task> const& task);
    
private:
    void Run();
    void DealTask(std::list<std::shared_ptr<Task>> &task_list);
    void DealDelayTask(std::list<std::shared_ptr<Task>> &task_list);
    
    void Wakeup();
    void DealWakeUp();

    void GetTimeOutTask(std::list<std::shared_ptr<Task>> &task_list);
    int32_t GetNextExpiredTime();
    static uint64_t GetCurTimeMs(); 

private:
    bool stop_running_;

    std::list<std::shared_ptr<Task>> task_list_;
    std::list<std::pair<std::shared_ptr<Task>,uint64_t>> delay_task_list_;
    std::unordered_map<std::shared_ptr<Task>,uint64_t> repeat_task_;

    std::mutex task_list_mutex_;
    std::mutex delay_task_list_mutex_;

    std::shared_ptr<std::thread> thread_;

    int32_t wakeup_fd_;
};

#endif