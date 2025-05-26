#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 5
#define MAX_IO_EVENTS 3
#define MAX_TIME 200
#define TIME_QUANTUM 2

// 프로세스를 의미하는 구조체
typedef struct
{

    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int is_completed;
    int is_waiting;

    // 다수 I/O를 위한 필드
    int io_event_count;
    int io_request_times[MAX_IO_EVENTS];
    int io_burst_times[MAX_IO_EVENTS];
    int current_io_index;
    int io_remaining;

    int start_time;
    int end_time;
    int waiting_time;
    int turnaround_time;
} Process;

// 전역 변수
Process plist[MAX_PROCESSES];
Process *ready_queue[MAX_PROCESSES * 2];
Process *waiting_queue[MAX_PROCESSES * 2];
int ready_count = 0, waiting_count = 0;

// 이후 코드들에서 반복문에서 활용하기 위함
int process_count = MAX_PROCESSES;

// Gantt 차트를 위한 타임라인
int gantt_chart[MAX_TIME];
int gantt_index = 0;

void Create_Process()
{
    srand(42);
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

// Ready queue에서 FCFS 방식으로 프로세스 하나 꺼내기
// Ready queue의 가장 앞의 프로세스의 pid를 return 하고, Ready queue는 한칸식 앞으로 당겨준다, 그리고 ready_count는 하나 감소시켜준다, 하나의 프로세스가 빠져나갔으므로
int Pop_Ready()
{
    if (ready_count == 0)
        return -1;
    int pid = ready_queue[0]->pid;
    for (int i = 0; i < ready_count - 1; i++)
    {
        ready_queue[i] = ready_queue[i + 1];
    }
    ready_count--;
    return pid;
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

// 웨이팅큐에 넣기
void Add_To_Waiting(Process *p)
{
    for (int i = 0; i < waiting_count; i++)
        if (waiting_queue[i] == p)
            return;

    waiting_queue[waiting_count++] = p;
}

// Nonpreemptive SJF
/*
cpu burst time이 짧아도, 도착 시간 & nonpreemptive이므로 먼저 도착한 프로세스가 실행되고, 중간에 cpu를 뺏기지 않음
cpu를 뺏기는 경우는, io 요청에 의해 스스로 나가는 경우 뿐

Premmptive SJF와의 차이점은, 레디큐에서 가장 짧은 프로세스를 찾는 작업을,
실행 중인 프로세스가 없을 때에만 하느냐 vs 매 시점 하느냐
*/
void Nonpreemptive_SJF()
{

    printf("\nNonpreemptive_SJF\n");
    int current_time = 0;
    int completed = 0;
    int running_pid = -1;
    Process *running = NULL;

    while (completed < process_count)
    {

        // 프로세스 도착 -> Ready queue로
        for (int i = 0; i < process_count; i++)
        {

            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(&plist[i]);
            }
        }

        // 현재 실행중인 프로세스가 없다면, cpu가 비었을 때
        if (!running)
        {

            int shortest_index = -1;
            int shortest_time = 1e9;

            // Ready queue에서, 남은 시간이 가장 짧은 프로세스를 고르는 작업
            for (int i = 0; i < ready_count; i++)
            {
                int pid = ready_queue[i]->pid;
                if (plist[pid - 1].remaining_time < shortest_time)
                {

                    shortest_time = plist[pid - 1].remaining_time;
                    shortest_index = i;
                }
            }

            // 그렇게 선택한 프로세스를 실행준비(실행하는 건 아님)
            if (shortest_index != -1)
            {

                running = ready_queue[shortest_index];

                // Ready_queue를 하나씩 앞으로 밀어주는 것
                for (int j = shortest_index; j < ready_count - 1; j++)
                {
                    ready_queue[j] = ready_queue[j + 1];
                }
                ready_count--;
            }
        }

        // 현재 실행 중인 프로세스가 있다면, 진짜 실행 진행
        if (running)
        {

            if (running->start_time == -1)
                running->start_time = current_time;

            // 실행 중 io 요청 시점이면 Waiting queue로
            if (running->current_io_index < running->io_event_count && running->remaining_time == running->burst_time - running->io_request_times[running->current_io_index])
            {

                // cpu remaining이랑 헷갈리지x
                running->io_remaining = running->io_burst_times[running->current_io_index];
                running->is_waiting = 1;
                Add_To_Waiting(running);
                running->current_io_index++;

                // 추가
                /* 그래서 다른 알고리즘도 그렇지만, io 요청이 와서 그걸 처리하러 가는 상황에서는
                간트차트 넣어주면 안된다. 시점에 반영해주면 안된다.
                */
                running = NULL;
                continue;
            }
            // io요청 시점이 아니면, 계속 cpu 사용하기
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
                    running = NULL;
                    completed++;
                    current_time++;
                    continue;
                }
            }
        }
        gantt_chart[gantt_index++] = (running ? running->pid : 0);
        Process_Waiting_Queue();
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
    Nonpreemptive_SJF();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}