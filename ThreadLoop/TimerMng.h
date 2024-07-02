#ifndef TIMER_MNG_H
#define TIMER_MNG_H

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <mutex>
#include <functional>
#include <memory>
#include <unordered_map>

#include "TaskLoop.h"
#include "TaskDef.h"

class TimerMng final{
public:
    TimerMng();

    bool Init();
    void DeInit();

public:
    void AddTimer(std::shared_ptr<Task> task, uint32_t interval_ms, bool trigger_now = false);
    void RemoveTimer(std::shared_ptr<Task> task);

private:
    int32_t CreateTimerFd(uint32_t interval_ms);
    void OnTimerWakeUp(int32_t timer_fd, std::shared_ptr<Task> task);

private:
    std::unordered_map<std::string,std::shared_ptr<Event>> timerFd_map_; //taskname - Event
    std::shared_ptr<TaskLoop> taskLoop_;
    std::mutex mu_;
};

#endif
