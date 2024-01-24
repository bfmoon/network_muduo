#pragma once
#include<vector>
#include<string.h>
#include<algorithm>
#include<string>

/*
该类是设计一个缓冲区，用于数据接收与发送时的存储
格式如下：
+-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
其中 prependable bytes表示包头8个字节，readable bytes可读缓冲区，writable bytes可写缓冲区
*/
class Buffer
{
public:
    static const size_t kCheapPrepend=8;
    static const size_t kInitialSize=1024;

    explicit Buffer(size_t initialSize=kInitialSize)
    :buffer_(kCheapPrepend+initialSize)  //初始化缓冲区大小
    ,readerIndex_(kCheapPrepend)
    ,writerIndex_(kCheapPrepend)
    {

    }

    //返回可读缓冲区的大小
    size_t readableBytes()const{return writerIndex_-readerIndex_;}
    //返回可写缓冲区的大小
    size_t writableBytes()const { return buffer_.size()-readerIndex_;}
    //返回开头的大小
    size_t prependableBytes()const { return readerIndex_;}

    //返回可读缓冲区的起始地址
    const char* peek()const { return begin()+readerIndex_;}

    //对于onMessage回调函数，将接收的buffer数据转为string
    void retrieve(size_t len)
    {
        //如果该数据长度小于缓冲的大小，说明可以放下
        //则可读缓冲区的下标右移
        if(len<readableBytes())
        {
            readerIndex_+=len;
        }
        else
        {
            //否则，重新移动位置
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_=kCheapPrepend;
        writerIndex_=kCheapPrepend;
    }

    //将onMessage的数据转换为string字符串类型
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(),len);
        retrieve(len);  //上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }

    //判断可写缓冲区的大小是否大于要写入的数据长度
    //如果不是，则进行扩容
    void ensureWritableBytes(size_t len)
    {
        if(writableBytes()<len)
        {
            makeSpace(len);
        }
    }

    void append(const char* data,size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data,data+len,beginWrite());
    }

    char* beginWrite() { return begin()+writerIndex_;}
    const char* beginWrite()const { return begin()+writerIndex_;}

    // 从fd上读取数据
    ssize_t readFd(int fd,int* savedErrno);
    // 通过fd上发送数据
    ssize_t writeFd(int fd,int* savedErrno);


private:
    //返回缓冲区的起始地址
    char* begin() { return &*buffer_.begin();}
    const char* begin()const {return &*buffer_.begin();}

    void makeSpace(size_t len)
    {
        //这句话表示剩余的缓冲区大小，也就是可写缓冲区与头部相加的大小，如果不能够存放该数据，则要进行扩容
        //否则，直接在原有缓冲区上移动位置即可
        if(writableBytes()+prependableBytes()<len+kCheapPrepend)
        {
            buffer_.resize(writerIndex_+len);
        }
        else
        {
            size_t readable=readableBytes();


            //将begin()+writerIndex_该位置数据 移动到begin()+readerIndex_该位置上，一共移动begin()+kCheapPrepend这么长
            std::copy(begin()+readerIndex_,begin()+writerIndex_,begin()+kCheapPrepend);
            readerIndex_=kCheapPrepend;
            writerIndex_=readerIndex_+readable;
        }
    }

    std::vector<char>buffer_; //缓冲区的大小
    size_t readerIndex_;   //可读缓冲区的起始下标
    size_t writerIndex_;   //可写缓冲区的起始下标
};