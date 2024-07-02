#include <poll.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <array>

#include "TaskLoop.h"

TaskLoop::TaskLoop(std::string const& thread_name)
    :stop_running_{true},
    task_list_{},
    task_list_mutex_{},
    thread_{nullptr},
    wakeup_fd_{-1},
    epoll_fd_{-1}{
        wakeup_fd_ = eventfd(0U, EFD_NONBLOCK);
        epoll_fd_ = epoll_create(16);
    }

bool TaskLoop::Start(){
    stop_running_ = false;
    struct epoll_event evt{};
    evt.events = POLLIN | POLLERR  | POLLHUP;
    evt.data.fd = wakeup_fd_;
    int32_t ret{epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &evt)};
    if(ret != 0 ){
        return false;
    }
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

void TaskLoop::AddWatch(std::shared_ptr<Event> const& evt){
    auto lamda{[=](){
            struct epoll_event evt_listen{};
            evt_listen.events = evt->event;
            evt_listen.data.ptr = (void*)evt.get();
            int32_t ret{epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, evt->fd, &evt_listen)};
        }
    };
    std::shared_ptr<Task> task{std::make_shared<Task>(lamda)};
    PostTask(task);
}

void TaskLoop::RemoveWatch(std::shared_ptr<Event> const& evt){
    auto lamda{[=](){
            int32_t ret{epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, evt->fd, nullptr)};
        }
    };
    std::shared_ptr<Task> task{std::make_shared<Task>(lamda)};
    PostTask(task);
}

void TaskLoop::Run(){
    while(!stop_running_){
        {
            std::lock_guard<std::mutex> locker{task_list_mutex_};
            std::list<std::shared_ptr<Task>> tmp_task_list{};
            tmp_task_list.swap(task_list_);
            DealTask(tmp_task_list);
        }

        std::array<struct epoll_event,16> evts{};
        int32_t nready{epoll_wait(epoll_fd_, evts.data(), 16, -1)};
        if(nready<=0){
            std::cout<< "poll failed, nready: " << nready <<std::endl;
            continue;
        }
        
        for(int32_t i{0};i<nready;++i){
            if (evts[i].data.fd == wakeup_fd_) {
                DealWakeUp();
                continue;
            }
            std::cout << "event trigger" << std::endl;
            Event* evt{static_cast<Event*>(evts[i].data.ptr)};
            if(evt->event != evts[i].events){
                std::cout << "event trigger which is not listen" << std::endl;
                continue;
            }
            evt->func(evt);
        }
    }
    close(wakeup_fd_);
    close(epoll_fd_);
    wakeup_fd_ = -1;
    std::cout<< "thread loop end" <<std::endl;
}

void TaskLoop::DealTask(std::list<std::shared_ptr<Task>> &task_list){
    for(auto it{task_list.begin()}; it!=task_list.end(); ){
        (*it)->DoTask();
        it = task_list.erase(it);
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
