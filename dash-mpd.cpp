#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "list.h"
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
//#include <inttypes.h>
//#include <ctypedef.h>
//#define __STDC_FORMAT_MACRO
#include <cinttypes> 
//надо добавить #define __STDC_FORMAT_MACRO
//#include <unordered_map>
//#include <sys/time.h>
//#include <signal.h>
//#include <iostream>
//#include <string>
//#include <cstring>
//#include <functional>
//#include <cassert>
//#include <cinttypes>
//#include <atomic> 
#include "mpdutils.h"
#include "recorder.h"

static int mov_buffer_read(void* param, void* data, uint64_t bytes)
{
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (dash->offset + bytes > dash->bytes)
		return E2BIG;
	memcpy(data, dash->ptr + dash->offset, (size_t)bytes);
	return 0;
}

static int mov_buffer_write(void* param, const void* data, uint64_t bytes)
{
	uint8_t* ptr;
	size_t capacity;
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (dash->offset + bytes > dash->maxsize)
		return E2BIG;

	if (dash->offset + (size_t)bytes > dash->capacity)
	{
		capacity = dash->offset + (size_t)bytes + N_SEGMENT;
		capacity = capacity > dash->maxsize ? dash->maxsize : capacity;
		ptr = (uint8_t*)realloc(dash->ptr, capacity);
		if (NULL == ptr)
			return ENOMEM;
		dash->ptr = ptr;
		dash->capacity = capacity;
	}

	memcpy(dash->ptr + dash->offset, data, (size_t)bytes);
	dash->offset += (size_t)bytes;
	if (dash->offset > dash->bytes)
		dash->bytes = dash->offset;
	return 0;
}

static int mov_buffer_seek(void* param, uint64_t offset)
{
	struct dash_adaptation_set_t* dash;
	dash = (struct dash_adaptation_set_t*)param;
	if (offset >= dash->maxsize)
		return E2BIG;
	dash->offset = (size_t)offset;
	return 0;
}

static uint64_t mov_buffer_tell(void* param)
{
	return ((struct dash_adaptation_set_t*)param)->offset;
}

static struct mov_buffer_t s_io = {
	mov_buffer_read,
	mov_buffer_write,
	mov_buffer_seek,
	mov_buffer_tell,
};

static int dash_adaptation_set_segment(struct dash_mpd_t* mpd, struct dash_adaptation_set_t* track)
{
	int r;
	char name[N_NAME + 32];
	struct list_head *link;
	struct dash_segment_t* seg;

	r = fmp4_writer_save_segment(track->fmp4);
	if (0 != r)
		return r;

	seg = (struct dash_segment_t*)calloc(1, sizeof(*seg));
	seg->timestamp = track->dts;
	seg->duration = (track->dts_last - track->dts); //+40; //!!!!!

#if defined(DEBUG_INFO) 	
	printf("dash_adaptation_set_segment : timestamp - %lld, duration - %lld, dts_last - %lld, track->dts - %lld\n", seg->timestamp, seg->duration, track->dts_last, track->dts);
#endif

	if(MOV_OBJECT_AAC == track->object)
	{
		snprintf(name, sizeof(name), "%s-%" PRId64 ".m4a", track->prefix, seg->timestamp);
	}
	else
	{
		snprintf(name, sizeof(name), "%s-%" PRId64 ".m4v", track->prefix, seg->timestamp);
	}
	
	r = mpd->handler(mpd->param, track->setid, track->ptr, track->bytes, track->pts, track->dts, seg->duration, name);
	if (0 != r)
	{
		free(seg);
		return r;
	}
	//printf("=========== timestamp - %lld, duration - %lld, dts_last %lld\n", seg->timestamp, seg->duration, track->dts_last);
	// link
	//printf("list_insert_after(&seg->link, track->root.prev)\n");
	list_insert_after(&seg->link, track->root.prev);
	
	//struct list_head *p, *n;
	//struct dash_segment_t *mseg;
	//list_for_each_safe(p, n, &track->root)
	//{
	//	mseg = list_entry(p, struct dash_segment_t, link);
	//	//printf("************** timestamp - %lld, duration - %lld\n", mseg->timestamp, mseg->duration);
	//}
	
	track->count += 1;
	if (DASH_DYNAMIC == mpd->flags && track->count > N_COUNT)
	{
		link = track->root.next;
		list_remove(link);
		seg = list_entry(link, struct dash_segment_t, link);
		
		//printf("------------------------------- free seg ------------------------------ \n");
		
		free(seg);
		--track->count;
	}
	return 0;
}

static int dash_mpd_flush(struct dash_mpd_t* mpd)
{
	int i, r;
	struct dash_adaptation_set_t* track;

	for (r = i = 0; i < mpd->count && 0 == r; i++)
	{
		track = mpd->tracks + i;
		if (track->raw_bytes)
		{
			r = dash_adaptation_set_segment(mpd, track);
			
			// update maximum segment duration
			mpd->max_segment_duration = MAX(track->dts_last - track->dts, mpd->max_segment_duration);
#if defined(DEBUG_INFO) 			
			printf("dash_mpd_flush : max_segment_duration - %d\n",  mpd->max_segment_duration);
#endif			
			if(track->dts_last > track->dts)
			{
				track->bitrate = MAX(track->bitrate, (int)(track->raw_bytes * 1000 / (track->dts_last - track->dts) * 8));	
#if defined(DEBUG_INFO) 				
				printf("dash_mpd_flush : track->bitrate - %d\n", track->bitrate);
#endif				
			}
		}

		track->pts = INT64_MIN;
		track->dts = INT64_MIN;
		track->raw_bytes = 0;

		// reset track buffer
		track->offset = 0;
		track->bytes = 0;
	}

	return r;
}

struct dash_mpd_t* dash_mpd_create(int flags, dash_mpd_segment segment, void* param)
{
	struct dash_mpd_t* mpd;
	mpd = (struct dash_mpd_t*)calloc(1, sizeof(*mpd));
	if (mpd)
	{
		mpd->flags = flags;
		mpd->handler = segment;
		mpd->param = param;
		mpd->time = time(NULL);
	}
	return mpd;
}

void dash_mpd_destroy(struct dash_mpd_t* mpd)
{
	int i;
	struct list_head *p, *n;
	struct dash_segment_t *seg;
	struct dash_adaptation_set_t* track;

	dash_mpd_flush(mpd);

	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];

		if (track->ptr)
		{
			free(track->ptr);
			track->ptr = NULL;
		}

		list_for_each_safe(p, n, &track->root)
		{
			seg = list_entry(p, struct dash_segment_t, link);
			free(seg);
		}
	}

	free(mpd);
}

int dash_mpd_add_video_adapation_set(struct dash_mpd_t* mpd, const char* prefix, uint8_t object, int frame_rate, int width, int height, const void* extra_data, size_t extra_data_size)
{
	int r;
	char name[N_NAME + 16];
	struct dash_adaptation_set_t* track;

	r = strlen(prefix);
	if (mpd->count + 1 >= N_TRACK || extra_data_size < 4 || r >= N_NAME)
		return -1;

	//assert(MOV_OBJECT_H264 == object);//!!!!
	track = &mpd->tracks[mpd->count];
	memcpy(track->prefix, prefix, r);
	LIST_INIT_HEAD(&track->root);
	track->setid = mpd->count++;
	track->object = object;
	track->bitrate = 0;
	track->u.video.width = width;
	track->u.video.height = height;
	track->u.video.frame_rate = 10; //////////////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//assert(((const uint8_t*)extra_data)[0] == 1); // configurationVersion
	if (MOV_OBJECT_H264 == object)
	{
		track->u.video.avc.profile = ((const uint8_t*)extra_data)[1];
		track->u.video.avc.compatibility = ((const uint8_t*)extra_data)[2];
		track->u.video.avc.level = ((const uint8_t*)extra_data)[3];
	}
	if (MOV_OBJECT_HEVC == object)
	{
		//track->u.video.avc.profile = ((const uint8_t*)extra_data)[1];
		//track->u.video.avc.compatibility = ((const uint8_t*)extra_data)[2];
		//track->u.video.avc.level = ((const uint8_t*)extra_data)[12];
	}
	track->seq = 1;
	track->maxsize = N_FILESIZE;
	track->fmp4 = fmp4_writer_create(&s_io, track, MOV_FLAG_SEGMENT);
	if (!track->fmp4)
		return -1;
	track->track = fmp4_writer_add_video(track->fmp4, object, width, height, extra_data, extra_data_size);
	
	// save init segment file
	r = fmp4_writer_init_segment(track->fmp4);
	if (0 == r)
	{
		snprintf(name, sizeof(name), "%s-init.m4v", prefix);
		//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! create - %s\n", name);
		r = mpd->handler(mpd->param, mpd->count, track->ptr, track->bytes, 0, 0, 0, name);
	}

	track->bytes = 0;
	track->offset = 0;
	return 0 == r ? track->setid : r;
}

int dash_mpd_add_audio_adapation_set(struct dash_mpd_t* mpd, const char* prefix, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size)
{
	int r;
	char name[N_NAME + 16];
	struct dash_adaptation_set_t* track;

	r = strlen(prefix);
	if (mpd->count + 1 >= N_TRACK || extra_data_size < 2 || r >= N_NAME)
		return -1;

	assert(MOV_OBJECT_AAC == object);
	track = &mpd->tracks[mpd->count];
	memcpy(track->prefix, prefix, r);
	LIST_INIT_HEAD(&track->root);
	track->setid = mpd->count++;
	track->object = object;
	track->bitrate = 0;
	track->u.audio.channel = channel_count;
	track->u.audio.sample_bit = bits_per_sample;
	track->u.audio.sample_rate = sample_rate;
	track->u.audio.profile = ((const uint8_t*)extra_data)[0] >> 3;
	if(MOV_OBJECT_AAC == object && 31 == track->u.audio.profile)
		track->u.audio.profile = 32 + (((((const uint8_t*)extra_data)[0] & 0x07) << 3) | ((((const uint8_t*)extra_data)[1] >> 5) & 0x07));

	track->seq = 1;
	track->maxsize = N_FILESIZE;
	track->fmp4 = fmp4_writer_create(&s_io, track, MOV_FLAG_SEGMENT);
	if (!track->fmp4)
		return -1;
	track->track = fmp4_writer_add_audio(track->fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);

	r = fmp4_writer_init_segment(track->fmp4);
	if (0 == r)
	{
		snprintf(name, sizeof(name), "%s-init.m4a", prefix);
		r = mpd->handler(mpd->param, mpd->count, track->ptr, track->bytes, 0, 0, 0, name);
	}

	track->bytes = 0;
	track->offset = 0;
	return 0 == r ? track->setid : r;
}

int dash_mpd_input(struct dash_mpd_t* mpd, int adapation, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags)
{
	int r = 0;
	struct dash_adaptation_set_t* track;
	if (adapation >= mpd->count || adapation < 0)
		return -1;

	track = &mpd->tracks[adapation];
	if (NULL == data || 0 == bytes || ((MOV_AV_FLAG_KEYFREAME & flags) && (MOV_OBJECT_H264 == track->object || MOV_OBJECT_HEVC == track->object)))
	{
		track->dts_last = dts; //my
		r = dash_mpd_flush(mpd);
		//track->dts_last = dts; //my
		// FIXME: live duration
#if defined(DEBUG_INFO) 		
		printf("dash_mpd_input : 1.mpd->duration - %lld\n", mpd->duration);
#endif		
		mpd->duration += mpd->max_segment_duration;
#if defined(DEBUG_INFO) 		
		printf("dash_mpd_input : 2.mpd->max_segment_duration - %lld\n", mpd->max_segment_duration);
		printf("dash_mpd_input : 3.mpd->duration - %lld\n", mpd->duration);
#endif		
	}

	if (NULL == data || 0 == bytes)
	{	
		printf("Error: dash_mpd_input : NULL == data || 0 == bytes\n");
		return r;
	}

	if (0 == track->raw_bytes)
	{
		track->pts = pts;
		track->dts = dts;
#if defined(DEBUG_INFO) 		
		printf("dash_mpd_input: 4.track->pts - %lld\n",track->pts);
		printf("dash_mpd_input: 5.track->dts - %lld\n",track->dts);
#endif		
	}

	track->dts_last = dts;
	track->raw_bytes += bytes;
#if defined(DEBUG_INFO)	
	printf("dash_mpd_input:6.track->dts_last - %lld\n", track->dts_last);
	printf("dash_mpd_input:7.raw_bytes - %lld\n", track->raw_bytes);
#endif		
	return fmp4_writer_write(track->fmp4, track->track, data, bytes, pts, dts, flags);
}

//typedef struct
//{
//	char *path_file;
//	int64_t timestamp;
//	struct list_head link;
//}mpd_t;

// ISO/IEC 23009-1:2014(E) 5.4 Media Presentation Description updates (p67)
// 1. the value of MPD@id, if present, shall be the same in the original and the updated MPD;
// 2. the values of any Period@id attributes shall be the same in the original and the updated MPD, unless the containing Period element has been removed;
// 3. the values of any AdaptationSet@id attributes shall be the same in the original and the updated MPD unless the containing Period element has been removed;
size_t dash_mpd_playlist(dash_playlist_t* dash, struct dash_mpd_t* mpd, char* playlist, int stream, size_t bytes)
{
	// ISO/IEC 23009-1:2014(E)
	// G.2 Example for ISO Base media file format Live profile (141)
     const char* s_mpd_dynamic =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<MPD\n"
		"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		"    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\"\n"
		"    type=\"dynamic\"\n"
		"    minimumUpdatePeriod=\"PT%uS\"\n"
		"    timeShiftBufferDepth=\"PT%uS\"\n"
		"    availabilityStartTime=\"%s\"\n"
		"    minBufferTime=\"PT%uS\"\n"
		"    publishTime=\"%s\"\n"
		"    profiles=\"urn:mpeg:dash:profile:isoff-live:2011\">\n";

	// ISO/IEC 23009-1:2014(E)
	// G.1 Example MPD for ISO Base media file format On Demand profile
	const char* s_mpd_static =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<MPD\n"
		"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		"    xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		"    xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd\"\n"
		"    type=\"static\"\n"
		"    mediaPresentationDuration=\"PT%uS\"\n"
		"    minBufferTime=\"PT%uS\"\n"
		"    profiles=\"urn:mpeg:dash:profile:isoff-on-demand:2011\">\n";

	const char* s_h264 =
		"    <AdaptationSet contentType=\"video\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		"      <Representation id=\"H264\" mimeType=\"video/mp4\" codecs=\"avc1.%02x%02x%02x\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4v\" initialization=\"%s-init.m4v\">\n"
		"          <SegmentTimeline>\n";

	const char* s_h265 =
		"    <AdaptationSet contentType=\"video\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		//"      <Representation id=\"H265\" mimeType=\"video/mp4\" codecs=\"hvc1.%02x%02x%02x\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"      <Representation id=\"H265\" mimeType=\"video/mp4\" codecs=\"hev1.1.6.L123.B0\" width=\"%d\" height=\"%d\" frameRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4v\" initialization=\"%s-init.m4v\">\n"
		"          <SegmentTimeline>\n";

	const char* s_aac =
		"    <AdaptationSet contentType=\"audio\" segmentAlignment=\"true\" bitstreamSwitching=\"true\">\n"
		"      <Representation id=\"AAC\" mimeType=\"audio/mp4\" codecs=\"mp4a.40.%u\" audioSamplingRate=\"%d\" startWithSAP=\"1\" bandwidth=\"%d\">\n"
		"		 <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"%d\"/>\n"
		"        <SegmentTemplate timescale=\"1000\" media=\"%s-$Time$.m4a\" initialization=\"%s-init.m4a\">\n"
		"          <SegmentTimeline>\n";

	const char* s_footer =
		"          </SegmentTimeline>\n"
		"        </SegmentTemplate>\n"
		"      </Representation>\n"
		"    </AdaptationSet>\n";

	int i;
	size_t n;
	time_t now;
	char publishTime[32];
	char availabilityStartTime[32];
	unsigned int minimumUpdatePeriod;
	unsigned int timeShiftBufferDepth;
	struct dash_adaptation_set_t* track;
	struct dash_segment_t *seg;
	struct list_head *link;
	
	//pthread_mutex_lock(&mtx);		
	
	now = time(NULL);
	strftime(availabilityStartTime, sizeof(availabilityStartTime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&mpd->time));
	strftime(publishTime, sizeof(publishTime), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
	
	minimumUpdatePeriod = (unsigned int)MAX(mpd->max_segment_duration / 1000, 1);

	if (mpd->flags == DASH_DYNAMIC)
	{
		timeShiftBufferDepth = minimumUpdatePeriod * N_COUNT + 1;
	
#if defined(DEBUG_INFO) 

		printf("dash_mpd_playlist : timeShiftBufferDepth - %d\n",  timeShiftBufferDepth);
		printf("dash_mpd_playlist : minimumUpdatePeriod - %d\n",   minimumUpdatePeriod);
		printf("dash_mpd_playlist : publishTime - %s\n", publishTime);
		printf("dash_mpd_playlist : availabilityStartTime - %s\n", availabilityStartTime);
		printf("dash_mpd_playlist : mpd->max_segment_duration - %d\n", mpd->max_segment_duration);
#endif
		n = snprintf(playlist, bytes, s_mpd_dynamic, minimumUpdatePeriod, timeShiftBufferDepth, availabilityStartTime, minimumUpdatePeriod, publishTime);
		n += snprintf(playlist + n, bytes - n, "  <Period start=\"PT0S\" id=\"dash\">\n");
	}
	else
	{
		n = snprintf(playlist, bytes, s_mpd_static, (unsigned int)(mpd->duration / 1000), minimumUpdatePeriod);
		n += snprintf(playlist + n, bytes - n, "  <Period start=\"PT0S\" id=\"dash\">\n");
	}
	
#if defined(DEBUG_INFO_DUMP) 	
	printf("dash_mpd_playlist : mpd->count - %d\n", mpd->count);
#endif	
	for (i = 0; i < mpd->count; i++)
	{
		track = &mpd->tracks[i];
		if (MOV_OBJECT_H264 == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_h264, (unsigned int)track->u.video.avc.profile, (unsigned int)track->u.video.avc.compatibility, (unsigned int)track->u.video.avc.level, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->bitrate, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
#if defined(DEBUG_INFO_DUMP) 				
				printf("------------------seg->timestamp - %lld\n", seg->timestamp);
#endif				
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, s_footer);
		}
		else if (MOV_OBJECT_HEVC == track->object)
		{
			//n += snprintf(playlist + n, bytes - n, s_h265, (unsigned int)track->u.video.avc.profile, (unsigned int)track->u.video.avc.compatibility, (unsigned int)track->u.video.avc.level, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->bitrate, track->prefix, track->prefix);
			n += snprintf(playlist + n, bytes - n, s_h265, track->u.video.width, track->u.video.height, track->u.video.frame_rate, track->bitrate, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, s_footer);
		}
		else if (MOV_OBJECT_AAC == track->object)
		{
			n += snprintf(playlist + n, bytes - n, s_aac, (unsigned int)track->u.audio.profile, track->u.audio.sample_rate, track->bitrate, track->u.audio.channel, track->prefix, track->prefix);
			list_for_each(link, &track->root)
			{
				seg = list_entry(link, struct dash_segment_t, link);
				n += snprintf(playlist + n, bytes - n, "             <S t=\"%" PRId64 "\" d=\"%u\"/>\n", seg->timestamp, (unsigned int)seg->duration);
			}
			n += snprintf(playlist + n, bytes - n, s_footer);
		}
		else
		{
			assert(0);
		}
	}

	n += snprintf(playlist + n, bytes - n, "  </Period>\n</MPD>\n");
	
#if defined(DEBUG_INFO) 	
	printf("dash_mpd_playlist1 : mpd->count - %d\n", mpd->count);
#endif	
	
//#define	VAR (N_COUNT + 1)
	if(dash->count >= N_COUNT)
	{
			snprintf(dash->mpd_name, sizeof(dash->mpd_name), PREFIX_MDASH"%s%d-%d.mpd", "start", stream, dash->fname);
			
//#if defined(DEBUG_INFO_) 				
			//printf("1. mpd->name - %s, count - %d, stream - %d\n", dash->mpd_name, dash->count, stream);
//#endif	
			dash->fp = fopen(dash->mpd_name, "wb+");
			if (NULL == dash->fp)
			{
				printf("Error: mpd_name dash->fp\n");
				return 1; //TO do
			}	
			if(dash->fp != NULL)
			{
				if (n != fwrite(playlist, 1, n, dash->fp))
				{
					printf("Error: mpd->name - %s fwrite\n", dash->mpd_name);
					fclose(dash->fp);
					return n;
				}
			}
			dash->fname++;

	snprintf(dash->seg_name, sizeof(dash->seg_name), PREFIX_MDASH"%s-%" PRId64 ".m4v", track->prefix, seg->timestamp);
	//printf("2. seg_name - %s\n", seg_name);
	mpd_t *pmpd =(mpd_t *)malloc(sizeof(mpd_t));
	pmpd->path_file = strdup(dash->mpd_name); //!!!free
	pmpd->path_segfile = strdup(dash->seg_name); //!!!free
	pmpd->timestamp = get_uptime();
#if defined(DEBUG_INFO_DUMP) 	
	printf("2. Create addr - %p,  mpd - %s, %lld, path_segfile - %s\n", &pmpd->link, pmpd->path_file, pmpd->timestamp, pmpd->path_segfile);
#endif	
	//pthread_mutex_lock(&mtx);
	if(!stream)
	{
	//	pthread_mutex_lock(&mtx);
	//	pthread_mutex_lock(&pmpd->mpd_mtx);
		pthread_mutex_lock(&stm0_mpd_mtx);	
#if defined(DEBUG_INFO_DUMP) 			
		printf("list_insert_after 0 addr - %p,  mpd - %s, %lld, path_segfile - %s, stream - %d\n", &pmpd->link, pmpd->path_file, pmpd->timestamp, pmpd->path_segfile, stream);
#endif	
		list_insert_after(&pmpd->link, &gl_mpd_link_stream0); 
	//	pthread_mutex_unlock(&mtx);
	//	pthread_mutex_unlock(&pmpd->mpd_mtx);
		pthread_mutex_unlock(&stm0_mpd_mtx);	
	}
	else
	{
	//	pthread_mutex_lock(&mtx);
	//	pthread_mutex_lock(&pmpd->mpd_mtx);
		pthread_mutex_lock(&stm1_mpd_mtx);	
#if defined(DEBUG_INFO_DUMP) 			
		printf("list_insert_after 1 addr - %p,  mpd - %s, %lld, path_segfile - %s, stream - %d\n", &pmpd->link, pmpd->path_file, pmpd->timestamp, pmpd->path_segfile, stream);
#endif			
		list_insert_after(&pmpd->link, &gl_mpd_link_stream1); 
	//	pthread_mutex_unlock(&mtx);
		pthread_mutex_unlock(&stm1_mpd_mtx);	
		
	}						
		
	if(count_pthread > 0)
	{
		pthread_mutex_lock(&mtx);
		count_pthread--;
		if(!count_pthread)
		{
#if defined(DEBUG_INFO_DUMP) 			
			printf("send signal ==================>\n");
#endif			
			pthread_cond_signal(&cond);
		}
		pthread_mutex_unlock(&mtx);
	}

	}

	if(dash->fp != NULL && dash->count >= N_COUNT)
	{
//#ifdef DEBUG_dash_mpd_playlist
		//printf("fclose - %s\n", mpd_name);
//#endif
		//unlink(dash->name);
		fclose(dash->fp);
		
	}
	dash->count++;

#if defined(DEBUG_INFO) 	
	printf("dash_mpd_playlist2 - exit\n");
#endif	
	return n;
}
