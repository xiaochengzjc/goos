#include "Sysmon.h"
#include <thread>
#include <sys/signal.h>
#include "Proc.h"
#include "Coroutine.h"
#include "Log.h"

thread Sysmon::_m;

/**
 * 当前线程的信号处理
 * @param signo
 */
void Sysmon::sighandler(int signo)
{
    //不可靠信号需要重新安装
    regsig();
    Coroutine *co = GO_ZG(_g);
    Debug("receive signal:%d _g:%ld co->status:%d Grunnable:%d",signo,co,co->gstatus,Grunnable);
    //判断当前G是否状态正常
    if(co != nullptr && co->gstatus == Grunnable){
        //抢占切出
        GO_ZG(_g)->stackpreempt();
    }
    return;
}
/**
 * 线程信号处理
 * @param signo
 */
void Sysmon::regsig()
{
    struct sigaction actions;
    sigemptyset(&actions.sa_mask);
    /* 将参数set信号集初始化并清空 */
    actions.sa_flags = -1;
    actions.sa_handler = sighandler;
    sigaction(SIGURG,&actions,NULL);
}
/**
 * 对该线程发送信号，进行抢占调度
 * @param m
 */
void Sysmon::preemptPark(M *m)
{
    //同步两边状态
    m->tick = 0;
    GO_FETCH(m->_m,schedtick) = 0;
    Coroutine *co  = GO_FETCH(m->_m,_g);
    //发起抢占前判断G是否正常
    Debug("start park preempt: _g:%ld co->gstatus:%d Grunnable:%d",co,co->gstatus,Grunnable);
    if(co != nullptr && co->gstatus == Grunnable){
        Debug("start send signal:%d",SIGURG);
        pthread_kill(m->tid, SIGURG);
    }

}
/**
 * 都该线程进行周期和持有时间进行检查
 * 如果超过10ms则需要标记为抢占
 * @param m
 */
void Sysmon::preemptM(M *m)
{
    //检查周期是否一致
    if(m->tick != GO_FETCH(m->_m,schedtick)){
        Debug("period not consistent");
        m->tick = GO_FETCH(m->_m,schedtick);
        return;
    }
    //检查是否超时 上次时间+10ms 如果还小于当前时间
    auto prev = GO_FETCH(m->_m,schedwhen);
    auto now  = chrono::steady_clock::now();
    int timeout = chrono::duration<double,std::milli>(now-prev).count();
    //如果大于20ms 则需要执行抢占 php初始化比较消耗时间 10ms可能不够
    if(timeout > 20){
        Debug("over 20ms: start call preempt park");
        preemptPark(m);
    }
//    }else{
//        m->tick ++;
//    }
}
/**
 * 监控线程
 */
void Sysmon::monitor()
{
    Debug("sysmon start");
    //等待线程M加载完
    while (proc->threads != proc->start_threads)
        this_thread::sleep_for(chrono::milliseconds(1));
    int pn = 0;
    int bmaxnum = 0;
    for(;;)
    {
        pn = allm.size();
        this_thread::sleep_for(chrono::milliseconds(5));
        int total_n = 0;
        auto now = proc->now;
        for(M &m : allm){
            //TODO:need handle it,could crash
            if(GO_FETCH(m._m,_g) != nullptr){
                preemptM(&m);
            }else if(proc->tasks.empty()){
                total_n ++;
            }
        }
        double equal = chrono::duration<double,std::milli>(proc->now-now).count();
        //在此次检查线程期间是否 proc->task已更新，如果更新了就不计数
        if(now == proc->now && total_n == pn)
            bmaxnum ++;
        if(bmaxnum >= 5)break;
    }
    Debug("sysmon start ending...")
    delete proc;
}
void Sysmon::newm(size_t procn)
{
    regsig();
    if(proc != nullptr)throw "sysmon init failed";
    proc = new Proc(procn);
    if(proc == nullptr) throw "proc init failed";
    _m = thread(monitor);
}
void Sysmon::wait()
{
    _m.join();
}

