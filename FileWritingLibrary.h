#pragma once

#define SCHEDULER_ENABLED


// Тип результата выполненной операции

typedef int		FW_RESULT_TYPE;

#define	FW_OK				 0
#define	FW_FAIL				-1
#define	FW_INSUFFICIENT_SPACE		-2
#define	FW_UNKNOWN_LOCAL_STORAGE	-3
#define	FW_UNMOUNTED_LOCAL_STORAGE	-4
#define	FW_FILE_WRITE_ERROR		-5
#define	FW_DISABLED_STREAM		-6
#define	FW_UNSUPPORTED_FILE_FORMAT	-7


