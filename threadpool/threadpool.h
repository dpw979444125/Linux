#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include "condition.h"

//任务队列
typedef struct task{
  void *(*pfun)(void*);   //回调函数
  void *arg;              //回调函数参数
  struct task *next;
}task_t;

typedef struct threadpool{
  condition_t cond;   //同步互斥
  task_t *first;
  task_t *tail;
  int max_thread;
  int idle;
  int counter;
  int quit;
}threadpool_t;

//初始化
void threadpool_init(threadpool_t *pool, int max);

//往线程池添加任务
void threadpool_add(threadpool_t *pool, void*(*pf)(void*),void *arg);

//销毁线程池
void threadpool_destroy(threadpool_t *pool);

#endif
