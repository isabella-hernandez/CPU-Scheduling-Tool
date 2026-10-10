#include "scheduler.h"

#include <stdio.h>
/*
 * This is the source file you must complete.
 *
 * You are responsible for implementing the simulated operating-system
 * behaviour, including:
 *   - PCBs and process states;
 *   - the ready queue and waiting processes;
 *   - CPU and I/O burst accounting;
 *   - FCFS, Round Robin, and preemptive SRTF;
 *   - dispatch/context-switch overhead; and
 *   - calls to the supplied metrics and CSV logging functions.
 *
 * You may replace this file completely and add any .c/.h files you need.
 * The types and helper functions below are only a suggested decomposition.
 * You may rename, replace, combine, or remove them.
 */

/*
 * Suggested PCB starting point. Add the fields required by the handout, such
 * as remaining CPU time, completed bursts, I/O completion time, Ready-entry
 * order, and RR quantum usage.
 */
 // -------------------changed-----------------
typedef struct {
    ProcessSpec spec;
    ProcessState state;
    int remaining_cpu_time; // ticks down while RUNNING
    int bursts_completed; // increases each time a burst finishes
    int io_timetofinish; // set once a process enters WAITING
    int rr_used; // reset to 0 on dispatch
} PCB;

typedef struct {
    int pcb_index[MAX_PROCESSES];
    int front; // could be set to 0
    int count; // could be set to 0
} ReadyQueue;

typedef struct {
    TimelineActivity activity;
    int current_running_pcb;
    int time_remaining;
    int next_pcb_index;
} CpuState;
//-----------------changed ^^^^ -------------------

/* You may choose to maintain a structure for your queues.
 * Do so in whichever way you see fit.
 */
/* Keeping whole-simulation state together makes helper signatures smaller. 
 * Store whatever you feel is necessary for your implementation.
 */
typedef struct {
    size_t process_count;
} Simulation;
/*
 * Supplied output API examples
 * ----------------------------
 * These functions are declared in scheduler.h and implemented in io.c. They
 * format the CSV rows for you and calculate the metrics. You must use them
 * to ensure your output can be correctly processed by the test harness.
 *
 * State transition:
 *
 *   log_transition(output, time, pid,
 *                  STATE_NEW, STATE_READY, REASON_ARRIVAL);
 *
 * One timeline interval [time, time + 1):
 *
 *  If the CPU is running a process:
 *      record_timeline_tick(output, time, ACTIVITY_CPU, running_pid);
 *
 *  If the CPU is performing a context switch:
 *      record_timeline_tick(output, time, ACTIVITY_CONTEXT_SWITCH, 0);
 *
 *  If the CPU is idle:
 *      record_timeline_tick(output, time, ACTIVITY_IDLE, 0);
 *
 * Final metrics (one call after every process terminates):
 *
 *   if (!write_metrics(output, specs, count)) {
 *       return false;
 *   }
 *
 */ 
 // =================== helper functions ================
 const char *state_to_string(ProcessState state) {
    //using swtuch case for shortesr lines of code
    switch (state) {
        case STATE_NEW:         return "NEW";
        case STATE_READY:       return "READY";
        case STATE_RUNNING:     return "RUNNING";
        case STATE_WAITING:     return "WAITING"; 
        case STATE_TERMINATED: return "TERMINATED";
        default:                return "UNKNOWN";
    }
}
void print_pcb(const PCB *pcb) {
    printf("PID = %d STATE = %s remaining = %d bursts_done = %d IO_done_at = %d quantum = %d\n", pcb->spec.pid, state_to_string(pcb->state),
           pcb->remaining_cpu_time, pcb->bursts_completed, pcb->io_timetofinish, pcb->rr_used);
} // end print_pcb

int select_and_remove_srtf(ReadyQueue *queue, PCB *pcbs) {
    int best_queue_position = 0;
    int best_array_position = queue->front;
    int best_idx = queue->pcb_index[best_array_position];

    for (int i = 1; i < queue -> count; i++) {
        int array_position = (queue -> front + i) % MAX_PROCESSES;
        int candidate_idx = queue -> pcb_index[array_position];

        if (pcbs[candidate_idx].remaining_cpu_time < pcbs[best_idx].remaining_cpu_time) {
            best_queue_position = i;
            best_array_position = array_position;
            best_idx = candidate_idx;
        }
    } // end for

    // Shift every entry after best_queue_pos one slot forward, closing the gap.
    for (int i = best_queue_position; i < queue->count - 1; i++) {
        int from_position = (queue->front + i + 1) % MAX_PROCESSES;
        int to_position = (queue->front + i) % MAX_PROCESSES;
        queue->pcb_index[to_position] = queue->pcb_index[from_position];
    }
    queue->count--;
    return best_idx;
} // end select_and_remove_srtf
 // =================== helper functions ================
 // =============MY CODE===============STEP 1==================
 void enqueue (ReadyQueue *queue, int pcb_index){
    int open_slot = (queue->front + queue->count) % MAX_PROCESSES;
    queue->pcb_index[open_slot] = pcb_index;
    queue->count ++;
 } // end enqueue
 int dequeue(ReadyQueue *queue){
    //queue->pcb_index[] is an array with each proccesses index
    int remove = queue->pcb_index[queue->front]; //need ot look at that index to get the value
    int move = (queue->front + 1) % MAX_PROCESSES; // ifguring out where the next open slot is to move queue->front to
    queue->front = move;
    queue-> count --;
    return remove;
 } // end dequeue
 void initialize_pcbs(const ProcessSpec *specs, size_t count, PCB *pcbs) {
    for(size_t i =0; i < count; i++){
        // when looking at an array, the index is already telling it where to point to, 
        // therefore we use the '.' after the index instead of "->".
    pcbs[i].spec = specs[i];
    pcbs[i].remaining_cpu_time = specs[i].cpu_burst;
    pcbs[i].bursts_completed = 0;
    pcbs[i].io_timetofinish = -1;
    pcbs[i].rr_used = 0;
    pcbs[i].state = STATE_NEW;
    }
} // end initialize_pcbs
 // =============MY CODE===============STEP 1==================
 // =============MY CODE===============STEP 2==================
 bool all_terminated(const PCB *pcbs, size_t count) {
    for(size_t i = 0; i < count; i ++){
        if(pcbs[i].state != STATE_TERMINATED){
            return false;
        }
    }
    return true; 
    // return true only if EVERY pcb's state == STATE_TERMINATED
}
 // =============MY CODE============== STEP2 ===================
// =============MY CODE===============STEP 3==================
void dispatch_or_execute(PCB *pcbs, ReadyQueue *queue, CpuState *cpu, OutputContext *output, int time, const SimulationConfig *config) {

    if (cpu->activity == ACTIVITY_CPU) {
        int index = cpu->current_running_pcb;
        record_timeline_tick(output, time, ACTIVITY_CPU, pcbs[index].spec.pid);
        return;
    } // end if
    if (cpu->activity == ACTIVITY_CONTEXT_SWITCH) {
        cpu->time_remaining--;

        if (cpu->time_remaining <= 0) {
            // Switch is DONE now — only NOW do we pick who actually runs.
            int index;
            if (config->algorithm == ALG_SRTF) {
                index = select_and_remove_srtf(queue, pcbs);
            } else {
                index = dequeue(queue);
            }

            cpu->activity = ACTIVITY_CPU;
            cpu->current_running_pcb = index;

            pcbs[index].state = STATE_RUNNING;
            if (pcbs[index].remaining_cpu_time == 0) {
                pcbs[index].remaining_cpu_time = pcbs[index].spec.cpu_burst;
            }
            pcbs[index].rr_used = 0;

            log_transition(output, time, pcbs[index].spec.pid,
                STATE_READY, STATE_RUNNING, REASON_DISPATCH);
            record_timeline_tick(output, time, ACTIVITY_CPU, pcbs[index].spec.pid);
        } else {
            record_timeline_tick(output, time, ACTIVITY_CONTEXT_SWITCH, 0);
        }
        return;
    } // end big if

    // cpu->activity == ACTIVITY_IDLE
    if (queue->count > 0) {
        if (config->context_switch == 0) {
            // No switch cost so then we can immediately dispatch the next p.
            int index;
            if (config->algorithm == ALG_SRTF) {
                index = select_and_remove_srtf(queue, pcbs);
            } else {
                index = dequeue(queue);
            }
            cpu->activity = ACTIVITY_CPU;
            cpu->current_running_pcb = index;

            pcbs[index].state = STATE_RUNNING;
            if (pcbs[index].remaining_cpu_time == 0) {
                pcbs[index].remaining_cpu_time = pcbs[index].spec.cpu_burst;
            }
            pcbs[index].rr_used = 0;

            log_transition(output, time, pcbs[index].spec.pid,
                STATE_READY, STATE_RUNNING, REASON_DISPATCH);
            record_timeline_tick(output, time, ACTIVITY_CPU, pcbs[index].spec.pid);
        } else {
            cpu->activity = ACTIVITY_CONTEXT_SWITCH;
            cpu->time_remaining = config->context_switch;
            record_timeline_tick(output, time, ACTIVITY_CONTEXT_SWITCH, 0);
        }
    } else {
        record_timeline_tick(output, time, ACTIVITY_IDLE, 0);
    } // end else
} // end dispatch_or_execute

void admit_arrivals(PCB *pcbs, size_t count, int time, ReadyQueue *queue, OutputContext *output){
    for(size_t i = 0; i < count; i++){
        if((pcbs[i].spec.arrival_time == time) && (pcbs[i].state == STATE_NEW)){
            pcbs[i].state = STATE_READY;
            //log the transition
            enqueue(queue, i);
            log_transition(output, time, pcbs[i].spec.pid, 
                STATE_NEW, STATE_READY, REASON_ARRIVAL);
        } // end if
    } // end for
} // end admit_arrivals

void complete_io(PCB *pcbs, size_t count, int time, ReadyQueue *queue, OutputContext *output) {
    for(size_t i = 0; i < count; i++){
        if((pcbs[i].state == STATE_WAITING) && (pcbs[i].io_timetofinish == time)){
            pcbs[i].state = STATE_READY;
            pcbs[i].remaining_cpu_time = pcbs[i].spec.cpu_burst;
            enqueue(queue, i);
            log_transition(output, time, pcbs[i].spec.pid,
                STATE_WAITING, STATE_READY, REASON_IO_COMPLETE);
        } // end if
    } // end for
} // end complete_io

void resolve_cpu_interval(PCB *pcbs, int time, CpuState *cpu, OutputContext *output, ReadyQueue *queue, const SimulationConfig *config) {
    if (cpu->activity != ACTIVITY_CPU) {
        return;  // nothing to change
    }
    int index = cpu->current_running_pcb;
    pcbs[index].remaining_cpu_time--;
    pcbs[index].rr_used++;

    if (pcbs[index].remaining_cpu_time > 0) {
        if (config->algorithm == ALG_RR && pcbs[index].rr_used == config->quantum) {
            pcbs[index].state = STATE_READY;
            pcbs[index].rr_used = 0;
            log_transition(output, time, pcbs[index].spec.pid,
                STATE_RUNNING, STATE_READY, REASON_QUANTUM_EXPIRED);
            enqueue(queue, index);
            cpu->activity = ACTIVITY_IDLE;
            cpu->current_running_pcb = -1;
        }
        return;  // either still running, or just got re- equeued above
    }

    // Burst completed
    pcbs[index].bursts_completed++;
    if (pcbs[index].bursts_completed < pcbs[index].spec.num_cpu_bursts) {
        pcbs[index].state = STATE_WAITING;
        pcbs[index].io_timetofinish = time + pcbs[index].spec.io_burst;
        log_transition(output, time, pcbs[index].spec.pid,
            STATE_RUNNING, STATE_WAITING, REASON_CPU_BURST_COMPLETE);
    } else {
        pcbs[index].state = STATE_TERMINATED;
        log_transition(output, time, pcbs[index].spec.pid,
            STATE_RUNNING, STATE_TERMINATED, REASON_PROCESS_COMPLETE);
    }
    cpu->activity = ACTIVITY_IDLE;
    cpu->current_running_pcb = -1;
} // end resolve_cpu_interval
// =============MY CODE===============STEP 3==================
// =============MY CODE===============STEP 9==================
void maybe_preempt_srtf(PCB *pcbs, ReadyQueue *queue, CpuState *cpu, OutputContext *output, int time, const SimulationConfig *config) {
    if (config->algorithm != ALG_SRTF) {
        return; 
    }
    if (cpu->activity != ACTIVITY_CPU) {
        return;
    }
    int running_index = cpu->current_running_pcb;
    int best_queue_position = -1;
    for (int i = 0; i < queue->count; i++) {
        int array_position = (queue->front + i) % MAX_PROCESSES;
        int candidate_index = queue->pcb_index[array_position];

        if (best_queue_position == -1) {
            best_queue_position = i;
            continue;
        }
        int current_best_index = queue->pcb_index[(queue->front + best_queue_position) % MAX_PROCESSES];
        if (pcbs[candidate_index].remaining_cpu_time < pcbs[current_best_index].remaining_cpu_time) {
            best_queue_position = i;
        } 
    }
    if (best_queue_position == -1) {
        return;  // ready queue empty, nothing can preempt
    }

    int best_array_position = (queue->front + best_queue_position) % MAX_PROCESSES;
    int best_index = queue->pcb_index[best_array_position];

    if (pcbs[best_index].remaining_cpu_time >= pcbs[running_index].remaining_cpu_time) {
        return;  
    }

    pcbs[running_index].state = STATE_READY;
    log_transition(output, time, pcbs[running_index].spec.pid,
        STATE_RUNNING, STATE_READY, REASON_SRTF_PREEMPTION);
    enqueue(queue, running_index);

    cpu->activity = ACTIVITY_IDLE;
    cpu->current_running_pcb = -1;
} // end maybe_preempt_srtf
// =============MY CODE===============STEP 9==================

/*
 * Suggested helper decomposition (optional)
 * -----------------------------------------
 * You may find helpers with responsibilities like these useful:
 *
 *   initialize_pcbs(...)
 *   ready_enqueue(...)
 *   choose_next_process(...)
 *   admit_arrivals(...)
 *   complete_io(...)
 *   maybe_preempt_srtf(...)
 *   update_dispatch(...)
 *   advance_one_interval(...)
 *
 * Required event-loop order
 * -------------------------
 *
 *   while not all processes are terminated:
 *       resolve the CPU interval that ended at the current time
 *       admit arrivals in ascending PID order
 *       complete I/O in ascending PID order
 *       apply the SRTF preemption rule, if applicable
 *       complete or begin dispatch/context-switch activity
 *       record exactly one CPU, context-switch, or idle timeline tick
 *       advance time by one
 *
 * You must design and implement the loop yourself. The reference outputs care
 * about observable timing and ordering, not the names of your private helpers.
 */

/*
 * Run one complete scheduling simulation.
 *
 * Parameters
 * ----------
 * specs:
 *     A read-only array of validated process descriptions, sorted by ascending
 *     PID. Each entry supplies the arrival time, CPU- and I/O-burst lengths,
 *     and number of CPU bursts for one process. Copy entries into your PCBs if
 *     you need mutable per-process state. Do not modify or free this array.
 *
 * count:
 *     The number of ProcessSpec entries in specs.
 *
 * config:
 *     A read-only SimulationConfig containing the selected algorithm, the RR
 *     quantum, and the context-switch duration. The quantum affects scheduling
 *     only when config->algorithm is ALG_RR. Do not modify or free config.
 *
 * output:
 *     An already-open output context. Pass it to log_transition(),
 *     record_timeline_tick(), and write_metrics() as described above. main.c
 *     owns this context and will close it; do not close or free it here.
 *
 * Return true after the simulation and final metrics are written successfully.
 * Return false if initialization, simulation, or output generation fails.
 * main.c retains ownership of specs, config, and output in either case.
 */
bool run_simulation(const ProcessSpec *specs,
                    size_t count,
                    const SimulationConfig *config,
                    OutputContext *output)
{//start of function


    //====== my edits=========
    ReadyQueue ready_queue;
    CpuState cpu;

    PCB pcbs[MAX_PROCESSES];
    initialize_pcbs(specs, count, pcbs);
    
    cpu.activity = ACTIVITY_IDLE;
    cpu.current_running_pcb = -1;
    cpu.time_remaining = 0;
    cpu.next_pcb_index = -1;
    ready_queue.front = 0;
    ready_queue.count = 0;
    int time = 0;

    while (!all_terminated(pcbs, count)) {
    resolve_cpu_interval(pcbs, time, &cpu, output, &ready_queue, config);

    if (all_terminated(pcbs, count)) {
        break;
    }

    admit_arrivals(pcbs, count, time, &ready_queue, output);
    complete_io(pcbs, count, time, &ready_queue, output);
    maybe_preempt_srtf(pcbs, &ready_queue, &cpu, output, time, config);
    dispatch_or_execute(pcbs, &ready_queue, &cpu, output, time, config);
    time++;
}
    if(!write_metrics(output, specs, count)) {
        return false;
    }
    return true;
    //=====my edits =========
} // end of function
