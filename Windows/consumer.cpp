#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>

using namespace std;
// 共享缓冲文件名
TCHAR SHM_NAME[] = TEXT("PCP_NAMED_SHARED_MEM");
// 三个信号量名
TCHAR SEM_EMPTY[] = TEXT("PCP_EMPTY");
TCHAR SEM_FULL[] = TEXT("PCP_FULL");
TCHAR SEM_MUTEX[] = TEXT("PCP_MUTEX");
// 缓冲区大小为 4
const int BUFFER_SIZE = 4;
const int SHARED_MEM_SIZE = 1024;

// 3 个生产者重复 4 次，4 个消费者重复 3 次
const int PRODUCE_CYCLES = 2;
const int CONSUME_CYCLES = 2;

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

// 从 "W", "S", "B" 三个字母中随机取一个
char getStock()
{
  srand(time(NULL));
  char stock[] = {'W', 'S', 'B'};
  return stock[rand() % 3];
}

// 生成一个 1~3 秒的随机时间
int getRandomDelay()
{
  srand(time(NULL));
  return rand() % 3 + 1;
}

int main(int argc, char const *argv[])
{
  cout << "[CONSUMER] Consumer ID: " << argv[1]
       << ", PID: " << GetCurrentProcessId() << endl;

  HANDLE conSharedMappingFileHandle;
  LPVOID conShmFilePointer;

  conSharedMappingFileHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
  if (conSharedMappingFileHandle == NULL)
  {
    cerr << "[ERR] Could not create file mapping object: " << GetLastError() << endl;
    return 1;
  }
  conShmFilePointer = MapViewOfFile(conSharedMappingFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (conShmFilePointer == NULL)
  {
    cerr << "[ERR] Could not map view of file: " << GetLastError() << endl;
    CloseHandle(conSharedMappingFileHandle);
    return 1;
  }

  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)conShmFilePointer;
  pcpSharedMemory->semEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY);
  pcpSharedMemory->semFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL);
  pcpSharedMemory->semMutex = OpenMutex(SEMAPHORE_ALL_ACCESS, FALSE, SEM_MUTEX);

  for (int i = 0; i < CONSUME_CYCLES; i++)
  {
    WaitForSingleObject(pcpSharedMemory->semFull, INFINITE);
    WaitForSingleObject(pcpSharedMemory->semMutex, INFINITE);
    Sleep(getRandomDelay() * 1000);

    cout << "[CONSUMER] " << i << " TEST!!!" << getStock() << "Please work!" << endl;

    ReleaseMutex(pcpSharedMemory->semMutex);
    ReleaseSemaphore(pcpSharedMemory->semEmpty, 1, NULL);
  }

  UnmapViewOfFile(conShmFilePointer);
  conShmFilePointer = NULL;
  return 0;
}
