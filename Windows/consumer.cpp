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
  srand(time(NULL));
  return rand() % 3 + 1;
}

int main(int argc, char const *argv[])
{
  cout << "[CONSUMER] Consumer ID: " << argv[1]
       << ", PID: " << GetCurrentProcessId() << endl;

  HANDLE conSharedMappingFileHandle;
  LPVOID conShmFilePointer;

  conSharedMappingFileHandle = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, SHM_NAME);
  if (conSharedMappingFileHandle == NULL)
  {
    cerr << "[ERR] Consumer could not open file mapping object: " << GetLastError() << endl;
    return 1;
  }
  conShmFilePointer = MapViewOfFile(conSharedMappingFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  if (conShmFilePointer == NULL)
  {
    cerr << "[ERR] Consumer could not map view of file: " << GetLastError() << endl;
    CloseHandle(conSharedMappingFileHandle);
    return 1;
  }

  struct sharedMemory *pcpSharedMemory = (struct sharedMemory *)conShmFilePointer;
  HANDLE producerSemEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY);
  HANDLE producerSemFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL);
  HANDLE producerMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, SEM_MUTEX);

  for (int i = 0; i < CONSUME_CYCLES; i++)
  {
    WaitForSingleObject(producerSemFull, INFINITE);
    WaitForSingleObject(producerMutex, INFINITE);
    Sleep(getRandomDelay() * 1000);

    char stock = pcpSharedMemory->data.buffer[pcpSharedMemory->data.head];
    pcpSharedMemory->data.head = (pcpSharedMemory->data.head + 1) % BUFFER_SIZE;

    if (pcpSharedMemory->data.head == pcpSharedMemory->data.tail)
    {
      pcpSharedMemory->data.isEmpty = TRUE;
    }

    cout << "[CONSUMER] Consumer " << i << " consumes: " << stock << " | BUFFER: ";

    if (pcpSharedMemory->data.isEmpty == TRUE)
    {
      cout << "-";
    }
    else
    {
      for (int j = (pcpSharedMemory->data.tail - 1 >= pcpSharedMemory->data.head) ? (pcpSharedMemory->data.tail - 1) : (pcpSharedMemory->data.tail - 1 + BUFFER_SIZE); j >= pcpSharedMemory->data.head; j--)
      {
        cout << pcpSharedMemory->data.buffer[j % BUFFER_SIZE];
      }
    }

    cout << endl;

    ReleaseMutex(producerMutex);
    ReleaseSemaphore(producerSemEmpty, 1, NULL);
  }

  UnmapViewOfFile(conShmFilePointer);
  conShmFilePointer = NULL;
  CloseHandle(conSharedMappingFileHandle);

  return 0;
}
