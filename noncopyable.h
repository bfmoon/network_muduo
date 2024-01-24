#pragma once

//定义不可拷贝类
class noncopyable
{
public:
    noncopyable(const noncopyable&)=delete;
    void operator=(const noncopyable&)=delete;
protected:
    noncopyable()=default;
    ~noncopyable()=default;
};