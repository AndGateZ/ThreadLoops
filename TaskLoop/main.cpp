#include "TaskLoop.h"

#include <iostream>
#include <thread>
#include <chrono>

static void func1(){
    std::cout << "func1 start" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::cout << "func1 stop" << std::endl;
}

static void func2(){
    std::cout << "func2 start" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::cout << "func2 stop" << std::endl;
}

static void func3(){
    std::cout << "func3 start" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::cout << "func3 stop" << std::endl;
}


int main(){
    TaskLoop loop{};
    loop.Start();

    std::shared_ptr<Task> task1{std::make_shared<Task>(func1)};
    std::shared_ptr<Task> task2{std::make_shared<Task>(func2)};
    std::shared_ptr<Task> task3{std::make_shared<Task>(func3)};

    loop.PostTask(task1);
    loop.PostDelayTask(task2,5000);
    loop.PostRepeatTask(task3,3000);

    std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
    loop.RemoveRepeatTask(task3);
    std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
    loop.Stop();
    
    return 0;
}