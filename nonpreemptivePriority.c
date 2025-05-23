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

// 레디큐에 넣기
void Add_To_Ready(Process *p)
{
    // 레디큐에 들어가면 안되는 조건
    if (p->remaining_time <= 0 || p->is_completed || p->is_waiting)
        return;

    // 중복검사, 중복이 있어도 들어가면 안됨
    for (int i = 0; i < ready_count; i++)
        if (ready_queue[i] == p)
            return;

    ready_queue[ready_count++] = p;
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
        // 도착 프로세스 ready queue로 이동
        for (int i = 0; i < process_count; i++)
            if (plist[i].arrival_time == current_time)
                Add_To_Ready(&plist[i]);

        // 실행 프로세스가 없을 때만 ready queue에서 하나 선택, 여기서 preemptive와의 차이점이 나타나게 됨
        if (!running)
        {

            // ready queue에서 가장 높은 priority를 선택
            int max_priority = -1, sel_idx = -1;
            for (int i = 0; i < ready_count; i++)
            {

                if (!ready_queue[i]->is_completed && !ready_queue[i]->is_waiting && ready_queue[i]->remaining_time > 0)
                {

                    // 숫자가 클수록 높은 priority라고 가정
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

            // io요청 도달
            if (running->current_io_index < running->io_event_count &&
                running->remaining_time == running->burst_time - running->io_request_times[running->current_io_index])
            {

                running->io_remaining = running->io_burst_times[running->current_io_index];
                running->is_waiting = 1;
                Add_To_Waiting(running);
                running->current_io_index++;

                running = NULL;
                // io요청이 발생하면, 그 시점은 간트차트에 반영하지 말고 다시 그 시점으로 돌아가야하니까, continue;
                continue;
            }

            // io요청이 없다면, 실행-

            else
            {
                running->remaining_time--;

                if (running->remaining_time == 0)
                {

                    Process_Waiting_Queue();

                    // 5의 시점에서, remaining_time--해서 한 시점 소비했으니, 끝나는 시점은 +1한 시점이어야함
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

        // 실행 중 io도 없었고, 그렇다고 프로세스가 완전히 끝나지도 않았을 때
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
    // pid가 담김
    int last = gantt_chart[0];
    printf("[0~P%d ", last);

    for (int i = 1; i < gantt_index; i++)
    {
        // 프로세스가 전환된다면
        if (gantt_chart[i] != last)
        {
            // 이전꺼는 닫고,
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
