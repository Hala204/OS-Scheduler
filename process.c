#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char *argv[]) {
    initClk();
    init_remain_time();

    //printf("\nin Process.c remain_time = %d\n", *shm_remain_time);

    int prev_clk = getClk();
    while (*shm_remain_time > 0) {
        while (prev_clk == getClk());


        (*shm_remain_time)--;
        prev_clk = getClk();

    }

    destroyClk(false);
    return 0;
}
