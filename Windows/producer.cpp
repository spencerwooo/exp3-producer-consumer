#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include "mainpcp.h"

using namespace std;

// 从 "W", "S", "B" 三个字母中随机取一个
char getStock()
{
  srand(time(NULL) + GetCurrentProcessId());
  char stock[] = {'W', 'S', 'B'};
  return stock[rand() % 3];
}

// 生成一个 1~3 秒的随机时间
int getRandomDelay()
{
  srand(time(NULL) + GetCurrentProcessId());
  return rand() % 3 + 1;
}

int main(int argc, char const *argv[])
{
  // 共享主存的句柄变量
  HANDLE proSharedMappingFileHandle;
  // 文件映射视图变量（指针）
  LPVOID proShmFilePointer;

  // 读取共享映射文件
  proSharedMappingFileHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, SHM_NAME);
  if (proSharedMappingFileHandle == NULL)
  {
    cerr << "[ERR] Producer could not open file mapping object: " << GetLastError() << endl;
    return 1;
  }

  // 读取文件映射上的视图
  proShmFilePointer = MapViewOfFile(proSharedMappingFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (proShmFilePointer == NULL)
  {
    cerr << "[ERR] Producer could not map view of file: " << GetLastError() << endl;
    CloseHandle(proSharedMappingFileHandle);
    return 1;
  }

  // 获取指向共享缓冲区指针
  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)proShmFilePointer;
  // 获取三个信号量的句柄
  HANDLE consumerSemEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY);
  HANDLE consumerSemFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL);
  HANDLE consumerMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, SEM_MUTEX);

  // 生产者重复生产
  for (int i = 0; i < PRODUCE_CYCLES; i++)
  {
    // 随机一个等待时间
    Sleep(getRandomDelay() * 1000);

    // 两个 P 操作，先申请 EMPTY，再申请 MUTEX
    WaitForSingleObject(consumerSemEmpty, INFINITE);
    WaitForSingleObject(consumerMutex, INFINITE);

    // 获得一个随机字母作为货物
    char stock = getStock();

    // 将货物置入 buffer 尾部
    pcpSharedMemory->data.buffer[pcpSharedMemory->data.tail] = stock;
    // 将尾部指针向前移动一位
    pcpSharedMemory->data.tail = (pcpSharedMemory->data.tail + 1) % BUFFER_SIZE;
    // 将 buffer 为空置 FALSE
    pcpSharedMemory->data.isEmpty = FALSE;

    // 输出本次操作内容
    cout << "[PRODUCER] Producer " << i + 1 << " produces: " << stock << " | BUFFER: ";

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

    // 两个 V 操作，先释放 MUTEX，再释放 FULL
    ReleaseMutex(consumerMutex);
    ReleaseSemaphore(consumerSemFull, 1, NULL);
  }

  // 关闭文件映射视图
  UnmapViewOfFile(proShmFilePointer);
  proShmFilePointer = NULL;
  // 关闭共享主存句柄
  CloseHandle(proSharedMappingFileHandle);

  return 0;
}
