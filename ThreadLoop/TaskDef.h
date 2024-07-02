#ifndef TASK_DEF_H
#define TASK_DEF_H

#include <string>
#include <functional>
#include <iostream>

class Task final{
public:
  Task(std::function<void()> const& func,std::string const& task_name="")
    :is_running_{false},
    have_run_{false},
    task_name_{task_name},
    func_(func){}

public:
    bool IsRunning(){return is_running_;}
    bool HaveRun(){return have_run_;}
    std::string GetTaskName(){return task_name_;}

    void DoTask(){
        is_running_ = true;
        func_();
        have_run_ = true;
        is_running_ = false;
    }

private:
    bool is_running_;
    bool have_run_;
    std::string task_name_;
    std::function<void()> func_;
};


struct Event{
    Event(int32_t fd_in,int32_t evt,std::function<void(Event*)> fun)
    :fd(fd_in),
    event(evt),
    func(fun){}
    
    int32_t fd;
    int32_t event;
    std::function<void(Event*)> func;
};

#endif
