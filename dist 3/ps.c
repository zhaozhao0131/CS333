#ifdef CS333_P2
#include "types.h"
#include "user.h"
#define HEADER "\nPID\tName    UID\tGID\tPPID\tElapsed\tCPU\tState\tSize\t\n"

void
elapsed_caculate(uint elapsed_ticks)
{
  printf(2,"\t%d.%d\t", (elapsed_ticks)/1000, (elapsed_ticks)%1000);
}
void
cpu_caculate(uint CPU_total_ticks)
{
  printf(2, "%d.%d\t", (CPU_total_ticks)/1000, (CPU_total_ticks)%1000);
}

int
main(void)
{
  int max = 16;
  struct uproc *pt = malloc(max *sizeof(struct uproc));
  int num = getprocs(max, pt);
  if (num < 0)
  {
    printf(1,"ERROR");
  }
  printf(1,HEADER);
  for(int i = 0; i < num; i++) {
    printf(2, "%d\t%s\t%d\t%d\t%d", pt[i].pid, pt[i].name, pt[i].uid, pt[i].gid, pt[i].ppid); //fix PPID
    elapsed_caculate(pt[i].elapsed_ticks);
    cpu_caculate(pt[i].CPU_total_ticks);
    printf(1,"%s\t%d\t\n", pt[i].state, pt[i].size);
  }
  free(pt);
  exit();

}
#endif
