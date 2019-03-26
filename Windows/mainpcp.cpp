#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>

using namespace std;

const int SHARED_MEM_SIZE = 1024;

// 缓冲区大小为 4
const int BUFFER_SIZE = 4;
// 3 个生产者重复 4 次，4 个消费者重复 3 次
// 三个生产者进程 ID = 1、2、3
const int PRODUCER_PROCESS_ID_HEAD = 1;
const int PRODUCER_PROCESS_ID_TAIL = 1;
// 四个消费者进程 ID = 4、5、6、7
const int CONSUMER_PROCESS_ID_HEAD = 2;
const int CONSUMER_PROCESS_ID_TAIL = 2;
// 共 7 个进程
const int TOTAL_PROCESS_COUNT = 2;

TCHAR PRODUCER_EXE_PATH[] = TEXT("producer.exe");
TCHAR CONSUMER_EXE_PATH[] = TEXT("consumer.exe");

// 共享缓冲文件名
TCHAR SHM_NAME[] = TEXT("PCP_NAMED_SHARED_MEM");
// 三个信号量名
TCHAR SEM_EMPTY[] = TEXT("PCP_EMPTY");
TCHAR SEM_FULL[] = TEXT("PCP_FULL");
TCHAR SEM_MUTEX[] = TEXT("PCP_MUTEX");

HANDLE createdProcessHandles[TOTAL_PROCESS_COUNT + 1];
HANDLE createdThreadHandles[TOTAL_PROCESS_COUNT + 1];

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
  int shmIndex;
  struct sharedMemBuffer data;
  HANDLE semFull;
  HANDLE semEmpty;
  HANDLE semMutex;
};

void ProcessCreation(int processId)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  if (processId >= PRODUCER_PROCESS_ID_HEAD && processId <= PRODUCER_PROCESS_ID_TAIL)
  {
    TCHAR cmdLine[MAX_PATH];
    _stprintf(cmdLine, "%s %d", PRODUCER_EXE_PATH, processId);
    BOOL processCreationOk = CreateProcess(PRODUCER_EXE_PATH, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!processCreationOk)
    {
      cerr << "[ERR] Process creation failed with " << GetLastError() << "." << endl;
      return;
    }
  }
  else if (processId >= CONSUMER_PROCESS_ID_HEAD && processId <= CONSUMER_PROCESS_ID_TAIL)
  {
    TCHAR cmdLine[MAX_PATH];
    _stprintf(cmdLine, "%s %d", CONSUMER_EXE_PATH, processId);
    BOOL processCreationOk = CreateProcess(CONSUMER_EXE_PATH, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!processCreationOk)
    {
      cerr << "[ERR] Process creation failed with " << GetLastError() << "." << endl;
      return;
    }
  }

  createdProcessHandles[processId] = pi.hProcess;
  createdThreadHandles[processId] = pi.hThread;
}

int main(int argc, char const *argv[])
{
  HANDLE sharedMappingFileHandle;
  LPVOID shmFilePointer;

  // 创建共享映射文件句柄
  sharedMappingFileHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHARED_MEM_SIZE, SHM_NAME);
  if (sharedMappingFileHandle == NULL)
  {
    cerr << "[ERR] Could not create file mapping object " << GetLastError() << endl;
    return 1;
  }

  // 创建文件映射上的视图
  shmFilePointer = MapViewOfFile(sharedMappingFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
  if (shmFilePointer == NULL)
  {
    cerr << "[ERR] Could not map view of file " << GetLastError() << endl;
    CloseHandle(sharedMappingFileHandle);
    return 1;
  }

  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)shmFilePointer;

  // 初始化共享缓存区
  pcpSharedMemory->data.head = 0;
  pcpSharedMemory->data.tail = 0;
  pcpSharedMemory->shmIndex = 0;

  // 初始化信号量，两个同步信号量一个互斥信号量
  // 同步信号量 EMPTY = BUFFER_SIZE，表示可以进入缓冲区
  pcpSharedMemory->semEmpty = CreateSemaphore(NULL, BUFFER_SIZE, BUFFER_SIZE, SEM_EMPTY);
  // 同步信号量 FULL = 0，表示当前缓冲区没有物品
  pcpSharedMemory->semFull = CreateSemaphore(NULL, 0, BUFFER_SIZE, SEM_FULL);
  // 互斥信号量 MUTEX = 1
  pcpSharedMemory->semMutex = CreateMutex(NULL, 1, SEM_MUTEX);
  // 关闭文件映射上的视图的占用
  UnmapViewOfFile(shmFilePointer);
  // 关闭共享映射文件句柄
  CloseHandle(sharedMappingFileHandle);
  // 下一个即将创建的子进程序号
  int nextProcessId = 1;

  // 创建 7 个子进程
  while (nextProcessId <= TOTAL_PROCESS_COUNT)
  {
    ProcessCreation(nextProcessId++);
  }
  for (int i = 1; i <= TOTAL_PROCESS_COUNT; i++)
  {
    WaitForSingleObject(createdProcessHandles[i], INFINITE);
    CloseHandle(createdProcessHandles[i]);
    CloseHandle(createdThreadHandles[i]);
  }
  cout << "[MAIN] All processed have exited." << endl;

  return 0;
}
