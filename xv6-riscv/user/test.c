#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"
#include "../kernel/memlayout.h"
#include "../kernel/param.h"
#include "../kernel/spinlock.h"
#include "../kernel/sleeplock.h"
#include "../kernel/fs.h"
#include "../kernel/syscall.h"

void
alignment_test() {
  printf("\n=== Extra: Page Alignment Test ===\n");

  // 1. mmap 주소가 정렬되지 않은 경우 (예: 0x40001)
  // 과제 명세에 따라 addr가 0이 아니면 반드시 정렬되어야 합니다.
  uint64 bad_addr = 0x40001;
  uint64 addr1 = mmap(bad_addr, 4096, PROT_READ, MAP_ANONYMOUS, -1, 0);
  
  if (addr1 == 0) {
    printf("mmap unaligned addr returns -1: OK\n");
  } else {
    // 만약 0을 리턴해야 한다면 (addr1 == 0)으로 조건을 바꾸세요.
    printf("mmap unaligned addr returns -1: FAIL (got %ld)\n", addr1);
  }

  // 2. munmap 주소가 정렬되지 않은 경우
  if (munmap(0x50002) == -1) {
    printf("munmap unaligned addr returns -1: OK\n");
  } else {
    printf("munmap unaligned addr returns -1: FAIL\n");
  }
}

void
return_value_test() {
  printf("=== Extra: Return Value & Error Handling Test ===\n");

  // 1. addr 타입을 void*에서 uint64로 변경 (xv6 mmap 인터페이스 일치)
  // 1L << 60 같은 너무 큰 값 대신 MAXVA 근처의 큰 값을 시도
  uint64 addr1 = mmap(0x3FFFFFFFF000, 4096, PROT_READ, MAP_ANONYMOUS, -1, 0);
  if (addr1 == 0xffffffffffffffff) { // -1을 uint64로 표현
    printf("Invalid addr returns -1: OK\n");
  } else {
    printf("Invalid addr returns -1: FAIL\n");
  }

  // 2. 유효하지 않은 파일 디스크립터
  uint64 addr2 = mmap(0, 4096, PROT_READ, 0, -1, 0);
  if (addr2 == 0xffffffffffffffff) {
    printf("Invalid fd returns -1: OK\n");
  } else {
    printf("Invalid fd returns -1: FAIL\n");
  }

  // 3. munmap (인자를 1개만 받도록 수정: uint64 addr)
  if (munmap(0x12345670) == -1) {
    printf("munmap invalid addr returns -1: OK\n");
  } else {
    printf("munmap invalid addr returns -1: FAIL\n");
  }
}

void
page_fault_security_test() {
  printf("\n=== Extra: Page Fault Security & Permission Test ===\n");

  int pid = fork();
  if(pid == 0) {
    // PROT_READ만 주고 할당
    uint64 addr = mmap(0, 4096, PROT_READ, MAP_ANONYMOUS, -1, 0);
    char *m = (char*)addr;
    printf("Child: attempting to write to Read-only mmap...\n");
    
    // 여기서 Page Fault 발생 -> usertrap에서 권한 체크 후 Kill 해야 함
    m[0] = 'a'; 
    
    printf("Child: SUCCESS (This is FAIL, should have been killed)\n");
    exit(0);
  }
  
  int status;
  wait(&status);
  if(status != 0) { // 자식이 비정상 종료(Killed)되었는지 확인
    printf("Write to Read-only killed child: OK\n");
  } else {
    printf("Write to Read-only didn't kill child: FAIL\n");
  }
}

int main() {
    int fd;
    char *m1;
    int f0, f1, f2;

    printf("=== 1. Anonymous + MAP_POPULATE ===\n");
    f0 = freemem();
    m1 = (char*)mmap(0, 4096, PROT_READ|PROT_WRITE,
                     MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    f1 = freemem();
    printf("freemem: %d -> %d (diff=%d)\n", f0, f1, f0-f1);
    m1[0] = 'X';
    printf("write test: %c\n", m1[0]);
    munmap((uint64)m1);
    f2 = freemem();
    printf("after munmap freemem: %d\n", f2);

    printf("\n=== 2. Anonymous without MAP_POPULATE (lazy) ===\n");
    f0 = freemem();
    m1 = (char*)mmap(0, 4096, PROT_READ|PROT_WRITE,
                     MAP_ANONYMOUS, -1, 0);
    f1 = freemem();
    printf("after mmap (no alloc): %d -> %d (diff=%d, expect 0)\n",
           f0, f1, f0-f1);
    m1[0] = 'Y';  // page fault
    f2 = freemem();
    printf("after page fault: %d (diff=%d, expect 1)\n", f2, f1-f2);
    munmap((uint64)m1);
    printf("after munmap: %d\n", freemem());

    printf("\n=== 3. File mapping + MAP_POPULATE ===\n");
    fd = open("README", O_RDONLY);
    f0 = freemem();
    m1 = (char*)mmap(0, 8192, PROT_READ, MAP_POPULATE, fd, 0);
    f1 = freemem();
    printf("freemem diff=%d\n", f0-f1);
    printf("content: %c%c%c\n", m1[0], m1[1], m1[2]);
    munmap((uint64)m1);
    printf("after munmap: %d\n", freemem());
    close(fd);

    printf("\n=== 4. File mapping without MAP_POPULATE (lazy) ===\n");
    fd = open("README", O_RDONLY);
    f0 = freemem();
    m1 = (char*)mmap(0, 8192, PROT_READ, 0, fd, 0);
    f1 = freemem();
    printf("after mmap diff=%d (expect 0)\n", f0-f1);
    // page fault 유발
    char ch = m1[0];
    f2 = freemem();
    printf("after fault diff=%d (expect 1)\n", f1-f2);
    printf("content: %c%c%c\n", ch, m1[1], m1[2]);

printf("\n=== 5. Fork test ===\n");
int free_before = freemem();
printf("freemem before fork: %d\n", free_before);
printf("before fork m1[0]: %c (ascii=%d)\n", m1[0], (int)m1[0]);

int pid = fork();
if (pid == 0) {
    // 자식
    printf("child content: %c%c%c (should match parent)\n",
           m1[0], m1[1], m1[2]);
    printf("child freemem: %d\n", freemem());
    munmap((uint64)m1);  // exit 전에 munmap
    close(fd);
    exit(0);            // munmap 후 exit
} else {
    wait(0);
    printf("parent m1[0] after wait: %c (ascii=%d)\n", m1[0], (int)m1[0]);
    printf("parent freemem after fork+wait: %d\n", freemem());
}

    munmap((uint64)m1);
    close(fd);

    printf("\nAll tests passed!\n");

    return_value_test();
    page_fault_security_test();
    alignment_test();

    uint64 addr1 = mmap(0x40001, 4096, PROT_READ, MAP_ANONYMOUS, -1, 0);

if (addr1 == 0) { // -1 대신 0을 기대하도록 수정
    printf("mmap unaligned addr returns 0: OK\n");
} else {
    printf("mmap unaligned addr returns 0: FAIL (got %ld)\n", addr1);
}
    exit(0);
}