#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <queue>
#include <mutex>
#include <functional>
#include <memory>

#include "TaskLoop.h"
#include "TaskDef.h"

template <typename T>
class MsgQueue final{
public:
    MsgQueue()
    :evt_(nullptr),
    taskLoop_(nullptr),
    wakeup_fd_(-1),
    que_(),
    mu_(),
    func_(){}

    bool Init(std::function<void(const T&)> func){
        wakeup_fd_ = eventfd(0U, EFD_NONBLOCK);
        func_ = func;
        taskLoop_ = std::make_shared<TaskLoop>();
        if(!taskLoop_->Start()){
            return false;
        }
        evt_ = std::make_shared<Event>(wakeup_fd_, EPOLLIN, std::bind(&MsgQueue::onWakeup,this));
        taskLoop_->AddWatch(evt_);
        return true;
    }

    void DeInit(){
        if(taskLoop_!=nullptr){
            taskLoop_->RemoveWatch(evt_);
            taskLoop_->Stop();
            taskLoop_.reset();
        }
        if(wakeup_fd_ != -1){
            close(wakeup_fd_);
        }
        evt_.reset();
    }

    void PostMsg(const T& msg){
        std::lock_guard<std::mutex> locker_{mu_};
        //Todo: max que size
        que_.push(msg);
        Wakeup();
    }

private:
    void Wakeup(){
        uint64_t data{1U};
        ssize_t ret{write(wakeup_fd_,&data,sizeof(uint64_t))};
    }

    void onWakeup(){
        uint64_t data{0U};
        ssize_t ret{read(wakeup_fd_, &data,sizeof(uint64_t))};

        std::queue<T> que_tmp_{};
        {
            std::lock_guard<std::mutex> locker_{mu_};
            que_.swap(que_tmp_);
        }

        if(que_tmp_.empty()){
            return;
        }

        while(!que_tmp_.empty()){
            auto msg{que_tmp_.front()};
            que_tmp_.pop();
            func_(msg);
        }
    }

private:
    std::shared_ptr<Event> evt_;
    std::shared_ptr<TaskLoop> taskLoop_;
    int32_t wakeup_fd_;
    std::queue<T> que_;
    std::mutex mu_;
    std::function<void(const T&)> func_;
};

#endif
