#include <iostream>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
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
int semId = -1;
union semun semUnion;

// 初始化信号量标志（不是初始化信号量）
const int MUTEX = 0;
const int FULL = 1;
const int EMPTY = 2;

/* 维护一个缓冲区队列，一个头指针，一个尾指针
和一个标识符来表示队列是否为空 */
struct sharedMemory
{
  char buffer[BUFFER_SIZE];
  int head;
  int tail;
  bool isEmpty;
};

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

// 打印缓冲区中的内容
void printBufferStocks(struct sharedMemory *shMem)
{
  if (shMem->isEmpty == true)
  {
    cout << "-";
  }
  else
  {
    for (int i = (shMem->tail - 1 >= shMem->head) ? (shMem->tail - 1) : (shMem->tail - 1 + BUFFER_SIZE); i >= shMem->head; i--)
    {
      cout << shMem->buffer[i % BUFFER_SIZE];
    }
  }
}

// P 操作
void P(int semId, int semNum)
{
  struct sembuf semBuffer;

  // 申请一个资源，信号量减一
  semBuffer.sem_flg = 0;
  semBuffer.sem_num = semNum;
  semBuffer.sem_op = -1;

  semop(semId, &semBuffer, 1);
}

// V 操作
void V(int semId, int semNum)
{
  struct sembuf semBuffer;

  // 释放一个资源，信号量加一
  semBuffer.sem_flg = 0;
  semBuffer.sem_num = semNum;
  semBuffer.sem_op = 1;

  semop(semId, &semBuffer, 1);
}

void produce(int shmId, int producerId)
{
  sleep(getRandomDelay());

  // 获取一个随机字母作为货物
  char mostRecentStock = getStock();

  P(semId, EMPTY);
  P(semId, MUTEX);

  // 获取共享缓冲区指针
  struct sharedMemory *shmPtr;
  if ((shmPtr = (struct sharedMemory *)shmat(shmId, 0, 0)) == (void *)-1)
  {
    cerr << "[ERR] Producer get shared memory shmat failed." << endl;
  }

  // 将货物压入队尾
  shmPtr->buffer[shmPtr->tail] = mostRecentStock;
  shmPtr->tail = (shmPtr->tail + 1) % BUFFER_SIZE;
  shmPtr->isEmpty = false;

  // 打印共享缓冲区中的内容
  cout << "[PRODUCER] Producer " << producerId << " produce item: " << mostRecentStock << " | BUFFER: ";
  printBufferStocks(shmPtr);
  cout << endl;
  shmdt(shmPtr);

  V(semId, MUTEX);
  V(semId, FULL);
}

void consume(int shmId, int consumerId)
{
  P(semId, FULL);
  P(semId, MUTEX);

  // 随机等待一个 1~3 秒的时间
  sleep(getRandomDelay());

  // 获取共享缓冲区指针
  struct sharedMemory *shmPtr;
  if ((shmPtr = (struct sharedMemory *)shmat(shmId, 0, 0)) == (void *)-1)
  {
    cerr << "[ERR] Consumer get shared memory shmat failed." << endl;
  }

  // 将队列头的货物取出
  char mostRecentStock = shmPtr->buffer[shmPtr->head];
  shmPtr->head = (shmPtr->head + 1) % BUFFER_SIZE;

  if (shmPtr->head == shmPtr->tail)
  {
    shmPtr->isEmpty = true;
  }

  // 打印共享缓冲区中的内容
  cout << "[CONSUMER] Consumer " << consumerId << " consume item: " << mostRecentStock << " | BUFFER: ";
  printBufferStocks(shmPtr);
  cout << endl;
  shmdt(shmPtr);

  V(semId, MUTEX);
  V(semId, EMPTY);
}

int main(int argc, char const *argv[])
{
  // 初始化共享缓冲区
  int sharedMemoryId;
  struct sharedMemory *shmPointer;

  // 共享缓冲区 ID
  if ((sharedMemoryId = shmget(IPC_PRIVATE, BUFFER_SIZE, SHM_MODE)) < 0)
  {
    cerr << "[ERR] Create shared memory failed." << endl;
    exit(1);
  }

  // 共享缓存区指针
  if ((shmPointer = (struct sharedMemory *)shmat(sharedMemoryId, 0, 0)) == (void *)-1)
  {
    cerr << "[ERR] Get shared memory shmat failed." << endl;
    exit(1);
  }

  shmPointer->head = 0;
  shmPointer->tail = 0;
  shmPointer->isEmpty = true;

  // 创建信号量，创建两个同步信号量和一个互斥信号量
  if ((semId = semget(IPC_PRIVATE, 3, SEM_MODE)) < 0)
  {
    cerr << "[ERR] Create semaphore failed" << endl;
    exit(1);
  }

  // 互斥信号量 MUTEX 0 = 1
  semUnion.val = 1;
  if (semctl(semId, MUTEX, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] MUTEX init failed." << endl;
    exit(1);
  }

  // 同步信号量 FULL 1 = 0，表示当前缓冲区没有物品
  semUnion.val = 0;
  if (semctl(semId, FULL, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] FULL init failed." << endl;
    exit(1);
  }

  // 同步信号量 EMPTY = BUFFER_SIZE，表示可以进入缓冲区
  semUnion.val = BUFFER_SIZE;
  if (semctl(semId, EMPTY, SETVAL, semUnion) < 0)
  {
    cerr << "[ERR] EMPTY init failed." << endl;
    exit(1);
  }

  // 生产！（子进程 1）
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
        produce(sharedMemoryId, i);
      }
      exit(0);
    }
  }

  // 消费！（子进程 2）
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
        consume(sharedMemoryId, i);
      }
      exit(0);
    }
  }

  // 父进程等待两个子进程返回之后继续执行
  while (wait(0) > 0)
    ;
  shmdt(shmPointer);
  cout << "[MAIN PROCESS] Main process exit." << endl;

  return 0;
}
