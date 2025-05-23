#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 5
#define MAX_IO_EVENTS 3
#define MAX_TIME 200
#define TIME_QUANTUM 2

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

    // cpu remaining time이 얼마 남은 시점에, io가 발생할 것인가-
    int io_request_times[MAX_IO_EVENTS];
    int io_burst_times[MAX_IO_EVENTS];
    int current_io_index;
    int io_remaining;
    int is_waiting;
} Process;

// 구조체 배열을 만들고, 매핑시켜주기
Process plist[MAX_PROCESSES];
Process *ready_queue[MAX_PROCESSES * 2];
int ready_count = 0;
Process *waiting_queue[MAX_PROCESSES * 2];
int waiting_count = 0;
int process_count = MAX_PROCESSES;
int gantt_chart[MAX_TIME], gantt_index = 0;

void Create_Process()
{
    printf("\n[Processes Info]\n");
    srand(42);
    // 개발 다하고 srand(time(NULL)) 넣어서 더 완벽히 랜덤으로 구현

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

        // 프로세스 별로 io 횟수는 1번 ~ 3번 사이(무조건 발생한다는 의미)
        p->io_event_count = 1 + rand() % MAX_IO_EVENTS;
        for (int j = 0; j < p->io_event_count; j++)
        {
            /*
            burst time 12, io event count 2인 경우를 생각해보면 됨
            j + 1은 burst time에 걸쳐서 고르게 io를 발생시키기 위한 장치, *1, *2가 됨으로써-, io request time이 4가되고 8이된다.
            p->io_event_count + 1는 총 몇개의 구간에 걸쳐서 burst time이 실행되는지를 의미
            위 예시에서는, 3개의 구간에 걸쳐서 실행되므로, burst time은 3개의 구간으로 균등하게 나누면 4씩이다.
            */
            p->io_request_times[j] = (j + 1) * (p->burst_time / (p->io_event_count + 1));
            p->io_burst_times[j] = 1 + rand() % 4;
        }

        printf("P%d | AT:%d | CPU:%d | PRI:%d | IO:", p->pid, p->arrival_time, p->burst_time, p->priority);
        for (int j = 0; j < p->io_event_count; j++)
            // 얼만큼 실행되고, 언제 실행되는지
            printf(" %d@%d", p->io_burst_times[j], p->io_request_times[j]);
        printf("\n");
    }
}

// 웨이팅큐에 넣기
void Add_To_Waiting(Process *p)
{
    for (int i = 0; i < waiting_count; i++)
        if (waiting_queue[i] == p)
            return;

    waiting_queue[waiting_count++] = p;
}

// 웨이팅큐 처리, 웨이팅 큐에 있는 프로세스들 다 한시점씩 io처리해줌
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

            // 웨이팅큐 한칸씩 앞으로 당겨주기
            for (int j = i; j < waiting_count - 1; j++)
                waiting_queue[j] = waiting_queue[j + 1];
            waiting_count--;
            i--;
        }
    }
}

// Ready Queue에 추가
void Add_To_Ready(Process *p)
{
    if (p->remaining_time <= 0 || p->is_completed || p->is_waiting)
        return;

    for (int i = 0; i < ready_count; i++)
        if (ready_queue[i] == p)
            return;

    ready_queue[ready_count++] = p;
}

/*
도착 순서대로 time quantum만큼 실행되는 것 확인
time quantum도 다썼는데, 그 시점에 io도 발생하는 경우)
사실 자원을 반납하고, io 처리도 해주면 됨, 그래서 io 조건에 먼저 걸리게 해주었음

time quantum을 다써서 ready queue로 돌아가려는데, 그 시점에 새 프로세스가 진입하는 경우)
골고루 실행될 수 있도록 하기위해, 새 프로세스가 ready queue에 먼저 들어가도록 해주었음

preemptive 가 붙은 알고리즘들과 다르게, 실제로 ready queue에서 추가하고 삭제해주는 방식

1만큼 실행(time quantum만큼 다 실행x) 했을 때 io가 발생하는 경우)
time slice 초기화되도록 함

time qauntum을 다쓰고, 프로세스도 종료되는 경우)
서로 구분해야함, 안그러면 둘다 반영되어 io 두번 처리되는 등의 불상사
*/
void RoundRobin_IO()
{
    printf("\n[RoundRobin]\n");
    int current_time = 0;
    int completed = 0;
    int time_slice = 0;

    Process *running = NULL;

    while (completed < process_count)
    {

        // 프로세스 도착 시점에 Ready Queue 추가
        for (int i = 0; i < process_count; i++)
        {
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(&plist[i]);
            }
        }

        // 실행 중인 프로세스가 없으면 Ready Queue에서 꺼내기
        if (!running && ready_count > 0)
        {
            int pid = ready_queue[0]->pid;
            for (int i = 0; i < ready_count - 1; i++)
            {
                ready_queue[i] = ready_queue[i + 1];
            }
            ready_count--;
            running = &plist[pid - 1];
            time_slice = 0;
        }

        // 실행 중인 프로세스가 있으면
        if (running)
        {
            if (running->start_time == -1)
                running->start_time = current_time;

            // I/O 요청 도달
            if (running->current_io_index < running->io_event_count &&
                running->remaining_time == running->burst_time - running->io_request_times[running->current_io_index])
            {
                running->io_remaining = running->io_burst_times[running->current_io_index];
                running->current_io_index++;
                waiting_queue[waiting_count++] = running;

                running = NULL;
                time_slice = 0;

                continue;
            }

            if (time_slice == TIME_QUANTUM)
            {
                Add_To_Ready(running);
                running = NULL;
                time_slice = 0;
                continue;
            }

            // io요청이 없으면 계속 실행, 계속 실행하는데-
            else
            {
                running->remaining_time--;
                time_slice++;

                // 프로세스가 완전히 끝났다면
                if (running->remaining_time == 0)
                {
                    Process_Waiting_Queue();
                    running->end_time = current_time + 1;
                    running->turnaround_time = running->end_time - running->arrival_time;
                    running->waiting_time = running->turnaround_time - running->burst_time;
                    running->is_completed = 1;

                    /*이거, 프로세스가 완전히 끝났든, 타임 퀀텀이 됐든 '진행'이 되었기 때문에,  간트차트에 반영해주고 current_time++해야함 */
                    //
                    gantt_chart[gantt_index++] = running->pid;

                    running = NULL;
                    time_slice = 0;

                    completed++;
                    current_time++;
                    continue;
                }
            }
        }

        // 결국 여기는 io도 없고, 프로세스가 완전히 끝나지도 않고, timequantum도 남았을 때 오는 부분
        Process_Waiting_Queue();
        gantt_chart[gantt_index++] = (running ? running->pid : 0);
        current_time++;
    }
}

// Gantt 차트 출력
void Print_Gantt_Chart()
{
    printf("\n[Gantt Chart]\n");
    for (int i = 0; i < gantt_index; i++)
    {
        // 간트 차트 시작이거나, 프로세스가 바뀌었을 때만 출력(프로세스가 바뀌지 않았다면 연속적이니)
        if (i == 0 || gantt_chart[i] != gantt_chart[i - 1])
        {
            // 새 구간을 열기 전에 이전 구간을 닫는 용도
            if (i > 0)
                printf("] ");
            if (gantt_chart[i] == -1)
                printf("[%d~ IDLE ", i);
            // 실제로 cpu 사용중인 프로세스 출력
            else
                printf("[%d~P%d ", i, gantt_chart[i]);
        }
    }
    printf("]\n");
}

// 결과 출력, 프로세스별 wt, tt를 출력하고, 평균도 구해서 바로 출력
void Print_Results()
{
    int total_wt = 0, total_tat = 0;
    printf("\n[Process Summary]\n");
    for (int i = 0; i < process_count; i++)
    {
        Process p = plist[i];
        printf("P%d | WT: %2d | TAT: %2d\n", p.pid, p.waiting_time, p.turnaround_time);
        total_wt += p.waiting_time;
        total_tat += p.turnaround_time;
    }
    printf("Average WT = %.2f\n", (float)total_wt / process_count);
    printf("Average TAT = %.2f\n", (float)total_tat / process_count);
}

int main()
{
    srand(42);
    Create_Process();
    RoundRobin_IO();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}