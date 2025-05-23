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

// FCFS 스케줄러 (다수 I/O 포함)
/*
선입선출대로 실행됨(ready queue 순서에 따라),
간트 차트 1씩 밀리는 문제 해결 완료, burst time도 기존에 부여된 값과 동일
기본적으로 선입선출대로 실행되지만, io요청에 따라 선점되서 나가는 것까지 확인
*/
void FCFS()
{
    printf("\nFCFS\n");

    int current_time = 0;
    int completed = 0;
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

        // 아무도 cpu를 쓰고 있지 않는 경우, cpu할당을 위한 준비
        /*
        처음에 idle이라면 어쩔건데? -1을 리턴하고, 그럼 null을 참조하게 됨
        */
        if (!running)
        {
            int pid = Pop_Ready();
            if (pid > 0)
            {
                running = &plist[pid - 1];
            }
        }

        // cpu를 할당
        if (running)
        {

            if (running->start_time == -1)
            {
                running->start_time = current_time;
            }

            // cpu를 쓰는 중인데 io 작업이 있는 경우
            // 현재 프로세스가 몇번째 io인지, 그게 그 프로세스의 전체 io 횟수보다 낮으면서, 해당 io 시간이 되었으면 io 처리 실행
            if (running->current_io_index < running->io_event_count &&
                running->remaining_time == running->burst_time - running->io_request_times[running->current_io_index])
            {
                running->io_remaining = running->io_burst_times[running->current_io_index];
                // waiting queue에 집어 넣을꺼니까 한개 증가
                running->current_io_index++;
                // waiting queue에 넣어서 io 처리를 하는 행위
                Add_To_Waiting(running);
                // cpu를 안쓰므로 null
                running = NULL;
                continue;
            }

            // cpu를 쓰는 중인데 io 작업이 없는 경우
            else
            {
                running->remaining_time--;
                if (running->remaining_time == 0)
                {
                    Process_Waiting_Queue();
                    running->end_time = current_time + 1;
                    running->turnaround_time = running->end_time - running->arrival_time;

                    // 이렇게도 계산할  수 있음, turnaround = 대기 + 실행, 대기 = turnaround - 실행
                    running->waiting_time = running->turnaround_time - running->burst_time;

                    running->is_completed = 1;
                    gantt_chart[gantt_index++] = running->pid;

                    running = NULL;
                    completed++;
                    current_time++;
                    continue;

                    /*
                    프로세스가 끝난 경우, 그 시점까지는 running_pid가 프로세스 id로 간트차트에 기록되어야 함
                    그런데 기존 코드는 프로세스가 끝나면 running_pid를 -1로 바꾸어 버리고, 그 상태에서 간트차트에 반영해버리기 때문에
                    1개씩 시점이 앞으로 밀리고, cpu burst time도 한개씩 부족했던 것
                    */
                }
            }
        }

        // 현재 실행 중인 프로세스 기록 후, ++ 시켜서 다음 시점으로
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
    FCFS();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}