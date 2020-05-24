#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include "recorder.h"

void freeSubsessionBuffer(avi_frame_t *pSubBuffer)
{
	if (pSubBuffer) 
	{
		if (pSubBuffer->data) 
		{
			free(pSubBuffer->data);
		}
		memset(pSubBuffer, 0, sizeof(avi_frame_t));
	}
}


