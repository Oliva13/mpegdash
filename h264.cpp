#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-avc.h"
#include <unordered_map>
#include <sys/time.h>
#include <signal.h>
#include <iostream>
#include <string>
#include <cstring>
#include <functional>
#include <cassert>
#include <cinttypes>
#include <atomic>  
#include "FileWritingLibrary.h"
#include "recorder.h"
#include "mpdutils.h"

#define H264_NAL(v)	(v & 0x1F)

#define H264_NAL_IDR		5 // Coded slice of an IDR picture
#define H264_NAL_SPS		7 // Sequence parameter set
#define H264_NAL_PPS		8 // Picture parameter set

struct mpeg4_handle_t
{
	struct mpeg4_avc_t* avc;
	int errcode;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

typedef void(*h264_nalu_handler)(void* param, const void* nalu, size_t bytes);

const uint8_t* h264_startcode(const uint8_t *data, size_t bytes)
{
	size_t i;
	for (i = 2; i + 1 < bytes; i++)
	{
		if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
			return data + i + 1;
	}

	return NULL;
}

///@param[in] h264 H.264 byte stream format data(A set of NAL units)
void h264_stream(const void* h264, size_t bytes, h264_nalu_handler handler, void* param)
{
	ptrdiff_t n;
	const unsigned char* p, *next, *end;

	end = (const unsigned char*)h264 + bytes;
	p = h264_startcode((const unsigned char*)h264, bytes);

	while (p)
	{
		next = h264_startcode(p, end - p);
		if (next)
		{
			n = next - p - 3;
		}
		else
		{
			n = end - p;
		}

		while (n > 0 && 0 == p[n - 1]) n--; // filter tailing zero

		assert(n > 0);
		if (n > 0)
		{
			handler(param, p, (size_t)n);
		}

		p = next;
	}
}

void h264_handler(void* param, const void* nalu, size_t bytes)
{
	struct mpeg4_handle_t* mp4;	
	mp4 = (struct mpeg4_handle_t*)param;

	switch (((unsigned char*)nalu)[0] & 0x1f)
	{
		case H264_NAL_SPS:
			assert(bytes <= sizeof(mp4->avc->sps[mp4->avc->nb_sps].data));
			if (bytes <= sizeof(mp4->avc->sps[mp4->avc->nb_sps].data)
					&& mp4->avc->nb_sps < sizeof(mp4->avc->sps) / sizeof(mp4->avc->sps[0]))
			{
				mp4->avc->sps[mp4->avc->nb_sps].bytes = (uint16_t)bytes;
				memcpy(mp4->avc->sps[mp4->avc->nb_sps].data, nalu, bytes);
				++mp4->avc->nb_sps;
			}
			else
			{
				mp4->errcode = -1;
			}

			if (1 == mp4->avc->nb_sps)
			{
				mp4->avc->profile = mp4->avc->sps[0].data[1];
				mp4->avc->compatibility = mp4->avc->sps[0].data[2];
				mp4->avc->level = mp4->avc->sps[0].data[3];
			}
			break;

		case H264_NAL_PPS:
			assert(bytes <= sizeof(mp4->avc->pps[mp4->avc->nb_pps].data));
			if (bytes <= sizeof(mp4->avc->pps[mp4->avc->nb_pps].data)
					&& mp4->avc->nb_pps < sizeof(mp4->avc->pps) / sizeof(mp4->avc->pps[0]))
			{
				mp4->avc->pps[mp4->avc->nb_pps].bytes = (uint16_t)bytes;
				memcpy(mp4->avc->pps[mp4->avc->nb_pps].data, nalu, bytes);
				++mp4->avc->nb_pps;
			}
			else
			{
				mp4->errcode = -1;
			}
			break;

		case H264_NAL_IDR:
			mp4->avc->chroma_format_idc = 1;
			break;

			//case H264_NAL_SPS_EXTENSION:
			//case H264_NAL_SPS_SUBSET:
			//	break;
	}

	if (mp4->capacity >= mp4->bytes + bytes + 4)
	{
		mp4->ptr[mp4->bytes + 0] = (uint8_t)((bytes >> 24) & 0xFF);
		mp4->ptr[mp4->bytes + 1] = (uint8_t)((bytes >> 16) & 0xFF);
		mp4->ptr[mp4->bytes + 2] = (uint8_t)((bytes >> 8) & 0xFF);
		mp4->ptr[mp4->bytes + 3] = (uint8_t)((bytes >> 0) & 0xFF);
		memcpy(mp4->ptr + mp4->bytes + 4, nalu, bytes);
		mp4->bytes += bytes + 4;
	}
	else
	{
		mp4->errcode = -1;
	}
}

size_t mpeg4_annexbtomp4(struct mpeg4_avc_t* avc, const void* data, size_t bytes, void* out, size_t size)
{
	struct mpeg4_handle_t h;
	h.avc = avc;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	h.bytes = 0;
	h.errcode = 0;
	avc->chroma_format_idc = 0;
	avc->nb_pps = 0;
	avc->nb_sps = 0;
	avc->nalu = 4;
	h264_stream(data, bytes, h264_handler, &h);
	return 0 == h.errcode ? h.bytes : 0;
}

const uint8_t* h264_start_code(const uint8_t* ptr, const uint8_t* end)
{
	for (const uint8_t *p = ptr; p + 3 < end; p++)
	{
		if (0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00 == p[2] && 0x01 == p[3])))
			return p;
	}
	return end;
}

int h264_nal_type(const unsigned char* ptr)
{
	int i = 2;
	assert(0x00 == ptr[0] && 0x00 == ptr[1]);
	if (0x00 == ptr[2])
		++i;
	assert(0x01 == ptr[i]);
	return H264_NAL(ptr[i + 1]);
}
	

int h264_read_frame(unsigned long track_id, uint8_t* ptr, uint8_t* end, void* param_dash, int64_t pts_new)
{

//#ifdef DEBUG_h264_read_frame
	//printf("h264: track_id - %ld, ptr - %p, ptr+b - %p, dash - %p\n", track_id, ptr, end, param_dash);
//#endif	
	int track = -1;
	bool spspps = false;
	struct mpeg4_avc_t avc;
	//uint8_t m_extra_data[64 * 1024];
	
	int64_t pts = 0;
	
	//static int64_t pts = 0;
	
	int count_frame = 1; 
	int extra_data_size = 0;
	
	dash_playlist_t* dash = (dash_playlist_t*)param_dash;
		
#ifdef DEBUG_h264_read_frame	
	printf("<= h264_read_frame => dash->track_video - %d, pts - %lld\n", dash->track_video, pts_new);
#endif
	//const uint8_t* frame = offset; //h264_start_code(ptr, end);
	const uint8_t* frame = h264_start_code(ptr, end);
	const uint8_t* nalu = frame;
	
	//unsigned long sizebuf = (2 * 1024 * 1024);  
	
	//uint8_t *s_buffer = (uint8_t *)malloc(sizebuf);
	
	while (nalu < end)
	{
		const unsigned char* nalu2 = h264_start_code(nalu + 4, end);
		int nal_unit_type = h264_nal_type(nalu);
#ifdef DEBUG_h264_read_frame	
		printf("%d <= h264_read_frame => nal_unit_type - %lld, pts - %lld\n", count_frame, nal_unit_type, pts);
#endif
		
		//printf("count_frame - %d\n", count_frame);
		//count_frame++;
		
		assert(0 != nal_unit_type);
		
		//printf("========== nal_unit_type - 0x%x\n", nal_unit_type);
		
		if (nal_unit_type > 0 && nal_unit_type <= 5) 
		{
			//printf("nal_unit_type - 0x%x\n", nal_unit_type);
			//pts += 33; //
			// process one frame
			size_t bytes = nalu2 - frame;
			//size_t bytes = nalu2 - nalu;
			
			//pthread_mutex_lock(&mtx);
			
			//assert(bytes < sizeof(s_buffer));
			
			//pthread_mutex_lock(&mtx);
			
			assert(bytes < sizeof(dash->s_buffer));
			size_t n = mpeg4_annexbtomp4(&avc, frame, bytes, dash->s_buffer, sizeof(dash->s_buffer));
			//frame = nalu2; // next frame
			//fmp4_writer_write(mov, track, s_buffer, n, pts, pts, avc.chroma_format_idc ? MOV_AV_FLAG_KEYFREAME : 0);
			//mp4_onread(param, track_id, frame, bytes, pts, pts);
			pts = pts_new/1000; 
			//pts += 40; 
			//printf("mp4_onread - %lld\n", pts);
			mp4_onread(dash, track_id, dash->s_buffer, n, pts, pts);
			
			//pthread_mutex_unlock(&mtx);
			
			//nalu = nalu2; // next frame
			//nalu = nalu2; // next frame
			
//#ifdef DEBUG_h264_read_frame				
			//printf("h264_read_frame : track_id - %d, pts- %lld\n", track_id, pts);
			//fflush(stdout);
//#endif			
			//track_id++;
		    dash->track_video++;
		}
		nalu = nalu2;
    }
	
	//free(s_buffer);
	
	return extra_data_size;
}

int mpeg4_avc_decoder_configuration_record_save(const struct mpeg4_avc_t* avc, uint8_t* data, size_t bytes)
{
	uint8_t i;
	uint8_t *p = data;

	assert(0 < avc->nalu && avc->nalu <= 4);
	if (bytes < 7 || avc->nb_sps > 32) return -1;
	bytes -= 7;

	// AVCDecoderConfigurationRecord
	// ISO/IEC 14496-15:2010
	// 5.2.4.1.1 Syntax
	p[0] = 1; // configurationVersion
	p[1] = avc->profile; // AVCProfileIndication
	p[2] = avc->compatibility; // profile_compatibility
	p[3] = avc->level; // AVCLevelIndication
	p[4] = 0xFC | (avc->nalu - 1); // lengthSizeMinusOne: 3
	p += 5;

	// sps
	*p++ = 0xE0 | avc->nb_sps;
	for (i = 0; i < avc->nb_sps && bytes >= (size_t)avc->sps[i].bytes + 2; i++)
	{
		*p++ = (avc->sps[i].bytes >> 8) & 0xFF;
		*p++ = avc->sps[i].bytes & 0xFF;
		memcpy(p, avc->sps[i].data, avc->sps[i].bytes);

		p += avc->sps[i].bytes;
		bytes -= avc->sps[i].bytes + 2;
	}
	if (i < avc->nb_sps) return -1; // check length

	// pps
	*p++ = avc->nb_pps;
	for (i = 0; i < avc->nb_pps && bytes >= (size_t)avc->pps[i].bytes + 2; i++)
	{
		*p++ = (avc->pps[i].bytes >> 8) & 0xFF;
		*p++ = avc->pps[i].bytes & 0xFF;
		memcpy(p, avc->pps[i].data, avc->pps[i].bytes);

		p += avc->pps[i].bytes;
		bytes -= avc->pps[i].bytes + 2;
	}
	if (i < avc->nb_pps) return -1; // check length

	if (bytes >= 4)
	{
		if (avc->profile == 100 || avc->profile == 110 ||
				avc->profile == 122 || avc->profile == 244 || avc->profile == 44 ||
				avc->profile == 83 || avc->profile == 86 || avc->profile == 118 ||
				avc->profile == 128 || avc->profile == 138 || avc->profile == 139 ||
				avc->profile == 134)
		{
			*p++ = 0xFC | avc->chroma_format_idc;
			*p++ = 0xF8 | avc->bit_depth_luma_minus8;
			*p++ = 0xF8 | avc->bit_depth_chroma_minus8;
			*p++ = 0; // numOfSequenceParameterSetExt
		}
	}

	return (int)(p - data);
}

int init_h264_read_frame(void* param, unsigned long track_id, int frame_rate, int width, int height, const uint8_t* ptr, const uint8_t* end)
{
	//int64_t pts = 0;
	int track = -1;
	//bool spspps = false;
	struct mpeg4_avc_t avc;
	uint8_t m_extra_data[64 * 1024];
	
	//int8_t* offset;
	dash_playlist_t *dash = (dash_playlist_t *)param;
	//uint8_t s_buffer[1024 * 1024]; // !!!! To Do
	
	int extra_data_size = 0;
	
    const uint8_t* frame = h264_start_code(ptr, end);
	const uint8_t* nalu = frame;
	while (nalu < end)
	{
		const unsigned char* nalu2 = h264_start_code(nalu + 4, end);
		int nal_unit_type = h264_nal_type(nalu);
		assert(0 != nal_unit_type);
		if(nal_unit_type <= 5)
		{
			// process one frame
			size_t bytes = nalu2 - frame;
			assert(bytes < sizeof(dash->s_buffer));
			size_t n = mpeg4_annexbtomp4(&avc, frame, bytes, dash->s_buffer, sizeof(dash->s_buffer));
			frame = nalu2; // next frame
			//if (!spspps)
			//{
			//	//if (!avc.chroma_format_idc || avc.nb_sps < 1 || avc.sps[0].bytes < 4)
			//	//	continue; // wait for key frame
			//	//spspps = true;
				extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, m_extra_data, sizeof(m_extra_data));
				assert(extra_data_size > 0); // check buffer length
			//	//printf("extra_data_size - %d\n", extra_data_size);
				//*offset = (uint8_t*)nalu2;
				break;
			//}
			////fmp4_writer_write(mov, track, s_buffer, n, pts, pts, avc.chroma_format_idc ? MOV_AV_FLAG_KEYFREAME : 0);
			//pts += 40;
		}
		nalu = nalu2;
    }
	
	mp4_onvideo(param, track_id, MOV_OBJECT_H264, frame_rate, width, height, m_extra_data, extra_data_size);
	//mp4_onread(param, track_id, nalu,extra_data_size, 10, 10); //pts !!!
	//free(s_buffer);
	return 0;
}





