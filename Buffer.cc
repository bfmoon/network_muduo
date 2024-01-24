#include"Buffer.h"

#include<errno.h>
#include<sys/uio.h>
#include<unistd.h>

const size_t Buffer::kCheapPrepend ;
const size_t Buffer::kInitialSize ;

/**
 * 从fd上读取数据  Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小
 */ 
ssize_t Buffer::readFd(int fd,int* savedErrno)
{
    
    char extrabuf[65536]={0}; //栈上的内存空间  64K
    //定义内核缓冲区
    iovec vec[2];
    const size_t writable=writableBytes();
    //第一个缓冲区的大小为可可写缓冲区的大小
    //base为起始地址
    vec[0].iov_base=begin()+writerIndex_;
    vec[0].iov_len=writable;

    //第二个为栈上的内存空间  64K
    vec[1].iov_base=extrabuf;
    vec[1].iov_len=sizeof extrabuf;
    //如果接收的数据小于可写缓冲区的大小，则就选择第二个栈上的空间，否则选择第一个，直接将接受的数据写入可写缓冲区中
    const int iovcnt=(writable< sizeof extrabuf)?2:1;

    //其中readv是异步非阻塞io函数
    const ssize_t n=::readv(fd,vec,iovcnt);
    if(n<0)
    {
        *savedErrno=errno;
    }
    else if(n<=writable)
    {
        writerIndex_+=n;
    }
    else
    {
        writerIndex_=buffer_.size();
        append(extrabuf,n-writable);
    }
    return n;
}
// 通过fd上发送数据
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}