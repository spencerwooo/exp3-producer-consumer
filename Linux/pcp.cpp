#include <iostream>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// 有些平台根据 POSIX 规范未定义信号量 Union，我们自己定义
#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
#else
union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};
#endif

// 共享内存大小 1024
#define SHARED_MEM_SIZE 1024
// 600 权限：读写
#define SHM_MODE 0600
#define SEM_MODE 0600

using namespace std;

// 3 个生产者重复 4 次，4 个消费者重复 3 次
const int PRODUCER_COUNT = 3;
const int CONSUMER_COUNT = 4;
const int PRODUCE_TIMES = 4;
const int CONSUME_TIMES = 3;
// 缓冲区大小为 4
const int BUFFER_SIZE = 4;

// 初始化共享内存 ID，信号量 ID 和信号量集合
int sharedMemoryId = -1;
int semSetId = -1;
union semun semUnion;

// 生成一个 1~3 秒的随机时间
int getRandomDelay()
{
  srand(time(NULL));
  return rand() % 3 + 1;
}

// 从 "W", "S", "B" 三个字母中随机取一个
char getStock()
{
  srand(time(NULL));
  char stock[] = {'W', 'S', 'B'};
  return stock[rand() % 3];
}

void *createSharedMemory(size_t size)
{
  int protection = PROT_READ | PROT_WRITE;
  int visibility = MAP_ANONYMOUS | MAP_SHARED;
  return mmap(NULL, size, protection, visibility, 0, 0);
}

void produce()
{
  char mostRecentStock = getStock();
  cout << "[PRODUCER] Produce item: " << mostRecentStock << endl;
}

void consume()
{
  char mostRecentStock = getStock();
  cout << "[CONSUMER] Consume item: " << mostRecentStock << endl;
}

void P(int semSetId, int semNum)
{
  struct sembuf semBuffer;

  // 申请一个资源，信号量减一
  semBuffer.sem_flg = SEM_UNDO;
  semBuffer.sem_num = semNum;
  semBuffer.sem_op = -1;

  if (semop(semSetId, &semBuffer, 1) < 0)
  {
    cerr << "[ERR] P operation failed." << endl;
    exit(1);
  }
}

void V(int semSetId, int semNum)
{
  struct sembuf semBuffer;

  // 释放一个资源，信号量加一
  semBuffer.sem_flg = SEM_UNDO;
  semBuffer.sem_num = semNum;
  semBuffer.sem_op = 1;

  if (semop(semSetId, &semBuffer, 1) < 0)
  {
    cerr << "[ERR] V operation failed." << endl;
    exit(1);
  }
}

int main(int argc, char const *argv[])
{
  void *sharedMemory = createSharedMemory(SHARED_MEM_SIZE);
  char *initMemory = "-";
  memcpy(sharedMemory, initMemory, sizeof(sharedMemory));

  // 创建信号量，创建两个同步信号量和一个互斥信号量
  if ((semSetId = semget(IPC_PRIVATE, 3, SEM_MODE)) < 0)
  {
    cerr << "[ERR] Create semaphore failed" << endl;
    exit(1);
  }

  // 互斥信号量 0，表示缓冲区大小为 BUFFER_SIZE = 4
  semUnion.val = BUFFER_SIZE;
  if (semctl(semSetId, 0, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] Semaphore 1 init failed." << endl;
    exit(1);
  }

  // 同步信号量 1 为 0，表示当前缓冲区没有物品
  semUnion.val = 0;
  if (semctl(semSetId, 1, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] Semaphore 2 init failed." << endl;
    exit(1);
  }

  // 同步信号量 2 为 1，表示可以进入缓冲区
  semUnion.val = 1;
  if (semctl(semSetId, 2, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] Semphore 3 init failed." << endl;
    exit(1);
  }

  // 消费！（子进程 1）
  for (int i = 0; i < CONSUMER_COUNT; i++)
  {
    pid_t consumerProcess = fork();
    if (consumerProcess < 0)
    {
      cerr << "[ERR] Consumer process creation failed." << endl;
      exit(1);
    }
    if (consumerProcess == 0)
    {
      cout << "[CONSUMER] Consumer ID: " << i << ", PID = " << getpid() << endl;
      for (int j = 0; j < CONSUME_TIMES; j++)
      {
        // 随机等待一个 1~3 秒的时间
        sleep(getRandomDelay());
        P(semSetId, 1);
        P(semSetId, 2);
        consume();
        V(semSetId, 2);
        V(semSetId, 0);
      }
      exit(0);
    }
  }

  // 生产！（子进程 2）
  for (int i = 0; i < PRODUCER_COUNT; i++)
  {
    pid_t producerProcess = fork();
    if (producerProcess < 0)
    {
      cerr << "[ERR] Producer process creation failed." << endl;
      exit(1);
    }
    if (producerProcess == 0)
    {
      cout << "[PRODUCER] Producer ID: " << i << ", PID = " << getpid() << endl;
      for (int j = 0; j < PRODUCE_TIMES; j++)
      {
        sleep(getRandomDelay());
        P(semSetId, 0);
        P(semSetId, 2);
        produce();
        V(semSetId, 2);
        V(semSetId, 1);
      }
      exit(0);
    }
  }

  // 父进程等待两个子进程返回之后继续执行
  while (wait(0) > 0)
    ;

  return 0;
}
