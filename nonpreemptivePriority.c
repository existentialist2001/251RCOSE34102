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
        p->priority = 1 + rand() % 5;
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
        if (p->io_remaining <= 0)
        {
            p->is_waiting = 0;
            Add_To_Ready(p);
            for (int j = i; j < waiting_count - 1; j++)
                waiting_queue[j] = waiting_queue[j + 1];
            waiting_count--;
            i--;
        }
    }
}
/*
io요청 때문에 스스로 나가는 경우를 제외하고는, cpu가 선점되는 경우 없음
cpu burst time, io burst time 모두 일치하는 것 확인
다만,
차이점이 있음

이 알고리즘은 거의 마지막에 구현한거라, 기존 알고리즘들은 프로세스가 io요청을 받았을 때, ready_queue에서 실질적으로 프로세스를 빼지 않고,
그 프로세스의 변수를 수정하여 다음 ready_queue 선택에서 배제되도록 했음
하지만 이건 실제로 ready_queue에서 프로세스가 제거되도록 하여 우선순위가 같을 시, FCFS 방식으로 동작함
*/
void Priority_Nonpreemptive_IO()
{
    int current_time = 0, completed = 0;
    Process *running = NULL;

    while (completed < process_count)
    {
        // 도착 프로세스 ready로 이동
        for (int i = 0; i < process_count; i++)
            if (plist[i].arrival_time == current_time)
                Add_To_Ready(&plist[i]);

        // 실행 프로세스가 없을 때만 ready에서 하나 선택
        if (!running)
        {
            int max_priority = -1, sel_idx = -1;
            for (int i = 0; i < ready_count; i++)
            {
                if (!ready_queue[i]->is_completed && !ready_queue[i]->is_waiting && ready_queue[i]->remaining_time > 0)
                {

                    // 우선순위 숫자가 클수록 높은 priority라고 가정
                    // 여기서 > 이기 때문에( =>가 아니기 때문에) priority가 같은 경우에도 FCFS로 작동
                    if (ready_queue[i]->priority > max_priority)
                    {
                        max_priority = ready_queue[i]->priority;
                        sel_idx = i;
                    }
                }
            }
            // running에 할당 & ready_queue에서 삭제
            if (sel_idx != -1)
            {
                running = ready_queue[sel_idx];
                // ready_queue에서 삭제
                for (int j = sel_idx; j < ready_count - 1; j++)
                    ready_queue[j] = ready_queue[j + 1];
                ready_count--;
            }
        }

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
                running = NULL; // I/O 요청 시 바로 running 비움
                continue;
            }
            else
            {
                running->remaining_time--;

                if (running->remaining_time == 0)
                {
                    Process_Waiting_Queue();
                    running->end_time = current_time + 1;
                    running->turnaround_time = running->end_time - running->arrival_time;
                    running->waiting_time = running->turnaround_time - running->burst_time;
                    running->is_completed = 1;

                    gantt_chart[gantt_index++] = running->pid;

                    running = NULL; // 프로세스 종료 시 running 비움
                    completed++;
                    current_time++;
                    continue;
                }
            }
        }
        gantt_chart[gantt_index++] = (running ? running->pid : 0);
        current_time++;
        Process_Waiting_Queue();
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
    Priority_Nonpreemptive_IO();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}
