#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "threadpool.h"

//初始化
void threadpool_init(threadpool_t *pool, int max)
{
  condition_init(&pool->cond);
  pool->first = NULL;
  pool->tail  = NULL;
  pool->max_thread = max;
  pool->idle = 0;   //空闲线程个数
  pool->counter = 0;//当前线程个数
  pool->quit = 0;   //如果退出，是1
}

void *myroute(void *arg)      //执行任务的函数
{
  threadpool_t *pool = (threadpool_t*)arg;
  int timeout = 0;
  while(1)
  {
    condition_lock(&pool->cond);
    pool->idle++;
    timeout = 0;
    while(pool->first == NULL && pool->quit == 0)
    {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += 2;
      int r = condition_timedwait(&pool->cond, &ts);       //等待，得到pool->first !=NULL 的响应，退出循环
      if(r == ETIMEDOUT)
      {
        timeout = 1;
        break;    //超时退出
      }
    }
    pool->idle--;

    if(pool->first != NULL)
    {
      task_t *p = pool->first;  //第一个任务拿下来
      pool->first = p->next;    //任务队列向后走一个
      condition_unlock(&pool->cond);   //防止文件编译太久
      (p->pfun)(p->arg);            //执行任务
      condition_lock(&pool->cond);
      free(p);
      p = NULL;
    }


    if(pool->first == NULL && timeout == 1)  //超时退出
    {
      condition_unlock(&pool->cond);
      printf("%#X thread timeout\n",pthread_self());
      break;          //退出while循环
    }

    if(pool->quit == 1 && pool->first == NULL)
    {
      printf("%#Xthread destroy\n",pthread_self());
      pool->counter --;
      if(pool->counter == 0)
      {
        condition_signal(&pool->cond);
      }
      condition_unlock(&pool->cond);
      break;
    }

    condition_unlock(&pool->cond);
  }
} 

//往线程池添加任务
void threadpool_add(threadpool_t *pool, void*(*pf)(void*), void *arg)
{
  //生成任务节点,main函数传的任务
  task_t *newtask = (int*)malloc(sizeof(task_t));
  newtask->pfun = pf;
  newtask->arg = arg;
  newtask->next = NULL;

  condition_lock(&pool->cond);
  //放入任务队列
  if(pool->first == NULL)
  {
    pool->first = newtask;
  }
  else
  {
    pool->tail->next = newtask;
  }
  pool->tail = newtask;

  if(pool->idle > 0 && pool->first != NULL)                                    //有空闲线程
  {
    condition_signal(&pool->cond);  //发送pool->first != NULL的信号
  }
  else if(pool->counter < pool->max_thread)             //线程数没有达到上限
  {
    pthread_t tid;
    pthread_create(&tid, NULL, myroute, (void*)pool);   //创建线程
    pool->counter++;
  }
  condition_unlock(&pool->cond);
}

void threadpool_destroy(threadpool_t* pool)
{
  if(pool->quit)
  {
    return ;
  }
  condition_lock(&pool->cond);
  pool->quit = 1;
  if(pool->counter > 0)
  {
    if(pool->idle > 0)
    {
      condition_boardcast(&pool->cond);
    }
  }
  while(pool->counter > 0)         //在此wait，直到pool->counter==0，signal发送信号，不再等待
  {
    condition_wait(&pool->cond);
  }

  condition_unlock(&pool->cond);
  condition_destroy(&pool->cond);
}
