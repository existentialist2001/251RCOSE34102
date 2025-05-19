#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 5
#define MAX_IO_EVENTS 3
#define MAX_TIME 200

typedef struct
{
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int start_time, end_time;
    int waiting_time, turnaround_time;
    int is_completed;

    // I/O 관련
    int io_event_count;
    int io_request_times[MAX_IO_EVENTS];
    int io_burst_times[MAX_IO_EVENTS];
    int current_io_index;
    int io_remaining;
    int is_waiting;
} Process;

Process plist[MAX_PROCESSES];
Process *ready_queue[MAX_PROCESSES * 2];
int ready_count = 0;
Process *waiting_queue[MAX_PROCESSES * 2];
int waiting_count = 0;
int process_count = MAX_PROCESSES;
int gantt_chart[MAX_TIME], gantt_index = 0;

void Create_Process()
{
    srand(42); // 랜덤 고정
    for (int i = 0; i < process_count; i++)
    {
        Process *p = &plist[i];
        p->pid = i + 1;
        p->arrival_time = rand() % 5;
        p->burst_time = 8 + rand() % 8;
        p->priority = 1 + rand() % 5; // 1~5 우선순위(클수록 높음)
        p->remaining_time = p->burst_time;
        p->start_time = -1;
        p->end_time = -1;
        p->waiting_time = 0;
        p->turnaround_time = 0;
        p->is_completed = 0;
        p->current_io_index = 0;
        p->io_remaining = 0;
        p->is_waiting = 0;

        p->io_event_count = 1 + rand() % MAX_IO_EVENTS;
        for (int j = 0; j < p->io_event_count; j++)
        {
            p->io_request_times[j] = (j + 1) * (p->burst_time / (p->io_event_count + 1));
            p->io_burst_times[j] = 1 + rand() % 4;
        }

        printf("P%d | AT:%d | CPU:%d | PRI:%d | IO:", p->pid, p->arrival_time, p->burst_time, p->priority);
        for (int j = 0; j < p->io_event_count; j++)
            printf(" %d@%d", p->io_burst_times[j], p->io_request_times[j]);
        printf("\n");
    }
}

void Add_To_Ready(Process *p)
{

    if (p->remaining_time <= 0 || p->is_completed || p->is_waiting)
        return;

    for (int i = 0; i < ready_count; i++)
        if (ready_queue[i] == p)
            return;

    ready_queue[ready_count++] = p;
}

void Add_To_Waiting(Process *p)
{
    for (int i = 0; i < waiting_count; i++)
        if (waiting_queue[i] == p)
            return;
    waiting_queue[waiting_count++] = p;
}

void Process_Waiting_Queue()
{
    for (int i = 0; i < waiting_count; i++)
    {
        Process *p = waiting_queue[i];
        p->io_remaining--;

        /*
        여기서 문제가, 다음 시점에 넣어줘야 하는데(한 시점 썼으니까) 바로 ready_queue에 넣어주고, 바로 검사하게 되어서
        시점의 왜곡이 발생하는 것
        */
        if (p->io_remaining <= 0)
        {
            // 사실 무의미한게, 이미 있기 때문에 중복 검사에서 걸려서 어짜피 안넣어짐, p->is_waiting = 0이 실질적으로 효과가 있는 거임
            p->is_waiting = 0;

            // 무의미하니까 주석처리(일단 놔둠)
            Add_To_Ready(p);

            for (int j = i; j < waiting_count - 1; j++)
                waiting_queue[j] = waiting_queue[j + 1];
            waiting_count--;
            i--;
        }
    }
}
/*
ready queue의 작동방식
여기서 ready queue는 실제로 어떤 데이터를 삭제하지 않는다.
그래서, p1이 p2와 priority가 같은데, p1이 먼저 ready_queue에 추가되었으면, 계속해서 p1이 선택되도록 구현되어있음
사실 균등하게 배분하고 싶은데, 너무 까다로워서 거기까진 손을 못대겠다.

그리고 cpu burst time, 순서, 선점 다 이루어지는데
io_burst_time이 엄밀하게 지켜지지 않고 있음
이거 계속 디버깅해보는데 못찾는 중..
-> '시점이 반영될 때에만' 동시에 io처리를 하도록 코드를 구현하면, io_burst_time이 엄밀하게 지켜짐
*/
void Preemptive_Priority_IO()
{
    printf("\nPreemptive_Priority_IO\n");
    int current_time = 0, completed = 0;
    Process *running = NULL;

    while (completed < process_count)
    {
        // 도착 프로세스 ready로 이동
        for (int i = 0; i < process_count; i++)
            if (plist[i].arrival_time == current_time)
                Add_To_Ready(&plist[i]);

        // ready 중에서 우선순위(숫자 클수록 높음) 가장 높은 것 고르기
        int max_priority = -1, sel_idx = -1;
        for (int i = 0; i < ready_count; i++)
        {

            if (!ready_queue[i]->is_completed && !ready_queue[i]->is_waiting && ready_queue[i]->remaining_time > 0)
            {
                if (ready_queue[i]->priority > max_priority)
                {
                    max_priority = ready_queue[i]->priority;
                    sel_idx = i;
                }
            }
        }
        running = (sel_idx != -1) ? ready_queue[sel_idx] : NULL;

        // reday_queue에서 해당 프로세스를 삭제해주는 작업(이거하면 큰일남)
        //  if (sel_idx != -1)
        //  {
        //      for (int j = sel_idx; j < ready_count - 1; j++)
        //          ready_queue[j] = ready_queue[j + 1];
        //      ready_count--;
        //  }

        // waiting queue 처리
        // io를 병렬적으로 한시점씩 싹 처리
        /*
        프로세스 실행 중 io가 발생하면, 바로 continue를 해서 current_time++을 하지 않는다.
        즉 '그 시점'은 반영하지 않는다. 그렇기 때문에 다음 루프에서 io처리를 할 때는, ready_queue 판단이 끝난 후 해야한다.
        */

        // 실행
        if (running)
        {
            if (running->start_time == -1)
                running->start_time = current_time;

            // I/O 요청 도달
            if (running->current_io_index < running->io_event_count &&
                running->remaining_time == running->burst_time - running->io_request_times[running->current_io_index])
            {
                running->io_remaining = running->io_burst_times[running->current_io_index];
                running->is_waiting = 1;
                Add_To_Waiting(running);
                running->current_io_index++;

                // 시점 반영안해버림
                continue;
            }
            // io요청이 없으면(시점반영되는 경우)
            else
            {
                running->remaining_time--;

                // 프로세스가 완전히 끝나면-
                if (running->remaining_time == 0)
                {
                    Process_Waiting_Queue();
                    running->end_time = current_time + 1;
                    running->turnaround_time = running->end_time - running->arrival_time;
                    running->waiting_time = running->turnaround_time - running->burst_time;
                    running->is_completed = 1;

                    gantt_chart[gantt_index++] = running->pid;

                    running = NULL;
                    completed++;
                    current_time++;
                    continue;
                }
            }
        }

        // 간트차트 기록, 이 경우는 io요청도 없었고, 실행했지만 프로세스가 완전히 끝나지 않은 경우(시점 반영 되는 경우)
        Process_Waiting_Queue();
        current_time++;
        gantt_chart[gantt_index++] = (running ? running->pid : 0);

        if (current_time > MAX_TIME - 2)
            break;
    }
}

void Print_Gantt_Chart()
{
    printf("\n[Gantt Chart]\n");
    int last = gantt_chart[0];
    printf("[0~P%d ", last);
    for (int i = 1; i < gantt_index; i++)
    {
        if (gantt_chart[i] != last)
        {
            printf("] [%d~P%d ", i, gantt_chart[i]);
            last = gantt_chart[i];
        }
    }
    printf("]\n");
}

void Print_Results()
{
    float total_wt = 0, total_tt = 0;
    printf("\n[Process Summary]\n");
    for (int i = 0; i < process_count; i++)
    {
        printf("P%d | WT: %d | TAT: %d\n", plist[i].pid, plist[i].waiting_time, plist[i].turnaround_time);
        total_wt += plist[i].waiting_time;
        total_tt += plist[i].turnaround_time;
    }
    printf("Average WT = %.2f\n", total_wt / process_count);
    printf("Average TAT = %.2f\n", total_tt / process_count);
}

int main()
{
    Create_Process();
    Preemptive_Priority_IO();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}
