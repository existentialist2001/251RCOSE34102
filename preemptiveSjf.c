#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 5
#define MAX_IO_EVENTS 3
#define MAX_TIME 200

/*
이 알고리즘의 경우, 간트차트 상에서 cpu burst time이 1씩 더 많이 출력됨,
디버깅을 계속 해봤지만 결국 실패함
원리는 잘 작동하는 걸 확인, 매 시점 ready queue에서 짧은 cpu burst time을 선택해서 선점하는 것,
짧아도 끝까지 실행되지 않고 io요청 때문에 중간에 나가는 것과 다시 돌아오는 것까지 확인
다만 간트차트 출력 +1 되는 것을 해결하지 못하겠음

다시 하니까(?) 간트차트 cpu burst time 맞음, 다만 시점이 하나씩 밀림(아마 배열 + 후처리 때문인 듯)
논리적으로, 그 시점의 모든 것이 결정된 후 간트차트에 반영해주는 게 맞아보임
*/

typedef struct
{
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int start_time, end_time;
    int waiting_time, turnaround_time;
    int is_completed;

    // I/O 관련
    int io_event_count;
    int io_request_times[MAX_IO_EVENTS];
    int io_burst_times[MAX_IO_EVENTS];
    int current_io_index; // 몇번째 io 이벤트 대기중인지
    int io_remaining;     // I/O 남은 시간

    int is_waiting; // 1: I/O 대기중, 0: CPU ready/실행
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
        p->remaining_time = p->burst_time;
        p->start_time = -1;
        p->end_time = -1;
        p->waiting_time = 0;
        p->turnaround_time = 0;
        p->is_completed = 0;

        p->current_io_index = 0;
        p->io_remaining = 0;
        p->is_waiting = 0;

        // 랜덤 I/O
        p->io_event_count = 1 + rand() % MAX_IO_EVENTS;

        for (int j = 0; j < p->io_event_count; j++)
        {
            p->io_request_times[j] = (j + 1) * (p->burst_time / (p->io_event_count + 1));
            p->io_burst_times[j] = 1 + rand() % 4;
        }

        // 디버깅 출력
        printf("P%d | AT:%d | CPU:%d | IO:", p->pid, p->arrival_time, p->burst_time);
        for (int j = 0; j < p->io_event_count; j++)
            printf(" %d@%d", p->io_burst_times[j], p->io_request_times[j]);
        printf("\n");
    }
}

// ready queue에 중복 없이 삽입
void Add_To_Ready(Process *p)
{
    // p -> is_waiting이 1이라는 것은, io 대기중이므로 ready queue에 들어가면X
    if (p->remaining_time <= 0 || p->is_completed || p->is_waiting)
        return;

    // 중복검사, sjf에서는 ready queue에 똑같은 프로세스가 여러개 있으면X
    for (int i = 0; i < ready_count; i++)
        if (ready_queue[i] == p)
            return;
    ready_queue[ready_count++] = p;
}

// waiting queue에 삽입 (중복X)
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

            // IO끝나고 ready queue에 넣었으니 waiting queue 한칸씩 앞으로 당기기
            for (int j = i; j < waiting_count - 1; j++)

                waiting_queue[j] = waiting_queue[j + 1];
            waiting_count--;
            i--;
        }
    }
}

void SRTF_IO_Ptr()
{
    int current_time = 0, completed = 0;
    Process *running = NULL;

    while (completed < process_count)
    {

        // 도착 프로세스 ready로 이동
        for (int i = 0; i < process_count; i++)

            if (plist[i].arrival_time == current_time)
                Add_To_Ready(&plist[i]);

        // 매 시점마다 waiting queue 처리
        Process_Waiting_Queue();

        // ready queue에서 SRTF 찾기
        int min_time = 1e9, min_idx = -1;

        for (int i = 0; i < ready_count; i++)
        {
            // ready queue의 프로세스가 완료되지 않았고, io를 기다리고 있지도 않으면- 후보로서 srtf를 찾기 위한 검사를 해줌
            if (!ready_queue[i]->is_completed && !ready_queue[i]->is_waiting && ready_queue[i]->remaining_time > 0)
            {
                if (ready_queue[i]->remaining_time < min_time)
                {
                    min_time = ready_queue[i]->remaining_time;
                    min_idx = i;
                }
            }
        }

        // 그렇게 선택된 프로세스를 넣어줌(가장 짧은 프로세스, 실행하기로 결정된 프로세스!)
        running = (min_idx != -1) ? ready_queue[min_idx] : NULL;

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
                /*여기서, ready queue에서 제거하지 않아도 됨, 왜냐하면 is_waiting = 1이기 때문에, ready_queue에 남아있더라도 가장 짧은 프로세스를 찾을 때
                고려안됨*/
                running->current_io_index++;
            }
            // io 요청이 없으면 곧장 실행
            else
            {
                running->remaining_time--;

                if (running && running->remaining_time == 0)
                {
                    running->end_time = current_time + 1;
                    running->turnaround_time = running->end_time - running->arrival_time;
                    running->waiting_time = running->turnaround_time - running->burst_time;
                    running->is_completed = 1;
                    completed++;

                    running = NULL;
                }
            }
        }

        // 간트 차트에 넣기
        if (running)
        {
            gantt_chart[gantt_index++] = running->pid;
        }
        else
        {
            gantt_chart[gantt_index++] = 0; // idle
        }

        current_time++;

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
        // 다른 거면, 다르게 출력
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
    SRTF_IO_Ptr();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}