#include"CurrentThread.h"

namespace CurrentThread
{
    //初始化为0
    __thread int t_cachedTid=0;

    //通过系统函数获取线程id
    void cacheTid()
    {
        if(t_cachedTid==0)
        {
            t_cachedTid=static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}


