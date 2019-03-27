#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include "mainpcp.h"

using namespace std;

// 生产者、消费者可执行文件
TCHAR PRODUCER_EXE_PATH[] = TEXT("producer.exe");
TCHAR CONSUMER_EXE_PATH[] = TEXT("consumer.exe");

// 生产者消费者子进程、线程的句柄集合
HANDLE createdProcessHandles[TOTAL_PROCESS_COUNT + 1];
HANDLE createdThreadHandles[TOTAL_PROCESS_COUNT + 1];

// 创建进程
void ProcessCreation(int processId)
{
  // 进程的启动信息结构体
  STARTUPINFO si;
  // 进程的创建信息结构体
  PROCESS_INFORMATION pi;
  // 初始化各个结构体
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  // 创建子进程，我们根据 processId 来区分创建生产者还是消费者
  // 如果 processId 位于 1~3 之间则创建生产者 producer
  if (processId >= PRODUCER_PROCESS_ID_HEAD && processId <= PRODUCER_PROCESS_ID_TAIL)
  {
    // TCHAR cmdLine[MAX_PATH];
    // _stprintf(cmdLine, "%s %d", PRODUCER_EXE_PATH, processId);

    // 获得创建进程返回值，对异常进行处理
    BOOL processCreationOk = CreateProcess(PRODUCER_EXE_PATH, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!processCreationOk)
    {
      cerr << "[ERR] Process creation failed with " << GetLastError() << "." << endl;
      return;
    }

    // 输出创建的进程信息
    cout << "[CREATED] Producer ID: " << processId
         << ", PID: " << GetCurrentProcessId() + processId << endl;
  }
  // 如果 processId 位于 4~7 之间则创建消费者 consumer
  else if (processId >= CONSUMER_PROCESS_ID_HEAD && processId <= CONSUMER_PROCESS_ID_TAIL)
  {
    // TCHAR cmdLine[MAX_PATH];
    // _stprintf(cmdLine, "%s %d", CONSUMER_EXE_PATH, processId);

    // 获得创建进程返回值，对异常进行处理
    BOOL processCreationOk = CreateProcess(CONSUMER_EXE_PATH, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!processCreationOk)
    {
      cerr << "[ERR] Process creation failed with " << GetLastError() << "." << endl;
      return;
    }

    // 输出创建的进程信息
    cout << "[CREATED] Consumer ID: " << processId
         << ", PID: " << GetCurrentProcessId() + processId << endl;
  }

  // 将创建的进程和线程返回的句柄存入集合，方便主进程监控
  createdProcessHandles[processId] = pi.hProcess;
  createdThreadHandles[processId] = pi.hThread;
}

int main(int argc, char const *argv[])
{
  // 主进程开始
  cout << "[MAIN] Main process start." << endl;

  // 共享主存的句柄变量
  HANDLE sharedMappingFileHandle;
  // 文件映射视图变量（指针）
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

  // 创建共享缓存区结构体指针
  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)shmFilePointer;

  // 初始化共享缓存区
  pcpSharedMemory->data.head = 0;
  pcpSharedMemory->data.tail = 0;

  // 初始化信号量，两个同步信号量一个互斥信号量
  // 同步信号量 EMPTY = BUFFER_SIZE，表示可以进入缓冲区
  HANDLE semEmpty = CreateSemaphore(NULL, BUFFER_SIZE, BUFFER_SIZE, SEM_EMPTY);
  // 同步信号量 FULL = 0，表示当前缓冲区没有物品
  HANDLE semFull = CreateSemaphore(NULL, 0, BUFFER_SIZE, SEM_FULL);
  // 互斥信号量 MUTEX = 1
  HANDLE semMutex = CreateMutex(NULL, FALSE, SEM_MUTEX);
  // 关闭文件映射上的视图的占用
  UnmapViewOfFile(shmFilePointer);
  shmFilePointer = NULL;

  // 下一个即将创建的子进程序号
  int nextProcessId = 1;

  // 创建 7 个子进程
  while (nextProcessId <= TOTAL_PROCESS_COUNT)
  {
    ProcessCreation(nextProcessId++);
    Sleep(500);
  }
  // 主进程依次等待 7 个子进程返回
  for (int i = 1; i <= TOTAL_PROCESS_COUNT; i++)
  {
    // 等待子进程返回
    WaitForSingleObject(createdProcessHandles[i], INFINITE);
    // 关闭进程和线程的句柄
    CloseHandle(createdProcessHandles[i]);
    CloseHandle(createdThreadHandles[i]);
  }
  cout << "[MAIN] All processed have exited." << endl;

  // 关闭共享映射文件句柄
  CloseHandle(sharedMappingFileHandle);

  return 0;
}
