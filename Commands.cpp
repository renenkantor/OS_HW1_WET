#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <signal.h>
#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

// ***********************************************************************************************************************************
// **********************************                FUNCTIONS FOR JOBS                 **********************************************
// ***********************************************************************************************************************************

JobEntry *JobsList::getJobById(int jobId) {
    for (auto &job : job_list) {
        if (job.job_id == jobId)
            return &job;
    }
    return nullptr;
}

JobEntry *JobsList::getJobByPId(int jobPId) {
    for (auto &job : job_list) {
        if (job.process_id == jobPId)
            return &job;
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    unsigned int pos;
    if (job_list.empty())
        return;
    for (pos = 0; pos < job_list.size(); pos++) {
        if (job_list[pos].job_id == jobId) break;
    }
    job_list.erase(job_list.begin() + pos);
}

void JobsList::removeJobByPId(int jobPId) {
    unsigned int pos;
    if (job_list.empty())
        return;
    for (pos = 0; pos < job_list.size(); pos++) {
        if (job_list[pos].process_id == jobPId) break;
    }
    job_list.erase(job_list.begin() + pos);
}

void JobsList::addJob(Command *cmd, int process_id, bool is_stopped) {
    time_t start_time = time(nullptr);
    string job_command = cmd->un_proccessed_cmd;

    int new_id;
    if (job_list.empty())
        new_id = 1;
    else
        new_id = SmallShell::getInstance().max_job_id + 1;
    JobEntry current_job(new_id, process_id, job_command, start_time, is_stopped, false);
    job_list.push_back(current_job);
    SmallShell::getInstance().max_job_id++;
    update_max_id();
}

bool JobsComparor(const JobEntry &first, const JobEntry &second) {
    return (first.job_id < second.job_id);
}

JobEntry *JobsList::getMaxJob() {
    std::sort(job_list.begin(), job_list.end(), JobsComparor);
    return &job_list.back();
}

JobEntry *JobsList::getLastStoppedJob() {
    int job_id;
    for (auto &it : job_list) {
        if (it.is_stopped) {
            job_id = it.job_id;
            break;
        }
    }
    return getJobById(job_id);
}

void JobsList::update_max_id() {
    int cur_max = 1;
    for (auto &it : job_list) {
        if (it.job_id > cur_max)
            cur_max = it.job_id;
    }
    SmallShell::getInstance().max_job_id = cur_max;
}

void JobsList::removeFinishedJobs() {
    if (job_list.empty()) return;
    int wait_res = waitpid(-1, nullptr, WNOHANG);
    // the problem with timeout not in job list is here !!!!!!!!!!!!!!!!!!!
    while (wait_res > 0) {
        removeJobByPId(wait_res);
        wait_res = waitpid(-1, nullptr, WNOHANG);
    }
    update_max_id();
}

int JobEntry::calc_job_elapsed_time() const {
    time_t *timer = nullptr;
    return (int) difftime(time(timer), start_time);
}

void JobEntry::continue_job() {
    if (kill(process_id, SIGCONT) == -1)
        perror("smash error: kill failed");
    else
        is_stopped = false;
}

// ***********************************************************************************************************************************
// **********************************                STRING FUNCTIONS                 ************************************************
// ***********************************************************************************************************************************

string left_trim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string right_trim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string both_trim(const string &s) {
    return right_trim(left_trim(s));
}

int parseCommandLine(const string &cmd_line, vector<string> &args) {
    FUNC_ENTRY()
    string buf;
    stringstream ss(both_trim(cmd_line));
    vector<std::string> tokens;
    while (ss >> buf)
        args.push_back(buf);
    return args.size();
    FUNC_EXIT()
}

bool isBackgroundCommand(string &cmd_line) {
    return cmd_line[cmd_line.find_last_not_of(WHITESPACE)] == '&';
}

void BuiltInCommand::remove_background_sign(string &cmd_line) {
    int i;
    bool to_remove = false;
    for (i = cmd_line.size() - 1; i >= 0; i--) {
        if (cmd_line[i] == '&') {
            to_remove = true;
            break;
        }
    }
    if (to_remove)
        cmd_line.erase(i);

}

// redirection >
bool checkFirstRedirection(const string &cmd_line) {
    return (cmd_line.find('>') != string::npos) && (cmd_line.find(">>") == string::npos);
}

// redirection >>
bool checkSecondRedirection(const string &cmd_line) {
    return (cmd_line.find(">>") != string::npos);
}

// pipe |
bool checkFirstPipe(const string &cmd_line) {
    return (cmd_line.find('|') != string::npos) && (cmd_line.find("|&") == string::npos);
}

// pipe |&
bool checkSecondPipe(string &cmd_line) {
    return (cmd_line.find("|&") != string::npos);
}

Command *SmallShell::CreateCommand(string &cmd_line) {

    string firstWord = both_trim(cmd_line.substr(0, cmd_line.find_first_of(" \n")));
    // **************       SPECIAL COMMANDS       **************
    if (checkFirstRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, true, false);
    else if (checkSecondRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, false, true);
    else if (checkFirstPipe(cmd_line))
        return new PipeCommand(cmd_line, true, false);
    else if (checkSecondPipe(cmd_line))
        return new PipeCommand(cmd_line, false, true);
        // **************       BUILT IN COMMANDS       **************
    else if (firstWord == "pwd" || firstWord == "pwd&")
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord == "showpid" || firstWord == "showpid&")
        return new ShowPidCommand(cmd_line);
    else if (firstWord == "jobs" || firstWord == "jobs&")
        return new JobsCommand(cmd_line, &jobs);
    else if (firstWord == "chprompt" || firstWord == "chprompt&")
        return new ChangePromptCommand(cmd_line);
    else if (firstWord == "cd" || firstWord == "cd&")
        return new ChangeDirCommand(cmd_line);
    else if (firstWord == "kill" || firstWord == "kill&")
        return new KillCommand(cmd_line, &jobs);
    else if (firstWord == "fg" || firstWord == "fg&")
        return new ForegroundCommand(cmd_line, &jobs);
    else if (firstWord == "bg" || firstWord == "bg&")
        return new BackgroundCommand(cmd_line, &jobs);
    else if (firstWord == "quit" || firstWord == "quit&")
        return new QuitCommand(cmd_line, &jobs);
    else if (firstWord == "cat" || firstWord == "cat&")
        return new CatCommand(cmd_line);
        // **************       EXTERNAL COMMANDS       **************
    else if (firstWord == "timeout")
        return new TimeOutCommand(cmd_line);
    else
        return new ExternalCommand(cmd_line);
    return nullptr;
}

void SmallShell::executeCommand(string &cmd_line) {
    jobs.removeFinishedJobs();
    jobs.update_max_id();
    Command *cmd = CreateCommand(cmd_line);
    cmd->un_proccessed_cmd = cmd_line;
    cmd->execute();
    delete cmd;
}

// ***********************************************************************************************************************************
// **********************************                BUILT IN EXECUTE                 ************************************************
// ***********************************************************************************************************************************

void ChangePromptCommand::execute() {
    vector<string> args;
    if (parseCommandLine(cmd_line, args) == 1 || args[1] == "&")
        SmallShell::getInstance().prompt = "smash> ";
    else {
        SmallShell::getInstance().prompt = args[1] + "> ";
    }
}

void ShowPidCommand::execute() {
    int ret_pid = 0;
    SYS_CALL(ret_pid, getpid());
    cout << "smash pid is " << ret_pid << endl;
}

void GetCurrDirCommand::execute() {
    char current_pwd[PATH_MAX_CD];
    if (getcwd(current_pwd, PATH_MAX_CD) != nullptr)
        cout << current_pwd << endl;
    else
        perror("smash error: pwd failed");
}

void ChangeDirCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    vector<string> args;
    int return_val;
    int num_of_args = parseCommandLine(cmd_line, args);

    // too many args.
    if (num_of_args >= 3) {
        perror("smash error: cd: too many arguments");
        return;
    }
    // no args - do nothing.
    if (num_of_args == 1)
        return;
    // change to specific dir.
    if (args[1] != "-") {
        char current_pwd[PATH_MAX_CD];
        getcwd(current_pwd, PATH_MAX_CD);
        SYS_CALL(return_val, chdir(args[1].c_str()));
        smash.prev_wd = string(current_pwd);
        // change to prev dir.
    } else {
        if (smash.prev_wd.empty()) {
            perror("smash error: cd: OLDPWD not set");
            return;
        } else {
            char current_pwd[PATH_MAX_CD];
            getcwd(current_pwd, PATH_MAX_CD);
            SYS_CALL(return_val, chdir(smash.prev_wd.c_str()));
            smash.prev_wd = string(current_pwd);
        }
    }
}

void JobsCommand::execute() {
    if (jobs_list->job_list.empty())
        return;
    std::sort(jobs_list->job_list.begin(), jobs_list->job_list.end(), JobsComparor);
    for (auto &job : jobs_list->job_list) {
        if (job.is_stopped)
            cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id << " "
                 << job.calc_job_elapsed_time() << " secs (stopped)" << endl;
        else if (!job.is_finished)
            cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id << " " <<
                 job.calc_job_elapsed_time() << " secs" << endl;
    }
}

void KillCommand::execute() {
    vector<string> args;
    int num_of_args = parseCommandLine(cmd_line, args);

    if (num_of_args != 3 || args[1][0] != '-') {
        perror("smash error: kill: invalid arguments");
        return;
    }

    // removes - sign from the first number.
    args[1].erase(0, 1);
    bool check_if_second_is_int = (args[1].find_first_not_of("-0123456789") == std::string::npos);
    bool check_if_third_is_int = (args[2].find_first_not_of("-0123456789") == std::string::npos);

    // second and third argument must be numbers.
    if (!check_if_second_is_int || !check_if_third_is_int) {
        perror("smash error: kill: invalid arguments");
        return;
    }

    SmallShell &smash = SmallShell::getInstance();
    int signum = stoi(args[1]);
    int job_id = stoi(args[2]);
    JobEntry *job_to_handle;

    if ((job_to_handle = smash.jobs.getJobById(job_id)) == nullptr) {
        string job_id_error = "smash error: kill: job-id " + args[2] + " does not exist";
        perror(job_id_error.c_str());
    } else {
        // done with error handling. Now execute kill.
        int return_value;
        SYS_CALL(return_value, kill(job_to_handle->process_id, signum));
        cout << "signal number " << args[1] << " was sent to pid " << job_to_handle->process_id << endl;
        // TODO - should we assign is_stopped if signum == SIGSTOP?
    }
}

void ForegroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    JobEntry *job_to_handle = nullptr;
    vector<string> args;
    int num_of_args = parseCommandLine(cmd_line, args);
    int status;
    // too many arguments.
    if (num_of_args > 2) {
        perror("smash error: fg: invalid arguments");
        return;
    } else if (num_of_args == 1) {
        // no arguments but job list is emtpy.
        if (smash.jobs.job_list.empty()) {
            perror("smash error: fg: jobs list is empty");
            return;
        }
            // no arguments so get the maximum job.
        else {
            job_to_handle = smash.jobs.getMaxJob();
            if (job_to_handle->is_stopped)
                job_to_handle->continue_job();
        }
    }
        // specific job handling.
    else {
        bool check_if_id_is_num = args[1].find_first_not_of("-0123456789") == std::string::npos;
        if(!check_if_id_is_num) {
            perror("smash error: fg: invalid arguments");
            return;
        }

        int job_id = stoi(args[1]);
        job_to_handle = smash.jobs.getJobById(job_id);
        // specific job does not exists in the list.
        if (job_to_handle == nullptr) {
            string error_str = "smash error: fg: job-id " + args[1] + " does not exist";
            perror(error_str.c_str());
            return;
        } else {
            if (job_to_handle->is_stopped)
                job_to_handle->continue_job();
        }
    }
    smash.current_fg_pid = job_to_handle->process_id;
    smash.current_fg_job_id = job_to_handle->job_id;
    smash.curr_fg_command = smash.CreateCommand(job_to_handle->job_command);
    cout << job_to_handle->job_command + " : " + to_string(job_to_handle->process_id) << endl;
    // wait until job_to_handled is finished or someone has stopped it (WUNTRACED).
    if (waitpid(job_to_handle->process_id, &status, WUNTRACED) < 0) {
        perror("smash error: waitpid failed");
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        smash.jobs.removeJobById(job_to_handle->job_id);
        smash.current_fg_pid = -1;
        smash.current_fg_job_id = -1;
    }
    smash.jobs.update_max_id();
}

void BackgroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    vector<string> args;
    int num_of_args = parseCommandLine(cmd_line, args);
    JobEntry *job_to_handle;
    // invalid arguments.
    if (num_of_args > 2) {
        perror("smash error: bg: invalid arguments");
        return;
    }

    if (num_of_args == 1) {
        job_to_handle = smash.jobs.getLastStoppedJob();
        // jobs list is empty.
        if (job_to_handle == nullptr) {
            perror("smash error: bg: there is no stopped jobs to resume");
            return;
        }
        // handle specific job.
    } else {
        bool check_if_id_is_num = args[1].find_first_not_of("-0123456789") == std::string::npos;
        if(!check_if_id_is_num) {
            perror("smash error: bg: invalid arguments");
            return;
        }
        int job_id = stoi(args[1]);
        job_to_handle = smash.jobs.getJobById(job_id);
        if (job_to_handle == nullptr) {
            string error_str = "smash error: bg: job-id " + args[1] + " does not exist";
            perror(error_str.c_str());
            return;
        }
        if (!job_to_handle->is_stopped) {
            string error_str = "smash error: bg: job-id " + args[1] + " is already running in the background";
            perror(error_str.c_str());
            return;
        }
    }
    job_to_handle->continue_job();
}

void QuitCommand::execute() {
    vector<string> args;
    int num_of_args = parseCommandLine(cmd_line, args);
    // with kill argument.
    if (num_of_args >= 2 && args[1] == "kill") {
        cout << "smash: sending SIGKILL signal to " << jobs_list->job_list.size() << " jobs:" << endl;
        for (auto &it : jobs_list->job_list) {
            if (kill(it.process_id, SIGKILL) == -1)
                perror("smash error: kill failed");
            else
                cout << it.process_id << ": " << it.job_command << endl;
        }
    }
    // TODO : we need to deal with deleting quit command itself before exiting.
    delete this;
    exit(0);
}

// ***********************************************************************************************************************************
// **********************************                EXTERNAL EXECUTE                 ************************************************
// ***********************************************************************************************************************************

void ExternalCommand::execute() {

    bool is_background = isBackgroundCommand(cmd_line);
    SmallShell &smash = SmallShell::getInstance();
    string cmd_line_with_bg;
    if (is_background) {
        cmd_line_with_bg = cmd_line;
        BuiltInCommand::remove_background_sign(cmd_line);
    }
    char *cmd_str = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_str, cmd_line.c_str());

    int pid = fork();

    if (pid < 0) {
        perror("smash error: fork failed");
        delete[] cmd_str;
        return;
    } else if (pid == 0) {
        setpgrp();
        char *argv[] = {(char *) "/bin/bash", (char *) "-c", cmd_str, nullptr};
        if (execv(argv[0], argv) < 0) {
            perror("smash error: execv failed");
            delete[] cmd_str;
            return;
        }
    } else {
        if (this->is_time_out)
            smash.time_out_list.add_entry(cmd_line, un_proccessed_cmd, pid, kill_time, time(nullptr), is_background);

        if (is_background) {
            cmd_line = cmd_line_with_bg;
            smash.jobs.addJob(this, pid, false);
        } else {
            smash.current_fg_pid = pid;
            smash.curr_fg_command = this;
            waitpid(pid, nullptr, 0 | WUNTRACED);
            smash.current_fg_pid = -1;
            smash.curr_fg_command = nullptr;
        }
    }

    delete[] cmd_str;
}

// ***********************************************************************************************************************************
// **********************************                SPECIAL EXECUTE                 *************************************************
// ***********************************************************************************************************************************

void RedirectionCommand::execute() {
    int flags = 0, red_pos = 0;
    string file_name;
    red_pos = cmd_line.find_first_of('>');
    if (first_redirection) {
        flags = O_RDWR | O_CREAT | O_TRUNC;
        file_name = both_trim(cmd_line.substr(red_pos + 1));
    } else if (second_redirection) {
        flags = O_RDWR | O_CREAT | O_APPEND;
        file_name = both_trim(cmd_line.substr(red_pos + 2));
    }
    string actual_command = both_trim(cmd_line.substr(0, red_pos));
    int return_value, fd, tmp_stdout;

    // switch between stdout to user file.
    SYS_CALL(fd, open(file_name.c_str(), flags, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH | S_IWOTH));
    SYS_CALL(tmp_stdout, dup(STDOUT_FILENO));
    SYS_CALL(return_value, close(STDOUT_FILENO));
    SYS_CALL(return_value, dup(fd));

    SmallShell::getInstance().executeCommand(actual_command);

    // switch back to stdout.
    SYS_CALL(return_value, dup2(tmp_stdout, STDOUT_FILENO));
    SYS_CALL(return_value, close(fd));
    SYS_CALL(return_value, close(tmp_stdout));
}

void CatCommand::execute() {
    vector<string> args;
    remove_background_sign(cmd_line);
    int num_of_args = parseCommandLine(cmd_line, args);
    if (num_of_args == 1) {
        perror("smash error: cat: not enough arguments");
        return;
    }

    // start with 1 to skip the cat command itself.
    for (int i = 1; i < num_of_args; i++) {
        int fd = 0;
        // try to open current file.
        if ((fd = open(args[i].c_str(), O_RDONLY)) == -1) {
            perror("smash error: open failed");
            continue;
        }
        ssize_t input_read = 0;
        // start reading and writing.
        while (true) {
            char buff[BUFFER_SIZE];
            if ((input_read = read(fd, buff, BUFFER_SIZE)) == -1) {
                perror("smash error: read failed");
                break;
            }
            // when we finish the file, input read will be 0 so break thw while and move to next file.
            if (input_read == 0)
                break;

            if (write(STDOUT_FILENO, buff, input_read) == -1) {
                perror("smash error: write failed");
                break;
            }

        }
        if (close(fd) == -1)
            perror("smash error: close failed");
    }
}

void PipeCommand::execute() {

    int new_pipe[2], fd = 0, return_value;
    SYS_CALL(return_value, pipe(new_pipe));

    int left_command_pid = -1;
    int right_command_pid = -1;
    int del_pos = cmd_line.find_first_of('|');
    string left_command = both_trim(cmd_line.substr(0, del_pos));
    string right_command;
    if (first_pipe)
        right_command = both_trim(cmd_line.substr(del_pos + 1));
    else if (second_pipe)
        right_command = both_trim(cmd_line.substr(del_pos + 2));

    SYS_CALL(left_command_pid, fork());
    if (left_command_pid == 0) {
        setpgrp();
        if (second_pipe)
            fd = STDERR_FILENO;
        else
            fd = STDOUT_FILENO;

        SYS_CALL(return_value, close(fd));
        SYS_CALL(return_value, close(new_pipe[0]));
        SYS_CALL(return_value, dup(new_pipe[1]));
        SYS_CALL(return_value, close(new_pipe[1]));
        SmallShell::getInstance().executeCommand(left_command);
        SYS_CALL(return_value, close(fd));
        exit(0);
    } else {
        SYS_CALL(right_command_pid, fork());
        if (right_command_pid == 0) {
            setpgrp();
            SYS_CALL(return_value, close(new_pipe[1]));
            SYS_CALL(return_value, close(STDIN_FILENO));
            SYS_CALL(return_value, dup(new_pipe[0]));
            SYS_CALL(return_value, close(new_pipe[0]));
            SmallShell::getInstance().executeCommand(right_command);
            SYS_CALL(return_value, close(STDIN_FILENO));
            exit(0);
        } else {
            SYS_CALL(return_value, close(new_pipe[0]));
            SYS_CALL(return_value, close(new_pipe[1]));
            SYS_CALL(return_value, waitpid(left_command_pid, nullptr, 0));
            SYS_CALL(return_value, waitpid(right_command_pid, nullptr, 0));
        }
    }
}

void TimeOutCommand::execute() {
    vector<string> args;
    string un_proccessed_cmd = cmd_line; // this is used only for printing.
    int num_of_args = parseCommandLine(cmd_line, args);
    if (num_of_args < 3) {
        perror("smash error: timeout: invalid arguments");
        return;
    }

    bool check_if_duration_is_int = (args[1].find_first_not_of("0123456789") == std::string::npos);
    if (!check_if_duration_is_int) {
        perror("smash error: timeout: invalid arguments");
        return;
    }
    SmallShell &smash = SmallShell::getInstance();
    string new_cmd_str;
    for (int i = 2; i < num_of_args; i++)
        new_cmd_str += args[i] + " ";

    new_cmd_str = both_trim(new_cmd_str);
    Command *new_cmd = smash.CreateCommand(new_cmd_str);

    new_cmd->is_time_out = true;
    new_cmd->kill_time = stoi(args[1]);
    new_cmd->un_proccessed_cmd = un_proccessed_cmd;
    new_cmd->execute();
    delete new_cmd;
}