#include <sys/timerfd.h>
#include <stdio.h>
#include <cmath>

#include "TimerMng.h"

TimerMng::TimerMng()
    :timerFd_map_(),
    taskLoop_(nullptr),
    mu_(){}

bool TimerMng::Init(){
    taskLoop_ = std::make_shared<TaskLoop>();
    if(!taskLoop_->Start()){
        return false;
    }
    return true;
}

void TimerMng::DeInit(){
    if(taskLoop_!=nullptr){
        for(auto it{timerFd_map_.begin()}; it != timerFd_map_.end(); it++){
            taskLoop_->RemoveWatch(it->second);
            timerFd_map_.erase(it->first);
        }
        taskLoop_->Stop();
        taskLoop_.reset();
    }
}

void TimerMng::AddTimer(std::shared_ptr<Task> task, uint32_t interval_ms, bool trigger_now){
    std::lock_guard<std::mutex> locker{mu_};
    std::string tsak_name{task->GetTaskName()};
    if(timerFd_map_.find(tsak_name) != timerFd_map_.end()){
        std::cout << "AddTimer task already exist: " << tsak_name << std::endl;
        return;
    }
    int32_t timer_fd{CreateTimerFd(interval_ms)};
    std::shared_ptr<Event> evt{std::make_shared<Event>(timer_fd, EPOLLIN, std::bind(&TimerMng::OnTimerWakeUp, this, timer_fd, task))};
    taskLoop_->AddWatch(evt);
    // std::shared_ptr<Task> task_trigger_timer{std::make_shared<Task>(std::bind(&TimerMng::SetTimerFd,this,timer_fd,interval_ms,trigger_now))};
    if(trigger_now){
        taskLoop_->PostTask(task);
    }
    timerFd_map_[tsak_name] = evt;
}

void TimerMng::RemoveTimer(std::shared_ptr<Task> task){
    std::lock_guard<std::mutex> locker{mu_};
    std::string tsak_name{task->GetTaskName()};
    if(timerFd_map_.find(tsak_name) == timerFd_map_.end()){
        std::cout << "RemoveTimer task not exist: " << tsak_name << std::endl;
        return;
    }
    std::shared_ptr<Event> evt{timerFd_map_[tsak_name]};
    taskLoop_->RemoveWatch(evt);
    close(evt->fd);
    timerFd_map_.erase(tsak_name);
}

int32_t TimerMng::CreateTimerFd(uint32_t interval_ms){
    int32_t timer_fd{timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)};
    if(timer_fd <=0){
        std::cout << "CreateTimerFd failed" << std::endl;
        return timer_fd;
    }

    uint64_t nsec{interval_ms * 1000 * 1000};
    uint64_t sec{interval_ms / 1000};
    nsec = nsec - sec*1000*1000;

    struct itimerspec is{};
    is.it_interval.tv_sec = sec;
    is.it_interval.tv_nsec = nsec;
    is.it_value.tv_sec = sec;
    is.it_value.tv_nsec = nsec;
    int32_t ret{timerfd_settime(timer_fd, 0, &is, nullptr)};
    if(0 != ret){
        std::cout << "timerfd_settime failed: " << ret << std::endl;
        std::cout << "errno: " << errno << std::endl;
    }

    return timer_fd;
}

void TimerMng::OnTimerWakeUp(int32_t timer_fd, std::shared_ptr<Task> task){
    uint64_t data{0U};
    ssize_t ret{read(timer_fd, &data,sizeof(uint64_t))};
    if(ret != sizeof(uint64_t)){
        std::cout<< "OnTimerWakeUp failed" <<std::endl;
    }
    task->DoTask();
}