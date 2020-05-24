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

//using namespace std;

//void on_cfg_changed_resolut(int num_stream)
//{
//	int res = 0;
//	char param[64], num[2];
//	static float tmp_framerate = 0;
//	rec_video_t *rec_param_video;
//	sprintf(num, "%d", num_stream);
//	
//	LOGE("on_cfg_changed_resolut");
//	//LOGE("==============================================================================================================\n");
//		
//	if(!num_stream)
//	{
//		rec_param_video = param_video_stream_0;
//	}
//	else
//	{
//		rec_param_video = param_video_stream_1;
//	}
//	
//	res = cfg_get_float(STREAM("framerate"), &rec_param_video->framerate); // To do rec_param_video[cn]
//	if(res != 0) 
//	{
//		LOGE("Error on_cfg_changed_resolut get framerate!!!\n");
//	}
//	if(tmp_framerate == 0)
//	{
//		tmp_framerate = rec_param_video->framerate;
//	}
//	else
//	{
//		if(tmp_framerate != rec_param_video->framerate)		
//		{
//			LOGE("new rec_param_video.framerate - %f", rec_param_video->framerate);
//			rec_param_video->start_init = false;
//		}
//	}
//}

//void on_cfg_changed_stream(int num_stream)
//{
//	int res = 0;
//	rec_video_t *rec_param_video;
//	char param[64], num[2];
//	
//	LOGE("on_cfg_changed_stream");
//		
//	sprintf(num, "%d", num_stream);
//	if(!num_stream)
//	{
//		rec_param_video = param_video_stream_0;
//	}
//	else
//	{
//		rec_param_video = param_video_stream_1;
//	}
//	
//	res = cfg_get_int(STREAM("enabled"), &rec_param_video->stream_active);
//	if(res != 0) 
//	{
//		LOGE("Error on_cfg_stream_enabled!!!\n");
//	}
//	LOGE("------- stream_active - 0x%x stream %d-----------", rec_param_video->stream_active, num_stream);
//	rec_param_video->start_init = false;
//	//pthread_cond_signal(&cond);
//}

//void on_cfg_changed_framerate(int num_stream)
//{
//	int res = 0;
//	char param[64], num[2];

//    static int tmp_width = 0;
//    static int tmp_height = 0;
//	rec_video_t *rec_param_video;
//	
//	LOGE("on_cfg_changed_framerate");

//	sprintf(num, "%d", num_stream);
//	
//	if(!num_stream)
//	{
//		rec_param_video = param_video_stream_0;
//	}
//	else
//	{
//		rec_param_video = param_video_stream_1;
//	}
//	
//	res = cfg_get_int(STREAM("width"),  &rec_param_video->width);
//	if(res != 0) 
//	{
//		LOGE("Error on_cfg_changed_framerate get width!!!\n");
//	}			
//	if(tmp_width == 0)
//	{
//		tmp_width = rec_param_video->width;
//	}
//	else
//	{
//		LOGE("!!!!!!!!!!!!!! =========================== new rec_param_video.width - %d, tmp_width - %d", rec_param_video->width, tmp_width);
//		if(tmp_width != rec_param_video->width)
//		{
//			LOGE("new rec_param_video.width - %d", rec_param_video->width);
//			tmp_width = rec_param_video->width;
//			rec_param_video->start_init = false;
//		}
//	}
//			
//	res = cfg_get_int(STREAM("height"), &rec_param_video->height);
//	if(res != 0) 
//	{
//		LOGE("Error on_cfg_changed_framerate get height!!!\n");
//	}			
//			
//	if(tmp_height == 0)
//	{
//		tmp_height = rec_param_video->height;
//	}
//	else
//	{
//		LOGE("!!!!!!!!!!!!!! =========================== new rec_param_video.height - %d, tmp_height - %d", rec_param_video->height, tmp_height);
//		if(tmp_height != rec_param_video->height)
//		{
//			LOGE("new rec_param_video.height - %d", rec_param_video->height);
//			tmp_height = rec_param_video->height;
//			rec_param_video->start_init = false;
//		}
//	}
//}

//void on_cfg_changed_encoder(int num_stream)
//{
//	char param[64], num[2];
//	int res = 0;
//	rec_video_t *rec_param_video;
//	
//	LOGE("on_cfg_changed_encoder");
//	
//	if(!num_stream)
//	{
//		rec_param_video = param_video_stream_0;
//	}
//	else
//	{
//		rec_param_video = param_video_stream_1;
//	}
//	sprintf(num, "%d", num_stream);
//	res = cfg_get_int(STREAM("encoder"), &rec_param_video->encoder);
//	if(res != 0) 
//	{
//		LOGE("Error on_cfg_changed_encoder!!!\n");
//	}
//	rec_param_video->start_init = false;
//}

//void subscribe_change_param_video(int ch)
//{	
//	char stream_resolut[64];
//	char stream_framerate[64];
//	char stream_encoder[64];
//	char stream_enabled[64];
//	
//	sprintf(stream_enabled, "stream%d", ch);
//	cfg_subscribe(stream_enabled, on_cfg_changed_stream, ch);
//	sprintf(stream_resolut, "stream%d:resolution", ch);
//	cfg_subscribe(stream_resolut, on_cfg_changed_resolut, ch);
//	sprintf(stream_framerate, "stream%d:framerate", ch);
//	cfg_subscribe(stream_framerate, on_cfg_changed_framerate, ch);
//	sprintf(stream_encoder, "stream%d:encoder", ch);
//	cfg_subscribe(stream_encoder, on_cfg_changed_encoder, ch);
//}


