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
    int cpu_burst_time;

    // 남은 cpu 시간, preemptive 용도
    int remaining_time;

    int priority;

    // 다수 I/O를 위한 필드
    int io_event_count;
    // 한 프로세스 I/O를 여러번 발생시키 위해 배열로
    int io_request_times[MAX_IO_EVENTS];
    int io_burst_times[MAX_IO_EVENTS];
    int current_io_index;
    int io_remaining;

    int start_time;
    int end_time;
    int waiting_time;
    int turnaround_time;

    // 완료 여부, 0이면 진행 중, 1이면 완료
    int is_completed;
    // 프로세스 상태, READY, RUNNING, WAITING, TERMINATED
    char state[16];

} Process;

// 전역 변수
Process plist[MAX_PROCESSES];
Process ready_queue[MAX_PROCESSES];
Process waiting_queue[MAX_PROCESSES];
int ready_count = 0, waiting_count = 0;
// waiting_count는 waiting queue에 있는 프로세스의 수

// 이후 코드들에서 반복문에서 활용하기 위함
int process_count = MAX_PROCESSES;

// Gantt 차트를 위한 타임라인
int gantt_chart[MAX_TIME];
int gantt_index = 0;

// 프로세스를 생성하고 랜덤한 값을 부여하는 함수
void Create_Process()
{
    for (int i = 0; i < process_count; i++)
    {
        Process *p = &plist[i];
        p->pid = i + 1;
        p->arrival_time = rand() % 10;
        p->cpu_burst_time = 10 + rand() % 11;
        p->remaining_time = p->cpu_burst_time;
        p->priority = rand() % 5;

        // I/O 요청 설정

        // io발생 횟수를 랜덤으로 정하기 위함
        p->io_event_count = 1 + rand() % MAX_IO_EVENTS;
        // 그 랜덤 횟수 동안, 언제 발생하는 지, 얼마나 발생하는 지를 랜덤으로 정해줌
        for (int j = 0; j < p->io_event_count; j++)
        {
            // 각각 1을 더해주는 건 0으로 곱하거나, 0으로 나누는 것을 피하면서 랜덤한 값을 구해주기 위함
            p->io_request_times[j] = (j + 1) * (p->cpu_burst_time / (p->io_event_count + 1));
            p->io_burst_times[j] = 1 + rand() % 5;
        }

        // 초기에는 일종의 더미 값들로 지정
        p->current_io_index = 0;
        p->io_remaining = 0;
        p->start_time = -1;
        p->end_time = -1;
        p->waiting_time = 0;
        p->turnaround_time = 0;
        p->is_completed = 0;
        snprintf(p->state, 16, "NEW");

        // 확인 출력
        printf("[P%d] AT:%d | CPU:%d | IO:", p->pid, p->arrival_time, p->cpu_burst_time);
        for (int j = 0; j < p->io_event_count; j++)
        {
            printf(" %d@%d", p->io_burst_times[j], p->io_request_times[j]);
        }
        printf(" | Priorty: %d", p->priority);
        printf("\n");
    }
}

// 도착 시간에 따라 프로세스를 정렬하는 함수
void Sort_By_Arrival(Process *plist, int n)
{

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {

            // pi가 pj보다 도착시간이 느리면 뒤로 보내준다.
            if (plist[i].arrival_time > plist[j].arrival_time)
            {

                Process temp = plist[i];
                plist[i] = plist[j];
                plist[j] = temp;
            }
        }
    }
}

// Ready Queue에 추가
void Add_To_Ready(Process p)
{
    // 항상 plist 기준으로 검사
    Process *orig = &plist[p.pid - 1];

    if (orig->remaining_time <= 0 || orig->is_completed)
        return;

    for (int i = 0; i < ready_count; i++)
    {
        if (ready_queue[i].pid == p.pid)
            return;
    }

    snprintf(orig->state, 16, "READY");
    ready_queue[ready_count++] = *orig;
}

// Ready queue에서 FCFS 방식으로 프로세스 하나 꺼내기
// Ready queue의 가장 앞의 프로세스의 pid를 return 하고, Ready queue는 한칸식 앞으로 당겨준다, 그리고 ready_count는 하나 감소시켜준다, 하나의 프로세스가 빠져나갔으므로
int Pop_Ready()
{
    if (ready_count == 0)
        return -1;
    int pid = ready_queue[0].pid;
    for (int i = 0; i < ready_count - 1; i++)
    {
        ready_queue[i] = ready_queue[i + 1];
    }
    ready_count--;
    return pid;
}

// Waiting queue 처리
// Waiting queue에 있는 프로세스들의 io 작업을 처리해준다.
void Process_Waiting_Queue()
{
    for (int i = 0; i < waiting_count; i++)
    {
        waiting_queue[i].io_remaining--;

        // io 작업이 끝나면-
        if (waiting_queue[i].io_remaining <= 0)
        {
            int pid = waiting_queue[i].pid;
            // 하나 빼주는 건 인덱스는 0부터 시작하므로
            if (plist[pid - 1].remaining_time > 0)
            {
                Add_To_Ready(plist[pid - 1]);
            }

            // 배열 한칸씩 앞으로 당기기
            for (int j = i; j < waiting_count - 1; j++)
            {
                waiting_queue[j] = waiting_queue[j + 1];
            }

            // 한 프로세스의 io 작업이 끝났으니, waiting_count를 하나 감소시키는 것
            waiting_count--;
            // 배열을 한 칸 땡겼으니 i를 감소시켜주는 것
            i--;
        }
    }
}

// FCFS 스케줄러 (다수 I/O 포함)
void FCFS_IO_With_MultipleIO()
{
    int current_time = 0;
    int completed = 0;
    int running_pid = -1;

    while (completed < process_count)
    {

        // 현재 실행 중인 프로세스 기록 후, ++ 시켜서 다음 시간단위로 넘어가는 것
        gantt_chart[gantt_index++] = running_pid;

        // 프로세스 도착 시점에 Ready Queue 추가
        for (int i = 0; i < process_count; i++)
        {
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(plist[i]);
            }
        }

        Process_Waiting_Queue();

        // 아무도 cpu를 쓰고 있지 않는 경우
        if (running_pid == -1 && ready_count > 0)
        {
            running_pid = Pop_Ready();
        }

        // cpu를 할당
        if (running_pid != -1)
        {
            Process *p = &plist[running_pid - 1];

            if (p->start_time == -1)
            {
                p->start_time = current_time;
            }

            // cpu를 쓰는 중인데 io 작업이 있는 경우
            // 현재 프로세스가 몇번째 io인지, 그게 그 프로세스의 전체 io 횟수보다 낮으면서, 해당 io 시간이 되었으면 io 처리 실행
            if (p->current_io_index < p->io_event_count &&
                p->remaining_time == p->cpu_burst_time - p->io_request_times[p->current_io_index])
            {
                snprintf(p->state, 16, "WAITING");
                p->io_remaining = p->io_burst_times[p->current_io_index];
                // io 처리를 했으니 한 개 증가
                p->current_io_index++;
                // waiting queue에 넣어서 io 처리를 하는 행위
                waiting_queue[waiting_count++] = *p;
                // cpu를 안쓰므로 -1 할당
                running_pid = -1;
            }

            // cpu를 쓰는 중인데 io 작업이 없는 경우
            else
            {
                p->remaining_time--;
                if (p->remaining_time == 0)
                {
                    p->end_time = current_time + 1;
                    p->turnaround_time = p->end_time - p->arrival_time;

                    // 이렇게도 계산할  수 있음, turnaround = 대기 + 실행, 대기 = turnaround - 실행
                    p->waiting_time = p->turnaround_time - p->cpu_burst_time;
                    snprintf(p->state, 16, "TERMINATED");
                    p->is_completed = 1;
                    running_pid = -1;
                    completed++;
                }
            }
        }

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

// Non-Preemptive SJF with Multiple I/O 스케줄러
void SJF_Nonpreemptive_IO()
{

    printf("SJF_Nonpreemptive_IO");
    int current_time = 0;
    int completed = 0;
    int running_pid = -1;

    // 스케줄러 종료 조건(FCFS도 이건 구조적으로 같음)
    while (completed < process_count)
    {

        gantt_chart[gantt_index++] = running_pid;

        // 프로세스 도착 -> Ready queue로
        for (int i = 0; i < process_count; i++)
        {

            // 프로세스가 도착했다는 건, arrival_time과 cpu의 현재 시점이 같다는 것
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(plist[i]);
            }
        }

        // Waiting queue 처리, i/o시간 감소 및 완료 시 Ready queue로 복귀, 아래에서 io발생한 프로세스가 위로 올라와서 처리되는 것
        Process_Waiting_Queue();

        // 현재 실행중인 프로세스가 없다면, cpu가 비었을 때
        if (running_pid == -1 && ready_count > 0)
        {

            int shortest_index = -1;
            int shortest_time = 1e9;

            // Ready queue에서, 남은 시간이 가장 짧은 프로세스를 고르는 작업
            for (int i = 0; i < ready_count; i++)
            {

                int pid = ready_queue[i].pid;
                if (plist[pid - 1].remaining_time < shortest_time)
                {

                    shortest_time = plist[pid - 1].remaining_time;
                    shortest_index = i;
                }
            }

            // 그렇게 선택한 프로세스를 실행준비(실행하는 건 아님)
            if (shortest_index != -1)
            {

                running_pid = ready_queue[shortest_index].pid;

                // 실행한 셈 치고, Ready_queue를 하나씩 앞으로 밀어주는 것
                for (int j = shortest_index; j < ready_count - 1; j++)
                {
                    ready_queue[j] = ready_queue[j + 1];
                }
                ready_count--;
            }
        }

        // 현재 실행 중인 프로세스가 있다면, 진짜 실행 진행
        if (running_pid != -1)
        {

            Process *p = &plist[running_pid - 1];

            if (p->start_time == -1)
                p->start_time = current_time;

            // 실행 중 io 요청 시점이면 Waiting queue로
            // 이건 fcfs랑 동일한데, 아직 io가 남았고, 현재 시점이 io요청 시점이면~ 이라는 뜻
            if (p->current_io_index < p->io_event_count && p->remaining_time == p->cpu_burst_time - p->io_request_times[p->current_io_index])
            {

                snprintf(p->state, 16, "WAITING");
                // cpu remaining이랑 헷갈리지x
                p->io_remaining = p->io_burst_times[p->current_io_index];
                p->current_io_index++;
                waiting_queue[waiting_count++] = *p;
                running_pid = -1;
            }
            // io요청 시점이 아니면, 계속 cpu 사용하기
            else
            {

                p->remaining_time--;

                if (p->remaining_time == 0)
                {

                    p->end_time = current_time + 1;
                    p->turnaround_time = p->end_time - p->arrival_time;
                    p->waiting_time = p->turnaround_time - p->cpu_burst_time;
                    snprintf(p->state, 16, "TERMINATED");
                    p->is_completed = 1;
                    running_pid = -1;
                    completed++;
                }
            }
        }
        current_time++;
    }
}

void SJF_Preemptive_IO()
{
    printf("\n[SJF_Preemptive_IO]\n");

    int current_time = 0;
    int completed = 0;
    int running_pid = -1;

    while (completed < process_count)
    {
        gantt_chart[gantt_index++] = running_pid;

        // 도착한 프로세스를 Ready Queue에 넣기
        for (int i = 0; i < process_count; i++)
        {
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(plist[i]);
            }
        }

        // I/O 대기 프로세스 체크
        Process_Waiting_Queue();

        // 가장 짧은 remaining_time을 가진 프로세스 선택
        int shortest_index = -1;
        int shortest_time = 1e9;

        for (int i = 0; i < ready_count; i++)
        {
            // ready queue에 있는 프로세스들 중에서, 가장 짧은 remaining_time을 가진 프로세스 선택
            int pid = ready_queue[i].pid;
            if (plist[pid - 1].remaining_time > 0 && plist[pid - 1].remaining_time < shortest_time)
            {
                shortest_time = plist[pid - 1].remaining_time;
                shortest_index = i;
            }
        }

        // 가장 짧은 프로세스가 정해졌다면-, 실행전 ready queue에서 제거
        if (shortest_index != -1)
        {
            int new_pid = ready_queue[shortest_index].pid;

            // Ready Queue에서 제거
            for (int j = shortest_index; j < ready_count - 1; j++)
            {
                ready_queue[j] = ready_queue[j + 1];
            }
            ready_count--;

            running_pid = new_pid;
        }
        else
        {
            running_pid = -1;
        }

        // 실행
        if (running_pid != -1)
        {
            Process *p = &plist[running_pid - 1];

            if (p->start_time == -1)
                p->start_time = current_time;

            // I/O 요청 시점 도달
            if (p->current_io_index < p->io_event_count &&
                p->remaining_time == p->cpu_burst_time - p->io_request_times[p->current_io_index])
            {
                snprintf(p->state, 16, "WAITING");
                p->io_remaining = p->io_burst_times[p->current_io_index];
                p->current_io_index++;
                waiting_queue[waiting_count++] = *p;
                running_pid = -1;
            }
            // io요청이 없다면-
            else
            {
                if (p->remaining_time == 1)
                {
                    p->remaining_time--;
                    p->end_time = current_time + 1;
                    p->turnaround_time = p->end_time - p->arrival_time;
                    p->waiting_time = p->turnaround_time - p->cpu_burst_time;
                    snprintf(p->state, 16, "TERMINATED");
                    p->is_completed = 1;
                    completed++;
                    running_pid = -1;
                }
                else
                {
                    p->remaining_time--;
                    Add_To_Ready(plist[p->pid - 1]);
                    running_pid = -1;
                }
            }
        }

        current_time++;
    }
}

void Priority_Nonpreemptive_IO()
{
    printf("\n[Priority_Nonpreemptive_IO]\n");

    int current_time = 0;
    int completed = 0;
    int running_pid = -1;

    while (completed < process_count)
    {

        // 도착한 프로세스를 Ready Queue에 넣기
        for (int i = 0; i < process_count; i++)
        {
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(plist[i]);
            }
        }

        // I/O 대기 프로세스 체크
        Process_Waiting_Queue();

        // CPU가 비었고 Ready Queue에 프로세스가 있으면 우선순위 높은 애 선택
        if (running_pid == -1 && ready_count > 0)
        {
            int highest_index = -1;
            int highest_priority = -1;

            for (int i = 0; i < ready_count; i++)
            {
                int pid = ready_queue[i].pid;
                // 우선순위 숫자가 클수록 높은 priority라고 가정
                // 여기서 > 이기 때문에( =>가 아니기 때문에) Priority가 같은 경우에도 FCFS로 작동
                if (plist[pid - 1].priority > highest_priority)
                {
                    highest_priority = plist[pid - 1].priority;
                    highest_index = i;
                }
            }
            if (highest_index != -1)
            {
                running_pid = ready_queue[highest_index].pid;

                // 선택된 프로세스 Ready Queue에서 제거
                for (int j = highest_index; j < ready_count - 1; j++)
                    ready_queue[j] = ready_queue[j + 1];
                ready_count--;
            }
        }

        // 실행
        if (running_pid != -1)
        {
            Process *p = &plist[running_pid - 1];

            if (p->start_time == -1)
                p->start_time = current_time;

            // I/O 요청 도달
            if (p->current_io_index < p->io_event_count &&
                p->remaining_time == p->cpu_burst_time - p->io_request_times[p->current_io_index])
            {
                snprintf(p->state, 16, "WAITING");
                p->io_remaining = p->io_burst_times[p->current_io_index];
                p->current_io_index++;
                waiting_queue[waiting_count++] = *p;
                running_pid = -1;
            }
            else
            {
                p->remaining_time--;

                if (p->remaining_time == 0)
                {
                    p->end_time = current_time + 1;
                    p->turnaround_time = p->end_time - p->arrival_time;
                    p->waiting_time = p->turnaround_time - p->cpu_burst_time;
                    snprintf(p->state, 16, "TERMINATED");
                    p->is_completed = 1;
                    running_pid = -1;
                    completed++;
                }
            }
        }
        gantt_chart[gantt_index++] = running_pid;
        current_time++;
    }
}
// Round Robin 전용 Ready Queue 추가 함수(Round Robin에서는 중복검사를 하면 안됨, ready queue에 여러개 같은 프로세스가 들어갈 수 있음)
void Add_To_Ready_RR(Process p)
{
    Process *orig = &plist[p.pid - 1];

    // 끝났으면 넣을 필요가X
    if (orig->remaining_time <= 0 || orig->is_completed)
        return;

    snprintf(orig->state, 16, "READY");
    ready_queue[ready_count++] = *orig;
}

void RoundRobin_IO()
{
    printf("\n[RoundRobin_IO]\n");
    int current_time = 0;
    int completed = 0;
    int running_pid = -1;
    int time_slice = 0;

    while (completed < process_count)
    {
        // printf("time: %d | running_pid: %d | time_slice: %d | ready_count: %d\n", current_time, running_pid, time_slice, ready_count);
        gantt_chart[gantt_index++] = running_pid;

        // 프로세스 도착 시점에 Ready Queue 추가
        for (int i = 0; i < process_count; i++)
        {
            if (plist[i].arrival_time == current_time)
            {
                Add_To_Ready(plist[i]);
            }
        }

        Process_Waiting_Queue();

        // 실행 중인 프로세스가 없으면 Ready Queue에서 꺼내기
        if (running_pid == -1 && ready_count > 0)
        {
            running_pid = Pop_Ready();
            time_slice = 0;
        }

        // 실행 중인 프로세스가 있을 때만 처리
        if (running_pid != -1)
        {
            Process *p = &plist[running_pid - 1];

            if (p->start_time == -1)
                p->start_time = current_time;

            // I/O 요청 도달
            if (p->current_io_index < p->io_event_count &&
                p->remaining_time == p->cpu_burst_time - p->io_request_times[p->current_io_index])
            {
                snprintf(p->state, 16, "WAITING");
                p->io_remaining = p->io_burst_times[p->current_io_index];
                p->current_io_index++;
                waiting_queue[waiting_count++] = *p;
                running_pid = -1;
                time_slice = 0;
            }
            else
            {
                p->remaining_time--;
                time_slice++;

                // 프로세스 종료
                if (p->remaining_time == 0)
                {
                    p->end_time = current_time + 1;
                    p->turnaround_time = p->end_time - p->arrival_time;
                    p->waiting_time = p->turnaround_time - p->cpu_burst_time;
                    snprintf(p->state, 16, "TERMINATED");
                    p->is_completed = 1;
                    running_pid = -1;
                    time_slice = 0;
                    completed++;
                }
                // time quantum 소진(선점)
                /*
                처음에, 조건문을 time_slice == TIME_QUANTUM로 설정했는데, time quantum보다 1개 적게 실행되었다.
                이유를 도통 모르겠어서 고민하다, + 1을 해주니 정상작동되었다.
                이게 논리상, 기존 코드는 1회 실행, 2회 실행(선점 발생, 2회 실행 마무리X) 이기 때문
                */
                else if (time_slice == TIME_QUANTUM + 1)
                {
                    Add_To_Ready_RR(*p);
                    running_pid = -1;
                    time_slice = 0;
                }
                // 아무 일도 없으면 running_pid 그대로 유지!
            }
        }

        current_time++;
    }
}

int main()
{
    srand(42);
    Create_Process();
    // FCFS_IO_With_MultipleIO();
    // SJF_Nonpreemptive_IO();
    // SJF_Preemptive_IO();
    Priority_Nonpreemptive_IO();
    // RoundRobin_IO();
    Print_Gantt_Chart();
    Print_Results();
    return 0;
}