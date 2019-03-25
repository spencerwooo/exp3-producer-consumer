#include <windows.h>
#include <time.h>
#include <iostream>

using namespace std;

// 3 个生产者重复 4 次，4 个消费者重复 3 次
const int PRODUCER_COUNT = 3;
const int CONSUMER_COUNT = 4;
const int PRODUCE_TIMES = 4;
const int CONSUME_TIMES = 3;
// 缓冲区大小为 4
const int BUFFER_SIZE = 4;

int getRandomDelay()
{
  srand(time(NULL));
  return rand() % 3 + 1;
}

char getRandomStock()
{
  srand(time(NULL));
  char stock[] = {'W', 'S', 'B'};
  return stock[rand() % 3];
}

void produce()
{
  char produceStock = getRandomStock();
  cout << "[PRODUCER] Produced item: " << produceStock << endl;
}

void consume()
{
  char consumeStock = getRandomStock();
  cout << "[CONSUMER] Consumed item: " << consumeStock << endl;
}

void P()
{
}

void V()
{
}

int main(int argc, char const *argv[])
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi[PRODUCER_COUNT];

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));
  for (int i = 0; i < PRODUCER_COUNT; i++) {
    // if (!CreateProcess())
  }

  return 0;
}
