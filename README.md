# 背景
在日常工作和学习源码过程中，经常可以看到ThreadLoop的运用，发现ThreadLoop作为一个基础工具，在具体项目中有不同而又十分相似的实现，虽然核心的机制万变不离其宗（IO多路复用），但面向的业务场景不同导致了不同的实践结果，目前见过有几种ThreadLoop的实践，本文做一个分析记录和知识点的总结

# 基础TaskLoop：
1. 面向Task：  
  Task类型包裹回调函数和必要数据，如have_run,is_run，task_tag等，TaskLoop负责执行Task，实质是处理Task类型包裹的回调函数
   - 核心接口：PostTask(Task)、PostDelayTask(Task)、PostRepeatTask(Task)
   - 功能：用户将Task/Event交给线程进行异步处理，同时实现简单的Timer机制，对任务触发的时间进行延迟或重复
   - 面向场景：工作线程执行Task
   - 缺点：仅能对Task进行处理，无法充分使用多路复用机制监听Socket或其他fd
   - 代码：TaskLoop/TaskLoop.h

2. 面向Event：  
  Event类型和Task相比，拥有了事件的语义，Event事件=需要监听的触发源+事件处理回调，一般来说触发源设计为Event Fd可以完美适配IO多路复用机制，同时仅监听Event fd可以给上层组件足够的灵活性，可以认为这是面向事件ThreadLoop很优雅的设计方法了
    - 核心接口：在面向Task的基础上，增加对事件的监听AddWatch(Event)
    - 功能：提供文件描述符监听的功能
    - 面向场景：事件驱动，需要监听文件描述符，例如Socket、Timer，上层仅需设计自己需要的Event即可实现定制化需求，例如进阶TaskLoop中的Timer就是很好的例子
    - 具体实践：  
    epoll+Event，基于epoll_data.ptr的指针完成监听事件的处理，在Event中设定各类事件（或指定事件）的回调，muduo的channel就是基于这种机制设计的
    - 代码：ThreadLoop/TaskLoop.h
    - 注意：  
    Event类可以设计为基类，事件处理回调OnEventCallback可以通过识别Event的不同类型做事件的分发，完成Event内信息的传递，后续也可以通过基类->子类的转换，实现更多的任务处理和信息传递能力

    - 讨论1：除了poll多路复用+wakeupFd可以用于实现任务/事件队列，相似的也可以使用条件变量的方式做事件的同步，但条件变量有唤醒丢失和虚假唤醒等问题，相较而言FD的多路复用监听机制更加稳定，同时基于文件描述符监听的方式，可以增加基于Timer FD的功能，可拓展性更强
    - 讨论2：在具体的实践中，只要能完成实际的业务需求也可以不用解耦，可直接将业务逻辑回调写到secket fd触发后的逻辑中，设计Event来提供监听的统一接口是为了通用性，提供可复用接口给其他模块

# 进阶TaskLoop
1. MsgQueue：TaskLoop+queue  
  基于TaskLoop，增加队列queue可实现一个异步的消息队列，提供PostMsg(msg)接口，MsgQueue初始化时绑定消息处理函数init(callback)，
    - 核心原理：MsgQueue内部有一个事件触发的Event，绑定到TaskLoop中进行监听，当用户调用PostMsg(msg)向内部的queue压入消息时，主动唤醒Event FD来wakeup TaskLoop监听中的文件描述符，在Event唤醒的回调内取出queue挤压的msg，调用提前绑定好的callback，完成消息的异步处理
    - 核心接口：PostMsg(msg)
    - 面向场景：异步消息处理队列
    - 代码：ThreadLoop/MsgQueue.h
2. Timer：TaskLoop+Timer Fd  
  基于面向Event的TaskLoop，在Timer的场景下，AddWatch(Event)实质上是AddWatch(TimerEventFd)，因此只需要在AddWatch(Event)的基础上做TimerFd创建的封装即可
    - 核心接口：AddTimer(Task)
    - 面向场景：定时器任务处理
    - 代码：ThreadLoop/TimerMng.h
3. ThreadLoopMng  
线程池，维护多个ThreadLoop的生命周期，派发任务
    - 核心接口：TaskLoop* GetTaskLoop()；
    - 面向场景：线程池管理工作线程，维护线程生命周期
