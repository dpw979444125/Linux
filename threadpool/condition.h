#ifndef __CONDITION_H__
#define __CONDITION_H__

#include <pthread.h>

typedef struct condition {
	pthread_mutex_t pmutex;  //互斥量
	pthread_cond_t pcond;    //条件变量
} condition_t;

int condition_init(condition_t *cond);
int condition_lock(condition_t *cond);
int condition_unlock(condition_t *cond);
int condition_wait(condition_t *cond);
int condition_timedwait(condition_t *cond, const struct timespec*abstime);
int condition_signal(condition_t *cond);
int condition_boardcast(condition_t *cond);
int condition_destroy(condition_t *cond);


#endif // __CONDITION_H__

