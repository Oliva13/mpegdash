#ifndef _SEM_BUFF_H_
#define _SEM_BUFF_H_

#include <pthread.h>
#include <semaphore.h>
#include "boolean.h"
#include "avi.h"

void freeSubsessionBuffer(avi_frame_t *pSubBuffer);

#endif
