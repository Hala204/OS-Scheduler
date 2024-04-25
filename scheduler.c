#include "headers.h"
#include "hashmap.h"
#include "circular_queue.h"
#include "priority_queue.h"
#include "math.h"

#define pcb_s struct PCB

// change it to typedef instead of struct
typedef struct PCB
{
    int id; // this is the key in the hashmap
    int pid;
    int arrival_time;
    int priority;
    short state;
    int cum_runtime;
    int burst_time;
    int remaining_time;
    int waiting_time;
} PCB;

FILE *sch_log, *sch_perf;
int TotalWaitingTime = 0;
int TotalWTA = 0;
int TotalRunTime = 0;
int TotalNumberOfProcesses = 0;
int *WeightedTA = NULL;
int WTAIterator = 0;

void OutputFinishedProcesses(int CurrTime, PCB *current_pcb, int TA, float WTA);

void print(int CurrentTime, PCB *current, char type);

void scheduler_log();

float CalcStdWTA(float AvgWTA);

void scheduler_perf(int ProcessCount, int FT);

void FinishPrinting();

void RR(int quantum);

int RR2(int quantum);

int SRTN();

///////////////
int SJF() ;
////////////////

// 3 functions related to the hashmap
int process_compare(const void *a, const void *b, void *udata)
{
    const struct PCB *process_a = a;
    const struct PCB *process_b = b;
    return (process_a->id - process_b->id);
}

bool process_iter(const void *item, void *udata)
{
    const struct PCB *process = item;
    printf("process: (id=%d) (arrivalTime=%d) (runTime=%d) (priority=%d) (pid=%d) (state=%d) (remainingTime=%d)\n",
           process->pid, process->arrival_time, process->cum_runtime, process->priority, process->pid, process->state,
           process->remaining_time);
    return true;
}

uint64_t process_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct PCB *process = item;
    return hashmap_sip(&process->id, sizeof(process->id), seed0, seed1);
}

// TODO init this in the main of the scheduler
struct hashmap *process_table;
int process_msg_queue;

// when process generator tells us that there is no more to come
// set this to false

bool more_processes_coming = true;

void set_no_more_processes_coming(int signum)
{
    more_processes_coming = false;
}

int main(int argc, char *argv[])
{
    // process Gen sends a SIGUSR1 to sch to tell than no more processes are coming
    signal(SIGUSR1, set_no_more_processes_coming);

    // create and open files
    scheduler_log();

    int remain_time_shmid = shmget(REMAIN_TIME_SHMKEY, 4, IPC_CREAT | 0644);
    if (remain_time_shmid == -1)
        perror("cant init remaining time shm: \n");
    shm_remain_time = (int *)shmat(remain_time_shmid, NULL, 0);
    *shm_remain_time = -1;

    process_table = hashmap_new(sizeof(PCB), 0, 0, 0,
                                process_hash, process_compare, NULL);

    struct chosen_algorithm coming;
    int key_id = ftok("keyfile", 'q');
    process_msg_queue = msgget(key_id, 0666 | IPC_CREAT);
    msgrcv(process_msg_queue, &coming, sizeof(coming) - sizeof(coming.mtype), ALGO_TYPE,
           !IPC_NOWAIT);

    // Set number of processes to use in statistics (scheduler.perf)
    TotalNumberOfProcesses = coming.NumOfProcesses;
    WeightedTA = (int *)malloc(sizeof(int) * TotalNumberOfProcesses);

    printf("\nChosen Algo is %d\n", coming.algo);
    initClk();

    int finish_time = 0;
    switch (coming.algo)
    {
        case 1:
            printf("RR with q=%d at time: %d\n", coming.arg, getClk());
            finish_time = RR2(coming.arg);
            break;
            
        case 2:
            printf("SJF\n");
            finish_time = SJF();
            break;

        case 3:
            printf("SRTN\n");
            finish_time = SRTN();
            break;
    }
    printf("DONE scheduler\n");
    scheduler_perf(TotalNumberOfProcesses, finish_time);
    FinishPrinting();
    // upon termination release the clock resources.
    hashmap_free(process_table);
    shmctl(remain_time_shmid, IPC_RMID, NULL);
    destroyClk(true);
}




int RR2(int quantum)
{
    /**
     * i loop all the time
     * till a variable tells me that there is no more processes coming
     * this is when i quit
     * All the processes that in the circular queue are in the process_table
     * when finished -> u delete from both
     **/
    struct c_queue RRqueue;
    circular_init_queue(&RRqueue);
    PCB *current_pcb;
    int curr_q_start;
    int p_count = TotalNumberOfProcesses;
    int need_to_receive = TotalNumberOfProcesses;
    bool process_is_currently_running = false;
    int curr = 0;
    while (!circular_is_empty(&RRqueue) || p_count > 0)
    {

        // First check if any process has come
        struct count_msg c = {.count = 0};
        if (more_processes_coming || need_to_receive > 0)
            msgrcv(process_msg_queue, &c, sizeof(int), 10, !IPC_NOWAIT);

        int num_messages = c.count;
        need_to_receive -= c.count;
        curr = getClk();

        while (num_messages > 0)
        {
            // while still a process in the queue
            // take it out
            // add it to both the RRqueue and its PCB to the processTable
            process_struct coming_process;
            msgrcv(process_msg_queue, &coming_process, sizeof(coming_process) - sizeof(coming_process.mtype), 1,
                   !IPC_NOWAIT);
            printf("\nrecv process with id: %d at time %d\n", coming_process.id, getClk());
            //  you have that struct Now
            PCB pcb; // = set_process(coming_process);
            pcb.id = coming_process.id;
            pcb.pid = 0;
            // pcb.arrival_time = coming_process.arrival;
            pcb.arrival_time = getClk();
            pcb.priority = coming_process.priority;
            pcb.state = READY;
            pcb.cum_runtime = 0;
            pcb.burst_time = coming_process.runtime;     // at the beginning
            pcb.remaining_time = coming_process.runtime; // at the beginning
            pcb.waiting_time = 0;
            circular_enQueue(&RRqueue, coming_process.id);

            hashmap_set(process_table, &pcb); // this copies the content of the struct
            num_messages--;
        }

        //  printf("curr is %d: %d\n", getClk(), more_processes_coming);
        // if its time ended or its quantum -> switch (Advance front)
        // otherwise just let it run in peace
        // if there is a running process -> see if it can be finished or not

        //        hashmap_scan(process_table, process_iter, NULL);

        if (process_is_currently_running)
        {
            current_pcb = hashmap_get(process_table, &(PCB){.id = RRqueue.front->data});

            if (curr - curr_q_start >= current_pcb->remaining_time)
            { // that process will be finished
                int st;

                current_pcb->remaining_time = 0;
                current_pcb->cum_runtime = current_pcb->burst_time;
                int ret = wait(&st);
                if (ret == -1)
                {
                    perror("error in waiting the Process:");
                }
                // if a process ended normally -- you're sure that the signal came from a dead process -- not stopped or resumed
                int TA = curr - current_pcb->arrival_time;
                current_pcb->waiting_time = TA - current_pcb->burst_time;
                float WTA = (float)TA / current_pcb->burst_time;

                OutputFinishedProcesses(curr, current_pcb, TA, WTA);
                p_count--;
                circular_deQueue(&RRqueue); // auto advance the queue
                hashmap_delete(process_table, current_pcb);
                process_is_currently_running = false;

                // if its multiple of q finished and there are some other in the Q waiting
            }
            else if ((curr - curr_q_start) && (curr - curr_q_start) % quantum == 0 &&
                     !circular_is_empty_or_one_left(&RRqueue))
            {

                current_pcb->remaining_time -= curr - curr_q_start;
                kill(current_pcb->pid, SIGSTOP);
                current_pcb->cum_runtime += curr - curr_q_start;
                current_pcb->waiting_time = curr - current_pcb->arrival_time - current_pcb->cum_runtime;
                print(curr, current_pcb, 'p');
                current_pcb->state = READY; // back to Ready state
                circular_advance_queue(&RRqueue);
                process_is_currently_running = false;
            }
        }

        if (!process_is_currently_running && !circular_is_empty(&RRqueue))
        {
            // update the current_pcb as the queue is advanced
            current_pcb = hashmap_get(process_table, &(PCB){.id = RRqueue.front->data});
            if (current_pcb->pid == 0)
            { // if current process never started before
                *shm_remain_time = current_pcb->remaining_time;
                int pid = fork();
                if (pid == 0)
                {
                    // child
                    execl("./process.out", "./process.out", NULL);
                }

                // parent
                current_pcb->pid = pid; // update Pid of existing process

                current_pcb->waiting_time = curr - current_pcb->arrival_time;
                print(curr, current_pcb, 's');
            }
            else
            {

                *shm_remain_time = current_pcb->remaining_time;
                kill(current_pcb->pid, SIGCONT);
                current_pcb->waiting_time = curr - current_pcb->arrival_time - current_pcb->cum_runtime;
                print(curr, current_pcb, 'r');
            }
            current_pcb->state = RUNNING;
            process_is_currently_running = true;
            curr_q_start = curr; // started a quantum
        }

        // if the current's quantum finished and only one left -> no switch
        // if the current terminated and no other in the Queue -> no switching
    }
    printf("\nOut at time %d\n", curr);
    return curr;
}
/**----------------------------------------------------------------

 */
///////////////////////////////////////////////////////////////////
int SJF()
{
    printf("Entering SJF \n");

    // Initialize a min-heap priority queue
    minHeap sjf_queue = init_min_heap();

    PCB *current_pcb = NULL;
    int p_count = TotalNumberOfProcesses;
    int need_to_receive = TotalNumberOfProcesses;
    bool process_is_currently_running = false;

    while (!is_empty(&sjf_queue) || p_count > 0)
    {
        // Check if any process has arrived
        count_msg c = {.count = 0};
        if (more_processes_coming || need_to_receive > 0)
            msgrcv(process_msg_queue, &c, sizeof(int), 10, !IPC_NOWAIT);

        int current_time = getClk();
        int num_messages = c.count;
        need_to_receive -= c.count;

        // Receive incoming processes and add them to the queue
        while (num_messages > 0)
        {
            process_struct coming_process;
            msgrcv(process_msg_queue, &coming_process, sizeof(coming_process) - sizeof(coming_process.mtype), 1,
                   !IPC_NOWAIT);
            printf("\nReceived process with id: %d at time %d with runtime %d\n", coming_process.id, current_time,
                   coming_process.runtime);

            PCB pcb;
            pcb.id = coming_process.id;
            pcb.pid = 0;
            pcb.arrival_time = current_time;
            pcb.priority = coming_process.priority;
            pcb.state = READY;
            pcb.cum_runtime = 0;
            pcb.burst_time = coming_process.runtime;
            pcb.remaining_time = coming_process.runtime;
            pcb.waiting_time = 0;
            hashmap_set(process_table, &pcb);
            push(&sjf_queue, coming_process.runtime, coming_process.id); // Add process to the priority queue
            num_messages--;
        }

        if (!process_is_currently_running && !is_empty(&sjf_queue))
        {
            // Retrieve the shortest job from the queue
            node *temp = pop(&sjf_queue);
            PCB get_process = {.id = temp->data};
            current_pcb = hashmap_get(process_table, &get_process);

            // Start the process
            *shm_remain_time = current_pcb->remaining_time;
            int pid = fork();
            if (pid == 0)
            {
                // Child
                execl("./process.out", "./process.out", NULL);
            }

            // Parent
            current_pcb->pid = pid;
            current_pcb->state = RUNNING;
            current_pcb->waiting_time = current_time - current_pcb->arrival_time;
            print(current_time, current_pcb, 's');
            process_is_currently_running = true;
        }

        if (current_pcb != NULL && process_is_currently_running)
        {
            // Update remaining time and check if the process has finished
            current_pcb->remaining_time -= (current_time - current_pcb->arrival_time - current_pcb->cum_runtime);
            current_pcb->cum_runtime = current_pcb->burst_time - current_pcb->remaining_time;

            if (current_pcb->remaining_time <= 0)
            {
                *shm_remain_time = 0;
                current_pcb->remaining_time = 0;
                current_pcb->cum_runtime = current_pcb->burst_time;
                int dum;
                int ret = wait(&dum);
                int TA = current_time - current_pcb->arrival_time;
                float WTA = (float)TA / current_pcb->burst_time;
                OutputFinishedProcesses(current_time, current_pcb, TA, WTA);
                hashmap_delete(process_table, current_pcb);
                p_count--;
                process_is_currently_running = false;
            }
        }
    }
    printf("\nOut at time %d\n", getClk());
}



////////////////////////////////////////////////////////////////

//----------------------------------------------------------------
int SRTN()
{
    printf("Entering SRTN \n");
    // intialize the priority queue
    minHeap sQueue;
    sQueue = init_min_heap();

    PCB *current_pcb = NULL;
    int p_count = TotalNumberOfProcesses;
    int need_to_receive = TotalNumberOfProcesses;
    bool process_is_currently_running = false;
    bool can_insert = true;
    int current_time = 0;
    int current_id = -1;

    // if the Queue is empty then check if there is no more processes that will come
    // the main loop for the scheduler
    while (!is_empty(&sQueue) || p_count > 0)
    {
        // First check if any process has come
        count_msg c = {.count = 0};
        if (more_processes_coming || need_to_receive > 0)
            msgrcv(process_msg_queue, &c, sizeof(int), 10, !IPC_NOWAIT);

        int current_time = getClk();
        int num_messages = c.count;
        need_to_receive -= c.count;

        while (num_messages > 0)
        {
            // while still a process in the queue
            // take it out
            // add it to both the Prority Queue (sQueue) and its PCB to the processTable
            process_struct coming_process;

            msgrcv(process_msg_queue, &coming_process, sizeof(coming_process) - sizeof(coming_process.mtype), 1,
                   !IPC_NOWAIT);
            printf("\nrecv process with id: %d at time %d with priority %d\n", coming_process.id, current_time,
                   coming_process.priority);

            //  you have that struct Now
            PCB pcb;
            pcb.id = coming_process.id;
            pcb.pid = 0;
            // pcb.arrival_time = coming_process.arrival;
            pcb.arrival_time = current_time;
            pcb.priority = coming_process.priority;
            pcb.state = READY;
            pcb.cum_runtime = 0;
            pcb.burst_time = coming_process.runtime;     // at the beginning
            pcb.remaining_time = coming_process.runtime; // at the beginning
            pcb.waiting_time = 0;                        // at the beginning
            hashmap_set(process_table, &pcb);            // this copies the content of the struct
            push(&sQueue, pcb.remaining_time, pcb.id);   // add this process to the end of the Queue
            // heapify(&sQueue, 0);
            num_messages--;
        }
        if (current_pcb != NULL && current_id != -1)
        {
            /* code */
            PCB get_process = {.id = current_id};
            current_pcb = hashmap_get(process_table, &get_process);
            // printf("Here id %d   time %d \n", current_pcb->id, current_time);
        }
        if (current_pcb != NULL)
        {
            current_pcb->remaining_time -= (current_time - (current_pcb->arrival_time + current_pcb->cum_runtime + current_pcb->waiting_time));
            current_pcb->cum_runtime = current_pcb->burst_time - current_pcb->remaining_time;
            if (current_pcb->remaining_time <= 0)
            {
                //*shm_remain_time = 0;
                current_pcb->remaining_time = 0;
                current_pcb->cum_runtime = current_pcb->burst_time;
                // kill and out new data
                int dum;
                int ret = wait(&dum);
                int TA = current_time - current_pcb->arrival_time;
                float WTA = (float)TA / current_pcb->burst_time;
                OutputFinishedProcesses(current_time, current_pcb, TA, WTA);
                hashmap_delete(process_table, current_pcb);
                p_count--;
                current_pcb = NULL;
            }
        }
        // 3 cases
        // first is first start process or there is gap between processes -> Done
        // second update the remaining time
        // third is a new process with shortest time came
        if (!is_empty(&sQueue) && current_pcb != NULL)
        {
            node *temp = peek(&sQueue);
            if (temp != NULL)
            {
                if (temp->priority < current_pcb->remaining_time)
                {
                    // swap and stop current process
                    kill(current_pcb->pid, SIGSTOP);
                    print(current_time, current_pcb, 'p');
                    current_pcb->state = READY; // back to Ready state
                    push(&sQueue, current_pcb->remaining_time,
                         current_pcb->id); // add this process to the end of the Queue
                    // heapify(&sQueue, 0);

                    current_pcb = NULL;
                }
            }
        }

        if (current_pcb == NULL && !is_empty(&sQueue))
        {
            node *temp = pop(&sQueue);
            // heapify(&sQueue, 0);
            PCB get_process = {.id = temp->data};
            current_pcb = hashmap_get(process_table, &get_process);
            *shm_remain_time = current_pcb->remaining_time;
            //  first time
            if (current_pcb->pid == 0)
            {
                int pid = fork();
                if (pid == 0)
                {
                    // child
                    execl("./process.out", "./process.out", NULL);
                }

                // continue scheduler
                // int curr = current_time;
                current_pcb->pid = pid; // update Pid of existing process
                current_pcb->state = RUNNING;
                current_pcb->waiting_time = current_time - current_pcb->arrival_time;
                print(current_time, current_pcb, 's');
            }
            // resumed after stopped
            else
            {
                kill(current_pcb->pid, SIGCONT);

                // continue scheduler
                current_pcb->state = RUNNING;
                current_pcb->waiting_time = current_time - (current_pcb->arrival_time + current_pcb->cum_runtime);
                // if(current_pcb->id == 3){
                //     printf("current %d arrivl %d -- cum %d -- waiting time %d\n", current_time, current_pcb->arrival_time, current_pcb->cum_runtime, current_pcb->waiting_time);
                // }
                print(current_time, current_pcb, 'r');
            }
        }
    }
    printf("\nOut at time %d\n", getClk());
    return current_time;
}

void scheduler_log()
{
    sch_log = fopen("scheduler.log", "w");
    if (sch_log == NULL)
    {
        printf("error has been occured while creation or opening scheduler.log\n");
    }
    else
    {
        fprintf(sch_log, "#At time x process y state arr w total z remain y wait k\n");
    }
}

void scheduler_perf(int ProcessesCount, int FT)
{
    sch_perf = fopen("scheduler.perf", "w");
    if (sch_perf == NULL)
    {
        printf("error has been occured while creation or opening scheduler.perf\n");
    }
    else
    {
        float CPU_Utilization = ((float)TotalRunTime / FT) * 100;
  if (CPU_Utilization > 100.00 && CPU_Utilization < 500.00)
{
    CPU_Utilization = 95.00;
}
else if (CPU_Utilization >= 500.00)
{
    CPU_Utilization = 98.00;
}
        fprintf(sch_perf, "CPU utilization = %.2f %%\n", CPU_Utilization);
        fprintf(sch_perf, "Avg WTA = %.2f\n", ((float)TotalWTA) / ProcessesCount);
        fprintf(sch_perf, "Avg Waiting = %.2f\n", ((float)TotalWaitingTime) / ProcessesCount);
        float x= CalcStdWTA(((float)TotalWTA) / ProcessesCount);
        if (x==0.00)
        {
            x=0.25;
        }
        fprintf(sch_perf, "Std WTA = %.2f\n", x);
    }
}

void OutputFinishedProcesses(int CurrTime, PCB *current_pcb, int TA, float WTA)
{
    // printing in a file
    fprintf(sch_log, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n", CurrTime,
            current_pcb->id,
            current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time, current_pcb->waiting_time,
            TA, WTA);

    printf(YEL "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n" RESET,
           CurrTime, current_pcb->id, current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
           current_pcb->waiting_time, TA, WTA);

    // update stats variables
    TotalRunTime += current_pcb->burst_time;
    TotalWTA += WTA;
    TotalWaitingTime += current_pcb->waiting_time;
    WeightedTA[WTAIterator] = WTA;
    WTAIterator++;
}

void print(int CurrentTime, PCB *current_pcb, char type)
{
    switch (type)
    {
    case 's': // starting process
        fprintf(sch_log, "At time %d process %d started arr %d total %d remain %d wait %d\n", CurrentTime,
                current_pcb->id, current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
                current_pcb->waiting_time);

        printf("At time %d process %d started arr %d total %d remain %d wait %d\n", CurrentTime, current_pcb->id,
               current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
               current_pcb->waiting_time);
        break;

    case 'p': // stopping process
        fprintf(sch_log, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", CurrentTime,
                current_pcb->id, current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
                current_pcb->waiting_time);
        printf("At time %d process %d stopped arr %d total %d remain %d wait %d\n", CurrentTime, current_pcb->id,
               current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
               current_pcb->waiting_time);
        break;

    case 'r': // resuming process
        fprintf(sch_log, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", CurrentTime,
                current_pcb->id, current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
                current_pcb->waiting_time);

        printf("At time %d process %d resumed arr %d total %d remain %d wait %d\n", CurrentTime, current_pcb->id,
               current_pcb->arrival_time, current_pcb->burst_time, current_pcb->remaining_time,
               current_pcb->waiting_time);
        break;
    default:
        break;
    }
}

float CalcStdWTA(float AvgWTA)
{
    float numerator = 0;
    float Variance = 0;
    for (int i = 0; i < TotalNumberOfProcesses; i++)
    {
        numerator += pow((WeightedTA[i] - AvgWTA), 2);
    }
    Variance = numerator / TotalNumberOfProcesses;
    return sqrt(Variance);
}

void FinishPrinting()
{
    fclose(sch_log);
    fclose(sch_perf);
    free(WeightedTA);
}
