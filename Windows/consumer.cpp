#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include "mainpcp.h"

using namespace std;

// 生成一个 1~3 秒的随机时间
int getRandomDelay()
{
  srand(time(NULL) + GetCurrentProcessId());
  return rand() % 3 + 1;
}

int main(int argc, char const *argv[])
{
  // 共享主存的句柄变量
  HANDLE conSharedMappingFileHandle;
  // 文件映射视图变量（指针）
  LPVOID conShmFilePointer;

  // 读取共享映射文件
  conSharedMappingFileHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, SHM_NAME);
  if (conSharedMappingFileHandle == NULL)
  {
    cerr << "[ERR] Consumer could not open file mapping object: " << GetLastError() << endl;
    return 1;
  }

  // 读取文件映射上的视图
  conShmFilePointer = MapViewOfFile(conSharedMappingFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (conShmFilePointer == NULL)
  {
    cerr << "[ERR] Consumer could not map view of file: " << GetLastError() << endl;
    CloseHandle(conSharedMappingFileHandle);
    return 1;
  }

  // 获取指向共享缓冲区指针
  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)conShmFilePointer;
  // 获取三个信号量的句柄
  HANDLE producerSemEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY);
  HANDLE producerSemFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL);
  HANDLE producerMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, SEM_MUTEX);

  // 消费者多次消费
  for (int i = 0; i < CONSUME_CYCLES; i++)
  {
    // 随机一个等待时间
    Sleep(getRandomDelay() * 1000);

    // 两个 P 操作，先申请 FULL，再申请 MUTEX
    WaitForSingleObject(producerSemFull, INFINITE);
    WaitForSingleObject(producerMutex, INFINITE);

    // 取出 buffer 头部物品
    char stock = pcpSharedMemory->data.buffer[pcpSharedMemory->data.head];
    // 将头部指针 head 向前移动一位
    pcpSharedMemory->data.head = (pcpSharedMemory->data.head + 1) % BUFFER_SIZE;

    // 判断 buffer 是否为空
    if (pcpSharedMemory->data.head == pcpSharedMemory->data.tail)
    {
      pcpSharedMemory->data.isEmpty = TRUE;
    }

    // 输出本次操作内容
    cout << "[CONSUMER] Consumer " << i + 4 << " consumes: " << stock << " | BUFFER: ";


    // 输出 buffer 内容
    if (pcpSharedMemory->data.isEmpty == TRUE)
    {
      cout << "-";
    }
    else
    {
      // 从前到后遍历 buffer 内容并输出
      for (int j = (pcpSharedMemory->data.tail - 1 >= pcpSharedMemory->data.head) ? (pcpSharedMemory->data.tail - 1) : (pcpSharedMemory->data.tail - 1 + BUFFER_SIZE); j >= pcpSharedMemory->data.head; j--)
      {
        cout << pcpSharedMemory->data.buffer[j % BUFFER_SIZE];
      }
    }

    cout << endl;


    // 两个 V 操作，先释放 MUTEX，再释放 EMPTY
    ReleaseMutex(producerMutex);
    ReleaseSemaphore(producerSemEmpty, 1, NULL);
  }

  // 关闭文件映射视图
  UnmapViewOfFile(conShmFilePointer);
  conShmFilePointer = NULL;
  // 关闭共享主存句柄
  CloseHandle(conSharedMappingFileHandle);

  return 0;
}
