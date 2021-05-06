#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <iostream>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

void ctrlZHandler(int sig_num) {
    SmallShell &smash = SmallShell::getInstance();
    smash.jobs.removeFinishedJobs();
    int curr_pid = smash.current_fg_pid;
    cout << "smash: got ctrl-Z" << endl;
    // nothing is in the foreground now.
    if (curr_pid == -1)
        return;
    int return_value;
    SYS_CALL(return_value, kill(curr_pid, SIGSTOP));
    cout << "smash: process " << curr_pid << " was stopped" << endl;
    JobEntry *job = smash.jobs.getJobByPId(curr_pid);
    // if job is not in the list, add it
    if (job == nullptr) {
        smash.jobs.addJob(smash.curr_fg_command, curr_pid, true);
    } else {
        job->is_stopped = true;
        job->start_time = time(nullptr); // reset the time of the job after it has stopped.
    }

    smash.jobs.getJobByPId(curr_pid)->is_stopped = true;
    smash.current_fg_pid = -1;
    smash.curr_fg_command = nullptr;
}

void ctrlCHandler(int sig_num) {
    SmallShell &smash = SmallShell::getInstance();
    int curr_pid = smash.current_fg_pid;
    cout << "smash: got ctrl-C" << endl;
    // nothing in fg.
    if (curr_pid == -1)
        return;
    int return_value;
    SYS_CALL(return_value, kill(curr_pid, SIGKILL));

    cout << "smash: process " << curr_pid << " was killed" << endl;
}

void alarmHandler(int sig_num) {
    SmallShell &smash = SmallShell::getInstance();
    if (smash.time_out_list.timeout_list.empty())
        return;
    int pid = smash.time_out_list.timeout_list.begin()->pid;
    // this alarm job has removed from the jobs list so remove it.
    if(smash.time_out_list.timeout_list.begin()->is_timeout_bg && smash.jobs.getJobByPId(pid) == nullptr) {
        smash.time_out_list.remove_entry(pid);
        return;
    }

    cout << "smash: got an alarm" << endl;
    int return_value;

    SYS_CALL(return_value, kill(pid, sig_num));
    cout << "smash: " << smash.time_out_list.timeout_list.begin()->un_proccessed_cmd << " timed out!" << endl;
    smash.time_out_list.timeout_list.erase(smash.time_out_list.timeout_list.begin()); // remove the recent timeout alarm.
    // set up new alarm for the next entry, if one exists.
    if (!smash.time_out_list.timeout_list.empty()) {
        alarm(difftime(smash.time_out_list.timeout_list.begin()->kill_time, time(nullptr)));
    }
    smash.jobs.removeJobByPId(pid);
}