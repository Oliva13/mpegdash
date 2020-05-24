#pragma once

#include <time.h>

#define NET_STTNGS_IS_CORRECT				0
#define NET_STTNGS_IP_WRONG					-11
#define NET_STTNGS_MASK_WRONG				-12
#define NET_STTNGS_GW_WRONG					-13
#define NET_STTNGS_IP_EQ_GW					-14
#define NET_STTNGS_IP_GW_IN_DIFF_SUBNETS	-15
#define NET_STTNGS_IP_EQ_SUBNET				-16
#define NET_STTNGS_GW_EQ_SUBNET				-17
#define NET_STTNGS_IP_EQ_BROADCAST			-18
#define NET_STTNGS_GW_EQ_BROADCAST			-19

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

typedef struct disk_info_s
{
	char name[16];
	char type[16];
	unsigned long total;
	unsigned long available;
	unsigned long free;
} disk_info_t;

#if defined (__cplusplus)
extern "C" {
#endif
void restart(char **argv);
#if 0
int update(char *res, int len);
#else
int update(char *res, int len, char* path);
#endif
int sendmail();
//char *md5sum(const char *str);

#define net_getdns1(addr) net_getdns(addr, 1)
#define net_getdns2(addr) net_getdns(addr, 2)

int net_getlinkstatus();
unsigned char *net_getmacaddr(char *);
unsigned long net_getipaddr(char *);
int net_setaddress(const char *);
int net_setnetmask(const char *);
unsigned long net_getnetmask(char *);
unsigned netmasklength(unsigned long);
int net_addgateway(const char *);
int net_getgateway(char *);
int net_setdns(const char *);
int net_getdns(char *sret, int num);
int net_sttngs_chk(char *ip, char *mask, char *gw);

void sys_getubootversion(char *);
void sys_getlinuxversion(char *);

typedef enum sync_flag_e {ST_START = 0, ST_STOP, ST_ONCE, ST_ONCE_BLOCKED} sync_flag_t;
int sys_synctime(const char *url, sync_flag_t flag);
int sys_settime(struct tm *time, int sec);
int sys_getlocaltime(struct tm *lt);
int sys_getutctime(struct tm *lt);
int sys_settimezone(const char *tz);
int sys_gettimezone(char *tz);

const char *tz_getposixstr(int idx);
const char *tz_getnamestr(int idx);

int sys_getdiskinfo(int, disk_info_t *);
int sys_mountdisk(const char *);
int sys_umountdisk(const char *);
int sys_formatdisk(const char *);

#if defined (__cplusplus)
}
#endif

