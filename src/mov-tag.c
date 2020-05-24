#include "mov-internal.h"

uint32_t mov_object_to_tag(uint8_t object)
{
	switch (object)
	{
	case MOV_OBJECT_H264:	return MOV_H264;
	case MOV_OBJECT_HEVC:	return MOV_HEVC;
	case MOV_OBJECT_MP4V:	return MOV_MP4V;
	case MOV_OBJECT_AAC:	return MOV_MP4A;
	case MOV_OBJECT_G711a:	return MOV_TAG('a', 'l', 'a', 'w');
	case MOV_OBJECT_G711u:	return MOV_TAG('u', 'l', 'a', 'w');
	default: return 0;
	}
}

uint8_t mov_tag_to_object(uint32_t tag)
{
	switch (tag)
	{
	case MOV_TAG('a', 'v', 'c', '2'):		// AVC2SampleEntry (ISO/IEC 14496-15:2010)
	case MOV_H264: return MOV_OBJECT_H264;	// AVCSampleEntry  (ISO/IEC 14496-15:2010)
	//case MOV_TAG('h', 'e', 'v', '1'):		// HEVCSampleEntry (ISO/IEC 14496-15:2013)
	case MOV_HEVC: return MOV_OBJECT_HEVC;  // HEVCSampleEntry (ISO/IEC 14496-15:2013)
	case MOV_MP4V: return MOV_OBJECT_MP4V;
	case MOV_MP4A: return MOV_OBJECT_AAC;
	case MOV_TAG('a', 'l', 'a', 'w'): return MOV_OBJECT_G711a;
	case MOV_TAG('u', 'l', 'a', 'w'): return MOV_OBJECT_G711u;
	default: return 0;
	}
}
