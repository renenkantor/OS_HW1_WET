#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
public:
    explicit Command(const char* cmd_line) : cmd_line(cmd_line) {};
    std::string cmd_line;
    virtual ~Command() = default;
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line) : Command(cmd_line) {} ;
    virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(const char* cmd_line) : Command(cmd_line) {} ;
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
public:
    explicit PipeCommand(const char* cmd_line) : Command(cmd_line) {} ;
    virtual ~PipeCommand() {}
    void execute() override;
};

class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const char* cmd_line) : Command(cmd_line) {} ;
    virtual ~RedirectionCommand() {}
    void execute() override;
    //void prepare() override;
    //void cleanup() override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
    explicit ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {} ;
    virtual ~ChangePromptCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    explicit ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {} ;
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {} ;
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {} ;
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobEntry {
public:
    JobEntry(int job_id, int process_id, const std::string& job_command, time_t start_time, bool stopped, bool finished) :
    job_id(job_id), process_id(process_id), job_command(job_command), start_time(start_time), is_stopped(stopped), is_finished(finished) {} ;
    int job_id;
    int process_id;
    time_t start_time;
    std::string job_command;
    bool is_stopped;
    bool is_finished;
    int calc_job_elapsed_time() const;
    void continue_job();
    void stop_job();
};

class JobsList {
public:
    std::vector<JobEntry> job_list;
    int max_job_id;
    void addJob(Command* cmd, int process_id);
    JobEntry * getMaxJob();
    //JobEntry * getLastJob(int* lastJobId);
    /*void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();*/
    JobEntry *getLastStoppedJob(int *jobId);
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
};

class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {} ;
    JobsList* jobs_list;
    virtual ~QuitCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {} ;
    JobsList* jobs_list;
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
public:
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {} ;
    JobsList* jobs_list;
    virtual ~KillCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {} ;
    JobsList* jobs_list;
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs_list(jobs) {} ;
    JobsList* jobs_list;
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class CatCommand : public BuiltInCommand {
public:
    explicit CatCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {} ;
    virtual ~CatCommand() {}
    void execute() override;
};

class SmallShell {
public:
    SmallShell();
    ~SmallShell() = default;
    std::string prompt;
    std::string current_wd;
    std::string prev_wd;
    JobsList jobs;
    int max_job_id;
    int current_fg_pid;
    int my_smash_pid;
    Command* curr_fg_command;
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator

    // make SmallShell singleton
    static SmallShell& getInstance() {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    void executeCommand(const char* cmd_line);
};

#endif //SMASH_COMMAND_H_