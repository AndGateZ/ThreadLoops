#ifndef TASK_LOOP_H
#define TASK_LOOP_H

#include <memory>
#include <list>
#include <string>
#include <thread>
#include <mutex>

#include "TaskDef.h"

class TaskLoop final{
public:
    TaskLoop(std::string const& thread_name="");
    bool Start();
    void Stop();

public:
    void PostTask(std::shared_ptr<Task> const& task);

    void AddWatch(std::shared_ptr<Event> const& evt);
    void RemoveWatch(std::shared_ptr<Event> const& evt);

private:
    void Run();
    void DealTask(std::list<std::shared_ptr<Task>> &task_list);
    
    void Wakeup();
    void DealWakeUp();

private:
    bool stop_running_;

    std::list<std::shared_ptr<Task>> task_list_;
    std::mutex task_list_mutex_;

    std::shared_ptr<std::thread> thread_;

    int32_t wakeup_fd_;
    int32_t epoll_fd_;
};

#endif
