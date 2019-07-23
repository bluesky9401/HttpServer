
#ifndef _NONCOPYABLE_H_
#define _NONCOPYABLE_H_

// 阻止拷贝、赋值(子类通过private继承实现该功能)
class Noncopyable {
public:
    Noncopyable(const Noncopyable&) = delete;
    void operator=(const Noncopyable&) = delete;

protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
};

#endif
