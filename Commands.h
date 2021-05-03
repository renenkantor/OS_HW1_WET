#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <algorithm>
#include <list>
#include <unistd.h>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define PATH_MAX_CD 1024
#define BUFFER_SIZE 1024

using std::string;
const string WHITESPACE = " \n\r\t\f\v";

// no need to use return after fail because the macro does it itself.
#define SYS_CALL(return_val, command)     \
    do                                    \
    {                                     \
        return_val = command;             \
        if (return_val == -1) {           \
            smash_error(#command);        \
            return;                       \
        }                                 \
    } while (0)

static void smash_error(const string &syscall) {
    string error_message = "smash error: " + syscall.substr(0, syscall.find('(')) + " failed";
    perror(error_message.c_str());
}

class Command {

public:
    explicit Command(string &cmd_line) : cmd_line(cmd_line) {};
    string cmd_line;
    string un_proccessed_cmd;
    bool is_time_out = false;
    bool is_bg = false;
    int kill_time;

    virtual ~Command() = default;

    virtual void execute() = 0;
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(string &cmd_line) : Command(cmd_line) {
        BuiltInCommand::remove_background_sign(cmd_line);
    };

    static void remove_background_sign(string &cmd_line);

    virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(string &cmd_line) : Command(cmd_line) {};

    virtual ~ExternalCommand() = default;

    void execute() override;
};

class PipeCommand : public Command {
public:
    explicit PipeCommand(string &cmd_line, bool first, bool second) : Command(cmd_line), first_pipe(first),
                                                                      second_pipe(second) {};
    bool first_pipe;
    bool second_pipe;

    virtual ~PipeCommand() = default;

    void execute() override;
};

class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(string &cmd_line, bool first, bool second) : Command(cmd_line),
                                                                             first_redirection(first),
                                                                             second_redirection(second) {};
    bool first_redirection;
    bool second_redirection;

    virtual ~RedirectionCommand() = default;

    void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
    explicit ChangePromptCommand(string &cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ChangePromptCommand() = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    explicit ChangeDirCommand(string &cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(string &cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(string &cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobEntry {
public:
    JobEntry(int job_id, int process_id, string &job_command, time_t start_time, bool stopped, bool finished) :
            job_id(job_id), process_id(process_id), job_command(job_command), start_time(start_time),
            is_stopped(stopped), is_finished(finished) {};
    int job_id;
    int process_id;
    string job_command;
    time_t start_time;
    bool is_stopped;
    bool is_finished;

    int calc_job_elapsed_time() const;

    void continue_job();
};

class JobsList {
public:
    std::vector<JobEntry> job_list;

    void addJob(Command *cmd, int process_id, bool is_stopped);

    JobEntry *getMaxJob();

    void removeFinishedJobs();

    JobEntry *getLastStoppedJob();

    JobEntry *getJobById(int jobId);

    JobEntry *getJobByPId(int jobPId);

    void removeJobById(int jobId);

    void removeJobByPId(int jobPId);

    void update_max_id();
};

class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(string &cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};
    JobsList *jobs_list;

    virtual ~QuitCommand() = default;

    void execute() override;
};

class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(string &cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};
    JobsList *jobs_list;

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
public:
    KillCommand(string &cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};
    JobsList *jobs_list;

    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    ForegroundCommand(string &cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};
    JobsList *jobs_list;

    virtual ~ForegroundCommand() = default;

    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
public:
    BackgroundCommand(string &cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {};
    JobsList *jobs_list;

    virtual ~BackgroundCommand() = default;

    void execute() override;
};

class CatCommand : public BuiltInCommand {
public:
    explicit CatCommand(string &cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~CatCommand() = default;

    void execute() override;
};

class TimeOutList {
public:
    class TimeOutEntry {
    public:
        TimeOutEntry(const string &cmd_line, const string &un_proccessed_cmd, int pid, int duration, time_t start_time)
                : cmd_line(cmd_line), un_proccessed_cmd(un_proccessed_cmd), pid(pid), start_time(start_time),
                  duration(duration) {
            kill_time = duration + start_time;
        }

        string cmd_line;
        string un_proccessed_cmd;
        int pid = -1;
        time_t kill_time; // time when the alarm should go off.
        time_t start_time; // time when we wrote the command.
        time_t duration; // how long the actual timeout is (timeout _duration_ ...)
        ~TimeOutEntry() = default;
    };

    time_t cur_minimum_time;
    std::vector<TimeOutEntry> timeout_list;

    static bool timeComparor(const TimeOutEntry &first, const TimeOutEntry &second) {
        return (first.kill_time < second.kill_time);
    }

    void
    add_entry(const string &cmd_line, const string &un_proccessed_cmd, int pid, time_t duration, time_t start_time) {
        TimeOutEntry time_out_entry(cmd_line, un_proccessed_cmd, pid, duration, start_time);
        timeout_list.push_back(time_out_entry);
        std::sort(timeout_list.begin(), timeout_list.end(), timeComparor);
        // change alarm when we create a sole timeout, or the new kill time is the first to timeout.
        if (duration + start_time < cur_minimum_time || timeout_list.size() == 1) {
            cur_minimum_time = duration + start_time;
            time_t alarm_elapsed = cur_minimum_time - timeout_list.front().start_time;
            alarm(alarm_elapsed);
        }

        for (auto &t : timeout_list) {
            time_t new_kill = t.kill_time + t.start_time - start_time;
            t.kill_time = new_kill;
        }
        std::sort(timeout_list.begin(), timeout_list.end(), timeComparor);
        cur_minimum_time = timeout_list.front().kill_time;
    }



    /*void remove_entry(int pid) {
        if (timeout_list.empty())
            return;
        time_t prev_kill = timeout_list.front().kill_time;
        timeout_list.erase(timeout_list.begin()); // remove the latest timeout.
        if (timeout_list.empty())
            return;
        cur_minimum_time = timeout_list.front().kill_time;
        time_t alarm_elapsed = timeout_list.front().kill_time - prev_kill;
        alarm(alarm_elapsed);
    }*/
};

class TimeOutCommand : public Command {
public:
    explicit TimeOutCommand(string &cmd_line) : Command(cmd_line) {};

    virtual ~TimeOutCommand() = default;

    void execute() override;
};

class SmallShell {
public:
    SmallShell(): prompt("smash> "), prev_wd(""), current_fg_pid(-1), current_fg_job_id(-1),max_job_id(-1), curr_fg_command(nullptr) {} ;
    ~SmallShell() = default;
    string prompt;
    string prev_wd;
    int current_fg_pid;
    int current_fg_job_id;
    int max_job_id;
    Command *curr_fg_command;
    JobsList jobs;
    TimeOutList time_out_list;

    Command *CreateCommand(string &cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator

    // make SmallShell singleton
    static SmallShell &getInstance() {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    void executeCommand(string &cmd_line);
};

#endif //SMASH_COMMAND_H_