#pragma once
/**
 * 该类主要是获取每个事件循环的线程id
*/
#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    //__thread就表示该变量只有该线程可以修改，其他线程修改不了
    extern __thread int t_cachedTid;

    //内存中缓存的线程id
    void cacheTid();

    inline int tid()
    {
        //就判断当前线程id是否已经获取，如果等于0，则从cache中获取
        if(__builtin_expect(t_cachedTid==0,0))
        {
            cacheTid();
        }
        //否则直接返回
        return t_cachedTid;
    }
}