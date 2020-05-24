#include "dash-mpd.h"
#include "dash-proto.h"
#include "mov-reader.h"
#include "mov-format.h"
#include "fmp4-writer.h"
#include "mpeg4-hevc.h"

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

#define H265_VPS		32
#define H265_SPS		33
#define H265_PPS		34


// TO DO
//static uint8_t s_buffer[2 * 1024 * 1024];

//#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define H265_NAL(v)	((v >> 1) & 0x3F)

struct hevc_handle_t
{
	struct mpeg4_hevc_t* hevc;
	int errcode;

	uint8_t* ptr;
	size_t bytes;
	size_t capacity;
};

typedef void(*hevc_nalu_handler)(void* param, const uint8_t* nalu, size_t bytes);

#define H265_VPS		32
#define H265_SPS		33
#define H265_PPS		34
#define H265_PREFIX_SEI 39
#define H265_SUFFIX_SEI 40

static uint8_t* w32(uint8_t* p, uint32_t v)
{
	*p++ = (uint8_t)(v >> 24);
	*p++ = (uint8_t)(v >> 16);
	*p++ = (uint8_t)(v >> 8);
	*p++ = (uint8_t)v;
	return p;
}

static uint8_t* w16(uint8_t* p, uint16_t v)
{
	*p++ = (uint8_t)(v >> 8);
	*p++ = (uint8_t)v;
	return p;
}

static const uint8_t* hevc_startcode(const uint8_t *data, size_t bytes) //h264 equ!!!!
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
///@param[in] hevc H.265 byte stream format data(A set of NAL units)
static void hevc_stream(const uint8_t* hevc, size_t bytes, hevc_nalu_handler handler, void* param)
{
	ptrdiff_t n;
	const uint8_t* p, *next, *end;

	end = hevc + bytes;
	p = hevc_startcode(hevc, bytes);

	while (p)
	{
		next = hevc_startcode(p, end - p);
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

static void hevc_profile_tier_level(const uint8_t* nalu, size_t bytes, uint8_t maxNumSubLayersMinus1, struct mpeg4_hevc_t* hevc)
{
	uint8_t i;
	uint8_t sub_layer_profile_present_flag[8];
	uint8_t sub_layer_level_present_flag[8];

	if (bytes < 12)
		return;

	hevc->general_profile_space = (nalu[0] >> 6) & 0x03;
	hevc->general_tier_flag = (nalu[0] >> 5) & 0x01;
	hevc->general_profile_idc = nalu[0] & 0x1f;

	hevc->general_profile_compatibility_flags = 0;
	hevc->general_profile_compatibility_flags |= nalu[1] << 24;
	hevc->general_profile_compatibility_flags |= nalu[2] << 16;
	hevc->general_profile_compatibility_flags |= nalu[3] << 8;
	hevc->general_profile_compatibility_flags |= nalu[4];

	hevc->general_constraint_indicator_flags = 0;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[5]) << 40;
	hevc->general_constraint_indicator_flags |= ((uint64_t)nalu[6]) << 32;
	hevc->general_constraint_indicator_flags |= nalu[7] << 24;
	hevc->general_constraint_indicator_flags |= nalu[8] << 8;
	hevc->general_constraint_indicator_flags |= nalu[9] << 8;
	hevc->general_constraint_indicator_flags |= nalu[10];

	hevc->general_level_idc = nalu[11];

	// TODO:
	for (i = 0; i < maxNumSubLayersMinus1; i++)
	{
		sub_layer_profile_present_flag[i];
		sub_layer_level_present_flag[i];
	}
}

static size_t hevc_rbsp_decode(const uint8_t* nalu, size_t bytes, uint8_t* sodb)
{
	size_t i, j;
	for (j = i = 0; i < bytes; i++)
	{
		if (i + 2 < bytes && 0 == nalu[i] && 0 == nalu[i + 1] && 0x03 == nalu[i + 2])
		{
			sodb[j++] = nalu[i];
			sodb[j++] = nalu[i + 1];
			i += 2;
		}
		else
		{
			sodb[j++] = nalu[i];
		}
	}
	return j;
}

static void hevc_handler(void* param, const uint8_t* nalu, size_t bytes)
{
	uint8_t* sodb;
	uint8_t nal_type;
	size_t sodb_bytes;
	struct hevc_handle_t* mp4;
	mp4 = (struct hevc_handle_t*)param;

	nal_type = (nalu[0] >> 1) & 0x3f;
	switch (nal_type)
	{
		case H265_VPS:
		case H265_SPS:
		case H265_PPS:
			sodb = mp4->hevc->numOfArrays > 0 ? mp4->hevc->nalu[mp4->hevc->numOfArrays - 1].data + mp4->hevc->nalu[mp4->hevc->numOfArrays - 1].bytes : mp4->hevc->data;
			if (mp4->hevc->numOfArrays >= sizeof(mp4->hevc->nalu) / sizeof(mp4->hevc->nalu[0])
					|| sodb + bytes >= mp4->hevc->data + sizeof(mp4->hevc->data))
			{
				mp4->errcode = -1;
				//  add printf
				return;
			}

			sodb_bytes = hevc_rbsp_decode(nalu, bytes, sodb);

			if (nal_type == H265_VPS)
			{
				uint8_t vps_max_sub_layers_minus1 = (nalu[3] >> 1) & 0x07;
				uint8_t vps_temporal_id_nesting_flag = nalu[3] & 0x01;
				mp4->hevc->numTemporalLayers = MAX(mp4->hevc->numTemporalLayers, vps_max_sub_layers_minus1 + 1);
				mp4->hevc->temporalIdNested = (mp4->hevc->temporalIdNested || vps_temporal_id_nesting_flag) ? 1 : 0;
				hevc_profile_tier_level(sodb + 6, sodb_bytes - 6, vps_max_sub_layers_minus1, mp4->hevc);
			}
			else if (nal_type == H265_SPS)
			{
				// TODO:
				mp4->hevc->chromaFormat; // chroma_format_idc
				mp4->hevc->bitDepthLumaMinus8; // bit_depth_luma_minus8
				mp4->hevc->bitDepthChromaMinus8; // bit_depth_chroma_minus8

				// TODO: vui_parameters
				mp4->hevc->min_spatial_segmentation_idc; // min_spatial_segmentation_idc
			}
			else if (nal_type == H265_PPS)
			{
				// TODO:
				mp4->hevc->parallelismType; // entropy_coding_sync_enabled_flag
			}

			mp4->hevc->nalu[mp4->hevc->numOfArrays].type = nal_type;
			mp4->hevc->nalu[mp4->hevc->numOfArrays].bytes = (uint16_t)bytes;
			mp4->hevc->nalu[mp4->hevc->numOfArrays].array_completeness = 1;
			mp4->hevc->nalu[mp4->hevc->numOfArrays].data = sodb;
			memcpy(mp4->hevc->nalu[mp4->hevc->numOfArrays].data, nalu, bytes);
			++mp4->hevc->numOfArrays;
			//  add printf
			return;

		case 16: // BLA_W_LP
		case 17: // BLA_W_RADL
		case 18: // BLA_N_LP
		case 19: // IDR_W_RADL
		case 20: // IDR_N_LP
		case 21: // CRA_NUT
		case 22: // RSV_IRAP_VCL22
		case 23: // RSV_IRAP_VCL23
			mp4->hevc->constantFrameRate = 0x04; // irap frame
			break;

		default:
			break;
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

size_t hevc_annexbtomp4(struct mpeg4_hevc_t* hevc, const void* data, size_t bytes, void* out, size_t size)
{
	struct hevc_handle_t h;
	h.hevc = hevc;
	h.ptr = (uint8_t*)out;
	h.capacity = size;
	h.bytes = 0;
	h.errcode = 0;

	hevc->configurationVersion = 1;
	hevc->lengthSizeMinusOne = 3; // 4 bytes
	hevc->numTemporalLayers = 0;
	hevc->temporalIdNested = 0;
	hevc->general_profile_compatibility_flags = 0xffffffff;
	hevc->general_constraint_indicator_flags = 0xffffffffffULL;
	hevc->min_spatial_segmentation_idc = 0;
	hevc->chromaFormat = 1; // 4:2:0
	hevc->bitDepthLumaMinus8 = 0;
	hevc->bitDepthChromaMinus8 = 0;
	hevc->avgFrameRate = 0;
	hevc->constantFrameRate = 0;
	hevc->numOfArrays = 0;

	hevc_stream((const uint8_t*)data, bytes, hevc_handler, &h);
	return 0 == h.errcode ? h.bytes : 0;
}


int mpeg4_hevc_decoder_configuration_record_save(const struct mpeg4_hevc_t* hevc, uint8_t* data, size_t bytes)
{
	uint16_t n;
	uint8_t i, j, k;
	uint8_t *ptr, *end;
	uint8_t *p = data;
	uint8_t array_completeness;
	const uint8_t nalu[] = {H265_VPS, H265_SPS, H265_PPS, H265_PREFIX_SEI, H265_SUFFIX_SEI};

	assert(hevc->lengthSizeMinusOne <= 3);
	end = data + bytes;
	if (bytes < 23)
		return 0; // don't have enough memory

	// HEVCDecoderConfigurationRecord
	// ISO/IEC 14496-15:2017
	// 8.3.3.1.2 Syntax
	assert(1 == hevc->configurationVersion);
	data[0] = hevc->configurationVersion;

	// general_profile_space + general_tier_flag + general_profile_idc
	data[1] = ((hevc->general_profile_space & 0x03) << 6) | ((hevc->general_tier_flag & 0x01) << 5) | (hevc->general_profile_idc & 0x1F);

	// general_profile_compatibility_flags
	w32(data + 2, hevc->general_profile_compatibility_flags);

	// general_constraint_indicator_flags
	w32(data + 6, (uint32_t)(hevc->general_constraint_indicator_flags >> 16));
	w16(data + 10, (uint16_t)hevc->general_constraint_indicator_flags);
	
	// general_level_idc
	data[12] = hevc->general_level_idc;
	
	// min_spatial_segmentation_idc
	w16(data + 13, 0xF000 | hevc->min_spatial_segmentation_idc);

	data[15] = 0xFC | hevc->parallelismType;
	data[16] = 0xFC | hevc->chromaFormat;
	data[17] = 0xF8 | hevc->bitDepthLumaMinus8;
	data[18] = 0xF8 | hevc->bitDepthChromaMinus8;
	w16(data + 19, hevc->avgFrameRate);
	data[21] = (hevc->constantFrameRate << 6) | ((hevc->numTemporalLayers & 0x07) << 3) | ((hevc->temporalIdNested & 0x01) << 2) | (hevc->lengthSizeMinusOne & 0x03);
//	data[22] = hevc->numOfArrays;

	p = data + 23;
	for (k = i = 0; i < sizeof(nalu)/sizeof(nalu[0]); i++)
	{
		ptr = p + 3;
		for (n = j = 0; j < hevc->numOfArrays; j++)
		{
			assert(hevc->nalu[j].type == ((hevc->nalu[j].data[0] >> 1) & 0x3F));
			if(nalu[i] != hevc->nalu[j].type)
				continue;

			if (ptr + 2 + hevc->nalu[j].bytes > end)
				return 0; // don't have enough memory

			array_completeness = hevc->nalu[j].array_completeness;
			assert(hevc->nalu[i].data + hevc->nalu[j].bytes <= hevc->data + sizeof(hevc->data));
			w16(ptr, hevc->nalu[j].bytes);
			memcpy(ptr + 2, hevc->nalu[j].data, hevc->nalu[j].bytes);
			ptr += 2 + hevc->nalu[j].bytes;
			n++;
		}

		if (n > 0)
		{
			// array_completeness + NAL_unit_type
			p[0] = (array_completeness << 7) | (nalu[i] & 0x3F);
			w16(p + 1, n);
			p = ptr;
			k++;
		}
	}

	data[22] = k;

	return p - data;
}

static inline const uint8_t* h265_start_code(const uint8_t* ptr, const uint8_t* end) // equ 264!!!
{
	for (const uint8_t *p = ptr; p + 3 < end; p++)
	{
		if (0x00 == p[0] && 0x00 == p[1] && (0x01 == p[2] || (0x00 == p[2] && 0x01 == p[3])))
			return p;
	}
	return end;
}

static inline int h265_nal_type(const unsigned char* ptr) // equ 264!!!
{
	int i = 2;
	assert(0x00 == ptr[0] && 0x00 == ptr[1]);
	if (0x00 == ptr[2])
		++i;
	assert(0x01 == ptr[i]);
	return H265_NAL(ptr[i + 1]); // not equ 264!!!
}

int h265_read_frame(unsigned long track_id, uint8_t* ptr, uint8_t* end, void* param_dash, int64_t pts_new)
{

//#ifdef DEBUG_h264_read_frame
	//printf("h264: track_id - %ld, ptr - %p, ptr+b - %p, dash - %p\n", track_id, ptr, end, param_dash);
//#endif	
	bool spspps = false;
	struct mpeg4_hevc_t hevc;
	int64_t pts = 0;
	int extra_data_size = 0;
	
	dash_playlist_t* dash = (dash_playlist_t*)param_dash;
#ifdef DEBUG_h264_read_frame	
	printf("<= h264_read_frame => dash->track_video - %d, pts - %lld\n", dash->track_video, pts);
#endif
	const uint8_t* frame = h265_start_code(ptr, end);
	const uint8_t* nalu = frame;
	while (nalu < end)
	{
		const unsigned char* nalu2 = h265_start_code(nalu + 4, end);
		int nal_unit_type = h265_nal_type(nalu);
#ifdef DEBUG_h264_read_frame	
		printf("%d <= h264_read_frame => nal_unit_type - %lld, pts - %lld\n", count_frame, nal_unit_type, pts);
#endif
	    //printf("dash->track_video - %d\n", dash->track_video);
    	assert(0 != nal_unit_type);
    	if(nal_unit_type <= 31)
		{
			//printf("nal_unit_type - 0x%x\n", nal_unit_type);
			// process one frame
			size_t bytes = nalu2 - frame;
			assert(bytes < sizeof(dash->s_buffer));
			size_t n = hevc_annexbtomp4(&hevc, frame, bytes, dash->s_buffer, sizeof(dash->s_buffer));
			pts = pts_new/1000; 
			
			//if(nal_unit_type >= 16 && nal_unit_type <= 23)
			//{
			//	printf("found keyframe h265 ...\n");
			//}
					
			mp4_onread(dash, track_id, dash->s_buffer, n, pts, pts);	
//#ifdef DEBUG_h264_read_frame				
			//printf("h264_read_frame : track_id - %d, pts- %lld\n", track_id, pts);
			//fflush(stdout);
//#endif			
		    dash->track_video++;
		}
		nalu = nalu2;
    }
	return extra_data_size;
}


int init_h265_read_frame(void* param, unsigned long track_id, int frame_rate, int width, int height, const uint8_t* ptr, const uint8_t* end)
{
	//int64_t pts = 0;
	int track = -1;
	//bool spspps = false;
	struct mpeg4_hevc_t hevc;
	uint8_t m_extra_data[64 * 1024];
	
	//int8_t* offset;
	
	int extra_data_size = 0;
	
	dash_playlist_t *dash = (dash_playlist_t *)param;
	
	memset(&hevc, 0, sizeof(hevc)); // new !!!
	    
	const uint8_t* frame = h265_start_code(ptr, end);
	const uint8_t* nalu = frame;
	while (nalu < end)
	{
		const unsigned char* nalu2 = h265_start_code(nalu + 4, end);
		int nal_unit_type = h265_nal_type(nalu);
		//nal_unit_type <= 31   
		if(nal_unit_type <= 31)   //16 <= nal_unit_type && nal_unit_type <= 23)
		{
			// process one frame
			size_t bytes = nalu2 - frame;
			assert(bytes < sizeof(dash->s_buffer));
			size_t n = hevc_annexbtomp4(&hevc, frame, bytes, dash->s_buffer, sizeof(dash->s_buffer));
			frame = nalu2; // next frame
			//if (!spspps)
			//{
			//	//if (!avc.chroma_format_idc || avc.nb_sps < 1 || avc.sps[0].bytes < 4)
			//	//	continue; // wait for key frame
			//	//spspps = true;
			extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, m_extra_data, sizeof(m_extra_data));
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
	
	mp4_onvideo(param, track_id, MOV_OBJECT_HEVC, frame_rate, width, height, m_extra_data, extra_data_size);
	//mp4_onread(param, track_id, nalu,extra_data_size, 10, 10); //pts !!!
	return 0;
}





