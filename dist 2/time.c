#ifdef CS333_P2
#include "types.h"
#include "user.h"


int
main(int t, char * argv[])
{
    if (t < 1) {
      printf(1, "(null) ran is 0.000second\n");
        exit();
    }
    uint start = 0, end = 0, pid = 0,
         elapsed_time = 0, sec = 0, 
	 milisec_ten = 0, milisec_hund = 0, 
	 milisec_thou = 0;

    if (t == 1) {
        printf(1, "%s ran in 0.000 seconds\n", argv[0]);
        exit();
    }
    start = uptime();
    pid = fork(); 
    if (pid > 0) {
        pid = wait(); 
    }
    else if (pid == 0) {
        exec(argv[1], (argv+1)); 
        exit();
    }
    end = uptime(); 
    elapsed_time = (end - start);

    sec = (elapsed_time / 1000); 
    milisec_ten = ((elapsed_time %= 1000) / 100); 
    milisec_hund = ((elapsed_time %= 100) / 10);
    milisec_thou = (elapsed_time %= 10);

    printf(1, "%s ran in %d.%d%d%d seconds\t\n", argv[1], sec, milisec_ten, milisec_hund, milisec_thou);
    exit();
}


#endif
