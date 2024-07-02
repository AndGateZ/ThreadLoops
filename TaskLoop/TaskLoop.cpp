#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>

#include "TaskLoop.h"

TaskLoop::TaskLoop(std::string const& thread_name)
    :stop_running_{true},
    task_list_{},
    delay_task_list_{},
    repeat_task_{},
    task_list_mutex_{},
    delay_task_list_mutex_{},
    thread_{nullptr},
    wakeup_fd_{-1}{
        wakeup_fd_ = eventfd(0U, EFD_NONBLOCK);
    }

bool TaskLoop::Start(){
    stop_running_ = false;
    thread_ = std::make_shared<std::thread>(std::bind(&TaskLoop::Run,this));
    if(thread_ == nullptr){
        stop_running_ = true;
        return false;
    }
    return true;
}

void TaskLoop::Stop(){
    if(thread_ == nullptr){
        return;
    }
    stop_running_ = true;
    Wakeup();
    if(thread_->joinable()){
        thread_->join();
    }
    thread_ = nullptr;
}

void TaskLoop::PostTask(std::shared_ptr<Task> const& task){
    if(task == nullptr){
        return;
    }
    std::lock_guard<std::mutex> locker{task_list_mutex_};
    task_list_.emplace_back(task);
    Wakeup();
}

void TaskLoop::PostDelayTask(std::shared_ptr<Task> const& task, uint64_t delay_ms){
    if(task == nullptr){
        return;
    }
    std::lock_guard<std::mutex> locker{delay_task_list_mutex_};
    auto pair{std::make_pair(task, delay_ms+GetCurTimeMs())};
    auto it = std::lower_bound(delay_task_list_.begin(), delay_task_list_.end(), pair, TimeCmp());
    delay_task_list_.insert(it, pair);
    Wakeup();
}

void TaskLoop::RemoveDelayTask(std::shared_ptr<Task> const& task){
    if(task == nullptr){
        return;
    }
    std::lock_guard<std::mutex> locker{delay_task_list_mutex_};
    for(auto it{delay_task_list_.begin()}; it!=delay_task_list_.end();){
        if(it->first == task){
            it = delay_task_list_.erase(it);
            break;
        }
    }
}
    
void TaskLoop::PostRepeatTask(std::shared_ptr<Task> const& task, uint64_t repeat_ms){
    if(task == nullptr){
        return;
    }
    if(repeat_ms <= 10U){
        std::cout<< "repeat_ms to small: "<< repeat_ms <<std::endl;
        return;
    }

    std::lock_guard<std::mutex> locker{delay_task_list_mutex_};
    auto pair{std::make_pair(task, repeat_ms+GetCurTimeMs())};
    auto it = std::lower_bound(delay_task_list_.begin(), delay_task_list_.end(), pair, TimeCmp());
    delay_task_list_.insert(it, pair);
    repeat_task_[task] = repeat_ms;
    Wakeup();
}

void TaskLoop::RemoveRepeatTask(std::shared_ptr<Task> const& task){
    if(task == nullptr){
        return;
    }
    std::lock_guard<std::mutex> locker{delay_task_list_mutex_};
    for(auto it{delay_task_list_.begin()}; it!=delay_task_list_.end();){
        if(it->first == task){
            it = delay_task_list_.erase(it);
            break;
        }
    }
    auto it{repeat_task_.find(task)};
    if(it != repeat_task_.end()){
        repeat_task_.erase(it);
    }
}

void TaskLoop::Run(){
    while(!stop_running_){
        {
            std::lock_guard<std::mutex> locker{task_list_mutex_};
            std::list<std::shared_ptr<Task>> tmp_task_list{};
            tmp_task_list.swap(task_list_);
            DealTask(tmp_task_list);
        }
        {
            std::lock_guard<std::mutex> locker{delay_task_list_mutex_};
            std::list<std::shared_ptr<Task>> tmp_task_list{};
            GetTimeOutTask(tmp_task_list);
            DealDelayTask(tmp_task_list);
        }

        int32_t next_time{GetNextExpiredTime()};
        std::cout<< "next_time: " << next_time <<std::endl;

        struct pollfd fds{};
        fds.fd = wakeup_fd_;
        fds.events = POLLIN | POLLERR  | POLLHUP;
        fds.revents = 0;
        nfds_t nfds{1};
        int32_t ret{poll(&fds, nfds, next_time)};
        if(ret>0){
            DealWakeUp();
        }else{
            std::cout<< "poll failed or timeout, ret: " << ret <<std::endl;
        }
    }
    close(wakeup_fd_);
    std::cout<< "thread loop end" <<std::endl;
}

void TaskLoop::DealTask(std::list<std::shared_ptr<Task>> &task_list){
    for(auto it{task_list.begin()}; it!=task_list.end(); ){
        (*it)->DoTask();
        it = task_list.erase(it);
    }
}

void TaskLoop::DealDelayTask(std::list<std::shared_ptr<Task>> &task_list){
    for(auto it{task_list.begin()}; it!=task_list.end(); ){
        (*it)->DoTask();
        std::shared_ptr<Task> task{*it};
        it = task_list.erase(it);
        if(repeat_task_.find(task) != repeat_task_.end()){
            uint64_t repeat_ms{repeat_task_[task]};
            auto pair{std::make_pair(task, repeat_ms + GetCurTimeMs())};
            auto it_insert = std::lower_bound(delay_task_list_.begin(), delay_task_list_.end(), pair, TimeCmp());
            delay_task_list_.insert(it_insert, pair);
        }
    }
}

void TaskLoop::Wakeup(){
    uint64_t data{1U};
    ssize_t ret{write(wakeup_fd_,&data,sizeof(uint64_t))};
    if(ret != sizeof(uint64_t)){
        std::cout<< "Wakeup failed" <<std::endl;
    }
    std::cout<< "Wakeup" <<std::endl;
}

void TaskLoop::DealWakeUp(){
    uint64_t data{0U};
    ssize_t ret{read(wakeup_fd_, &data,sizeof(uint64_t))};
    if(ret != sizeof(uint64_t)){
        std::cout<< "DealWakeUp failed" <<std::endl;
    }
    std::cout<< "DealWakeUp" <<std::endl;
}

void TaskLoop::GetTimeOutTask(std::list<std::shared_ptr<Task>> &task_list){
    uint64_t cur_time{GetCurTimeMs()};
    for(auto it{delay_task_list_.begin()}; it!=delay_task_list_.end();){
        if(cur_time >= it->second){
            task_list.emplace_back(it->first);
            it = delay_task_list_.erase(it);
        }else{
            break;
        }  
    }
}

int32_t TaskLoop::GetNextExpiredTime(){
    if(delay_task_list_.empty()){
        return -1;
    }
    uint64_t cur_time_ms{GetCurTimeMs()};
    if(delay_task_list_.begin()->second <= cur_time_ms){
        std::cout<< "GetNextExpiredTime error" <<std::endl;
        return 0;
    }
    return delay_task_list_.begin()->second - cur_time_ms;
}

uint64_t TaskLoop::GetCurTimeMs(){
    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}