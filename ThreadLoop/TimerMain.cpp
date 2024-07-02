#include "TimerMng.h"

#include <iostream>
#include <thread>
#include <chrono>

void func1(){
    std::cout << "Do func1" << std::endl;
}

void func2(){
    std::cout << "Do func2" << std::endl;
}

int main(){
    std::shared_ptr<TimerMng> timerMng{std::make_shared<TimerMng>()};
    if(!timerMng->Init()){
        std::cout << "Init Failed" << std::endl;
    }

    std::shared_ptr<Task> task1{std::make_shared<Task>(func1,"func1")};
    std::shared_ptr<Task> task2{std::make_shared<Task>(func2,"func2")};
    timerMng->AddTimer(task1,2000,true);
    timerMng->AddTimer(task2,1000);
    timerMng->AddTimer(task1,2000);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
    timerMng->RemoveTimer(task2);

    std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
    timerMng->DeInit();
    return 0;
}

//1. is.it_value为0表示不触发，非零值才启动
//2. 需要使用EPOLLLT来监听timer，如果是EPOLLET，在epollwait监听之前的活动就丢失了