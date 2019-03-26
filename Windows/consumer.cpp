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
  pcpSharedMemory->semEmpty = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_EMPTY);
  pcpSharedMemory->semFull = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, SEM_FULL);
  pcpSharedMemory->semMutex = OpenMutex(SEMAPHORE_ALL_ACCESS, FALSE, SEM_MUTEX);

  for (int i = 0; i < CONSUME_CYCLES; i++)
  {
    WaitForSingleObject(pcpSharedMemory->semFull, INFINITE);
    WaitForSingleObject(pcpSharedMemory->semMutex, INFINITE);
    Sleep(getRandomDelay() * 1000);

    pcpSharedMemory->shmIndex++;
    char stock = pcpSharedMemory->data.buffer[pcpSharedMemory->data.head];
    pcpSharedMemory->data.head = (pcpSharedMemory->data.head + 1) % BUFFER_SIZE;

    if (pcpSharedMemory->data.head == pcpSharedMemory->data.tail) {
      pcpSharedMemory->data.isEmpty = 1;
    }

    cout << "[CONSUMER] Consumer " << i << " consumes: " << stock << "| BUFFER: ";

    if (pcpSharedMemory->data.isEmpty)
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

    ReleaseMutex(pcpSharedMemory->semMutex);
    ReleaseSemaphore(pcpSharedMemory->semEmpty, 1, NULL);
  }

  UnmapViewOfFile(conShmFilePointer);
  conShmFilePointer = NULL;
  return 0;
}
