/*
 * ========================= jobQue_mem.cpp ==========================
 *                          -- tpr --
 *                                        CREATE -- 2019.04.24
 *                                        MODIFY -- 
 * ----------------------------------------------------------
 */
#include "esrc_jobQue.h"


//-------------------- CPP --------------------//
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include <iostream>
#include <string>
using std::cout;
using std::endl;


namespace esrc {//------------------ namespace: esrc -------------------------//

namespace {//------------ namespace --------------//

    //-- 提醒所有 job thread：jobs处理完毕后即可 自动 exit 了 --
    std::atomic<bool> exitJobThreadsFlag(false);

    std::deque<Job>          jobQue {};
    std::mutex               jobQueMutex;
    std::condition_variable  jobQueCondVar;


}//---------------- namespace end --------------//


/* ===========================================================
 *               atom_exitJobThreadsFlag_store
 * -----------------------------------------------------------
 * -- 通常为 游戏主线程 向 jobQue 写入一个 true
 *    提醒 job thread，在处理完 jobQue 中所有 jobs 后，即可自行exit
 */
void atom_exitJobThreadsFlag_store( bool _val ){
    exitJobThreadsFlag.store( _val );
}

/* ===========================================================
 *               atom_exitJobThreadsFlag_load
 * -----------------------------------------------------------
 * -- 通常为 job thread 调用，来查看 exitJobThreadsFlag 的值
 */
bool atom_exitJobThreadsFlag_load(){
    return exitJobThreadsFlag.load();
}


/* ===========================================================
 *                   atom_push_back_2_jobQue
 * -----------------------------------------------------------
 * -- 通常为 游戏主线程 向 jobQue 写入一个 job
 */
void atom_push_back_2_jobQue( const Job &_job ){

    {//--- atom ---//
        std::lock_guard<std::mutex> lg(jobQueMutex);
        esrc::jobQue.push_back( _job ); //- copy
    }
    //-- 解除 消费者线程 的阻塞
    //  注意，此句 不需要处于 上文的 lock作用域中
    jobQueCondVar.notify_one();
}


/* ===========================================================
 *                   atom_is_jobQue_empty
 * -----------------------------------------------------------
 * -- 通常由 job线程来 检查 jobQue 状态
 */
bool atom_is_jobQue_empty(){
    bool ret;
    {//--- atom ---//
        std::lock_guard<std::mutex> lg(jobQueMutex);
                //- 若抢占不到 mutex 的 lock权，就会持续 阻塞 下去
                //  直到 其他程序释放此 mutex，本语句 成功抢占 为止。
        ret = esrc::jobQue.empty();
    }
    return ret;
}


/* ===========================================================
 *                  atom_pop_from_jobQue
 * -----------------------------------------------------------
 * -- 通常由 job线程来从 jobQue 获取一个 job
 */
Job atom_pop_from_jobQue(){

    Job job;
    {//--- atom ---//
        std::unique_lock<std::mutex> ul(jobQueMutex);
        jobQueCondVar.wait_for( ul, std::chrono::milliseconds(500), []{ return !esrc::jobQue.empty(); } );
                //- 阻塞，直到 生产者调用 notify_xxx() 函数
                //  通过 参数3 用来防止 假醒
                //- 参数3 判断式 在被调用时，仍处于 unique_lock 实例的作用范围，所以是线程安全的
                //- 通过 参数2 来限制 wait时间（0.5秒）时间到了自己苏醒。

        //- 此句 jobQue.empty() 在被调用时，仍处于 unique_lock 实例的作用范围，所以是线程安全的
        if( esrc::jobQue.empty() ){
            //-- 说明并没有获取job，仅仅是时间到了
            job.jobType = JobType::JustTimeOut;
        }else{
            job = esrc::jobQue.front();
            esrc::jobQue.pop_front();
        }
    }
    return job;
}


}//---------------------- namespace: esrc -------------------------//

