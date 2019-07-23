#include <iostream>
using std::cout;
using std::cin;
using std::endl;
#include "ThreadPool.h"
// 测试
 class Test
 {
 private:
     /* data */
 public:
     Test(/* args */);
     ~Test();
     void func()
     {
         std::cout << "Work in thread: " << std::endl;
     }
 };

 Test::Test(/* args */)
 {
 }

 Test::~Test()
 {
 }


 int main()
 {
     Test t;
     ThreadPool tp(2);
     tp.start();
     for (int i = 0; i < 1000; ++i)
     {
         std::cout << "addtask" << std::endl;
         tp.addTask(std::bind(&Test::func, &t));
     }
 }

