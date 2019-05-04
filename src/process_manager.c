#include "process_manager.h"

static TABLE_HASH_FUNC(hash_psn)
{
    unsigned long result = ((ProcessSerialNumber*) key)->lowLongOfPSN;
    result = (result + 0x7ed55d16) + (result << 12);
    result = (result ^ 0xc761c23c) ^ (result >> 19);
    result = (result + 0x165667b1) + (result << 5);
    result = (result + 0xd3a2646c) ^ (result << 9);
    result = (result + 0xfd7046c5) + (result << 3);
    result = (result ^ 0xb55a4f09) ^ (result >> 16);
    return result;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static TABLE_COMPARE_FUNC(compare_psn)
{
    Boolean result;
    SameProcess((ProcessSerialNumber*) key_a, (ProcessSerialNumber*) key_b, &result);
    return result == 1;
}
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static void
process_manager_add_running_processes(struct process_manager *pm)
{
    ProcessSerialNumber psn = { kNoProcess, kNoProcess };
    while (GetNextProcess(&psn) == noErr) {
        struct process *process = process_create(psn);
        if (!process) continue;

        if (!process->background && !process->lsuielement && !process->xpc) {
            process_manager_add_process(pm, process);
        } else {
            process_destroy(process);
        }
    }
}
#pragma clang diagnostic pop

struct process *process_manager_find_process(struct process_manager *pm, ProcessSerialNumber *psn)
{
    // return table_find(&pm->process, psn);
    struct process **it = table_find(&pm->process, psn);
    return it ? *it : NULL;
}

void process_manager_remove_process(struct process_manager *pm, ProcessSerialNumber *psn)
{
    table_remove(&pm->process, psn);
}

void process_manager_add_process(struct process_manager *pm, struct process *process)
{
    table_add(&pm->process, &process->psn, &process);
}

void process_manager_init(struct process_manager *pm)
{
    pm->target = GetApplicationEventTarget();
    pm->handler = NewEventHandlerUPP(process_handler);
    pm->type[0].eventClass = kEventClassApplication;
    pm->type[0].eventKind = kEventAppLaunched;
    pm->type[1].eventClass = kEventClassApplication;
    pm->type[1].eventKind = kEventAppTerminated;
    table_init(&pm->process, 125, hash_psn, compare_psn);
    process_manager_add_running_processes(pm);
}

bool process_manager_begin(struct process_manager *pm)
{
    return InstallEventHandler(pm->target, pm->handler, 2, pm->type, pm, &pm->ref) == noErr;
}

bool process_manager_end(struct process_manager *pm)
{
    return RemoveEventHandler(pm->ref) == noErr;
}