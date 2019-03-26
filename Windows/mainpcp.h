#include <tchar.h>
#include <windows.h>
const int SHARED_MEM_SIZE = 1024;

// 缓冲区大小为 4
const int BUFFER_SIZE = 4;
// 3 个生产者重复 4 次，4 个消费者重复 3 次
// 三个生产者进程 ID = 1、2、3
const int PRODUCER_PROCESS_ID_HEAD = 1;
const int PRODUCER_PROCESS_ID_TAIL = 3;
// 四个消费者进程 ID = 4、5、6、7
const int CONSUMER_PROCESS_ID_HEAD = 4;
const int CONSUMER_PROCESS_ID_TAIL = 7;
// 共 7 个进程
const int TOTAL_PROCESS_COUNT = 7;

// 共享缓冲文件名
TCHAR SHM_NAME[] = TEXT("BUFFER");
// 三个信号量名
TCHAR SEM_EMPTY[] = TEXT("PCP_EMPTY");
TCHAR SEM_FULL[] = TEXT("PCP_FULL");
TCHAR SEM_MUTEX[] = TEXT("PCP_MUTEX");

// 3 个生产者重复 4 次，4 个消费者重复 3 次
const int PRODUCE_CYCLES = 4;
const int CONSUME_CYCLES = 3;

/* 维护一个缓冲区队列，一个头指针，一个尾指针
 * 和一个标识符来表示队列是否为空
 */
struct sharedMemBuffer
{
  char buffer[BUFFER_SIZE];
  int head;
  int tail;
  bool isEmpty;
};

/* 共享主存区
 * 包括缓冲区和三个信号量
 */
struct sharedMemory
{
  struct sharedMemBuffer data;
  HANDLE semFull;
  HANDLE semEmpty;
  HANDLE semMutex;
};
