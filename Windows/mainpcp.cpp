#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include "mainpcp.h"

using namespace std;

TCHAR PRODUCER_EXE_PATH[] = TEXT("producer.exe");
TCHAR CONSUMER_EXE_PATH[] = TEXT("consumer.exe");

HANDLE createdProcessHandles[TOTAL_PROCESS_COUNT + 1];
HANDLE createdThreadHandles[TOTAL_PROCESS_COUNT + 1];

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

  // 关闭共享映射文件句柄
  CloseHandle(sharedMappingFileHandle);

  return 0;
}
