//#include "aio-socket.h"
//#include "aio-timeout.h"
#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-avc.h"
//#include "http-server.h"
//#include "http-route.h"
//#include "cstringext.h"
//#include "sys/thread.h"
//#include "sys/system.h"
//#include "sys/path.h"

#include <unordered_map>
#include <sys/time.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <string>
#include <functional>
#include <cassert>
#include <cinttypes>
#include <atomic>

//#include "http-server.h"
//#include "http-parser.h"
//#include "http-server-internal.h"
//#include "aio-tcp-transport.h"
//#include "aio-accept.h"
//#include "sockutil.h"

#include "FileWritingLibrary.h"
#include "recorder.h"
#include "mpdutils.h"

char stream_name[256];

int check_stream(int ch)
{
	char curr_stream[64];
	int enabled = 1, res = 0;
	sprintf(curr_stream, "stream%d:enabled", ch);
	res = cfg_get_int(curr_stream, &enabled);
	if(res != 0) 
	{
		LOGE("Error: check_stream");
		return FW_FAIL;
	}
	return enabled;
	//if (enabled)
	//{
	//	LOGI("ENB: %s", curr_stream);
	//	return FW_OK;
	//}
	//else
	//{
	//	LOGI("DIS: %s", curr_stream);
	//	return FW_FAIL;
	//}
}

void get_stream_name(int ch, char *stream_name)
{
	char curr_stream[64];
	sprintf(curr_stream, "stream%d:encoder", ch);
	cfg_get_name(curr_stream, stream_name, 256);
}

void deallocate(void **buf)
{
	free(*buf);
	*buf = NULL;
}

int get_frame_num_video(int idx, int time_ms, avi_status_t *st_result, avi_status_t *max_result)
{
	int ret = 0;
	int i;
	int first_find_I = 0;
	avi_status_t *status;
	avi_status_t *prev_last_I, *prev_first_I;
	
	ret = avi_dir_stream(idx, &status);
	if(ret == 0 || status == NULL)
	{
		LOGE("Error avi_dir_stream!!!");
		return FW_FAIL;
	}
	
	avi_status_t *curr_I = (avi_status_t*)malloc(sizeof(avi_status_t));
	avi_status_t *p = status;

	int64_t search_time = p->pts - time_ms * 1000;

//	LOGW("find search_time - %lld", search_time);

	for (i = 0; i < ret; i++)
	{
		if(search_time < p->pts)
		{
			if(p->type == 'I')
			{
				memcpy(curr_I, p, sizeof(avi_status_t));
				prev_last_I = curr_I;
				if(!first_find_I)
				{
					prev_first_I = curr_I;
#if defined(DEBUG_INFO_DUMP)					
					LOGW("1.find prevI end %c %d %lld", prev_first_I->type, prev_first_I->number, prev_first_I->pts);
#endif					
					first_find_I = 1;
					memcpy(max_result, prev_first_I, sizeof(avi_status_t));
					//LOGW("max_result1 - %lld", max_result->pts);
				}
				//LOGW("find prev_last_I end %c %d %lld", prev_last_I->type, prev_last_I->number, prev_last_I->pts);
			}
		}
		else
		{
			if(p->type == 'I')
			{
#if defined(DEBUG_INFO_DUMP)				
				LOGW("2.find I break %c %d %lld", p->type, p->number, p->pts);
#endif				
				prev_last_I = p;
				memcpy(max_result, prev_last_I, sizeof(avi_status_t));
				//LOGW("max_result2 - %lld", max_result->pts);
				break;
			}
		}
		p++;
	}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//LOGW("End %c %d %lld", prev_last_I->type, prev_last_I->number, prev_last_I->pts);
	memcpy(st_result, prev_last_I, sizeof(avi_status_t)); // find prev_last_I
	free(status);
	free(curr_I);
	return FW_OK;
}

int get_param(int num_stream, rec_video_t *mpd_param_video)
{
	char param[64], num[2];
    FW_RESULT_TYPE nResult = FW_OK;
    int lDurationBefore = 0;
	avi_status_t *st_result = NULL, *max_result = NULL;
	int num_video_frame = 0;
	int res = 0;
	
	nResult = check_stream(num_stream);
	if(nResult == FW_FAIL)
	{
		LOGI("Error check_stream - %d", num_stream);
		goto exit;
	}
	else
	{
		if(nResult == 0)
		{
			mpd_param_video->stream_active = 0;
			nResult = FW_DISABLED_STREAM;
			LOGI("DISABLED_STREAM check_stream - %d", num_stream);
			goto exit;
		}
		else
		{
			mpd_param_video->stream_active = 1;
		}
	}

	get_stream_name(num_stream, stream_name);

	if(strcmp("H.265", stream_name) != 0)
	{
		if(strcmp("H.264", stream_name) != 0)
		{
			nResult = FW_UNSUPPORTED_FILE_FORMAT;
			LOGI("Error UNSUPPORTED_FILE_FORMAT check_stream_name - %s", stream_name);
			goto exit;
		}
	}

	//LOGI("found stream_name - %s ...", stream_name);

	st_result = (avi_status_t *)malloc(sizeof(avi_status_t));
	if(st_result == NULL)
	{
		LOGE("Error: malloc st_result");
		nResult = FW_FAIL;
		goto exit;
	}

    max_result =  (avi_status_t *)malloc(sizeof(avi_status_t));
	if(max_result == NULL)
	{
		LOGE("Error: malloc max_result");
		deallocate((void **)&st_result);
		nResult = FW_FAIL;
		goto exit;
	}

	res = get_frame_num_video(num_stream, lDurationBefore, st_result, max_result);
    if(res)
    {
		LOGE("Error: get_frame_num_video");
		deallocate((void **)&st_result);
		deallocate((void **)&max_result);
		nResult = FW_FAIL;
		goto exit;
    }

#if defined(DEBUG_GETPARAM)	
	LOGW("new st - lDurationBefore - %d", lDurationBefore);
    LOGW("new st - num_stream - %d", num_stream);
	LOGW("new st - st_result  - %d", st_result);
	LOGW("new st - max_result - %d", max_result);
#endif
		
	
	//LOGE("======================================================================================================================");
	//system("ls /tvh/av/0");
	//LOGE("======================================================================================================================");
	
	num_video_frame = st_result->number;
	
//	LOGW("num_vido_frame  numframe - %d, pts - %lld", st_result->number, st_result->pts);
	
	mpd_param_video->vfNumber = num_video_frame;
	mpd_param_video->vStream = num_stream;
	
	sprintf(num, "%d", num_stream);

	res = cfg_get_float(STREAM("framerate"), &mpd_param_video->framerate);
	if(res != 0) 
	{
		LOGE("Error: get framerate");
		nResult = FW_FAIL;
	}
	
	res = cfg_get_int(STREAM("width"),  &mpd_param_video->width);
	if(res != 0) 
	{
		LOGE("Error: get width");
		nResult = FW_FAIL;
	}
	
	res = cfg_get_int(STREAM("height"), &mpd_param_video->height);
	if(res != 0) 
	{
		LOGE("Error: get height");
		nResult = FW_FAIL;
	}
	
	res = cfg_get_int(STREAM("encoder"), &mpd_param_video->encoder);
	if(res != 0) 
	{
		LOGE("Error: get encoder");
		nResult = FW_FAIL;
	}
	
#if defined(DEBUG_GETPARAM)	
	LOGE("rec_param_video.framerate - %f", mpd_param_video->framerate);
	LOGE("rec_param_video.width - %d", mpd_param_video->width);
	LOGE("rec_param_video.height - %d", mpd_param_video->height);
	LOGE("rec_param_video.vStream - %d", mpd_param_video->vStream);
	LOGE("rec_param_video.encoder - %d", mpd_param_video->encoder);
	LOGE("rec_param_video.enable - %d", mpd_param_video->stream_active);
#endif
	
    mpd_param_video->start_init = false;
	
exit:
	if(st_result != NULL)
		deallocate((void **)&st_result);
	if(max_result != NULL)
		deallocate((void **)&max_result);
		
	return nResult; // To do check for NULL
}
