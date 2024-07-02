#include "MsgQueue.h"

#include <iostream>
#include <thread>
#include <chrono>

struct Msg{
    int32_t type;
    int32_t data;
};

static MsgQueue<Msg> que{};

void DealMsg(const Msg& msg){
    std::cout << "msgtype: " <<msg.type << " msgdata: " << msg.data << std::endl;
}

void makeMsg(){
    for(int i{0};i<10;++i){
        Msg msg{5,i};
        que.PostMsg(msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(){
    bool ret{que.Init(std::bind(DealMsg,std::placeholders::_1))};
    std::cout << "init ret: " <<ret << std::endl;

    std::thread th1{makeMsg};
    std::thread th2{makeMsg};
    th1.join();
    th2.join();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50*1000));
    que.DeInit();
    return 0;
}
