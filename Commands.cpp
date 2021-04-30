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

string _ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string _rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const string &cmd_line, vector<string> &args) {
    FUNC_ENTRY()
    string buf;
    stringstream ss(cmd_line);
    vector<std::string> tokens;
    while (ss >> buf)
        args.push_back(buf);
    return args.size();
    FUNC_EXIT()
}

bool _isBackgroundCommand(const string &cmd_line) {
    return cmd_line[cmd_line.find_last_not_of(WHITESPACE)] == '&';
}

string _removeBackgroundSign(string cmd_line) {
    // find last character other than spaces
    unsigned int idx = cmd_line.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return cmd_line;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return cmd_line;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[cmd_line.find_last_not_of(WHITESPACE, idx) + 1] = 0;
    return cmd_line;
}

// redirection >
bool checkFirstRedirection(const string cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find('>') != string::npos) && (cmd_str.find(">>") == string::npos);
}

// redirection >>
bool checkSecondRedirection(const string cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find(">>") != string::npos);
}

// pipe |
bool checkFirstPipe(const string cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find('|') != string::npos) && (cmd_str.find("|&") == string::npos);
}

// pipe |&
bool checkSecondPipe(const string &cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find("|&") != string::npos);
}

Command *SmallShell::CreateCommand(string &cmd_line) {

    string firstWord = _trim(cmd_line.substr(0, cmd_line.find_first_of(" \n")));
    if (checkFirstRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, true, false);
    else if (checkSecondRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, false, true);
    else if (checkFirstPipe(cmd_line))
        return new PipeCommand(cmd_line, true, false);
    else if (checkSecondPipe(cmd_line))
        return new PipeCommand(cmd_line, false, true);
    else if (firstWord == "pwd")
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord == "showpid")
        return new ShowPidCommand(cmd_line);
    else if (firstWord == "jobs")
        return new JobsCommand(cmd_line, &jobs);
    else if (firstWord == "chprompt")
        return new ChangePromptCommand(cmd_line);
    else if (firstWord == "cd")
        return new ChangeDirCommand(cmd_line);
    else if (firstWord == "kill")
        return new KillCommand(cmd_line, &jobs);
    else if (firstWord == "fg")
        return new ForegroundCommand(cmd_line, &jobs);
    else if (firstWord == "bg")
        return new BackgroundCommand(cmd_line, &jobs);
    else if (firstWord == "quit")
        return new QuitCommand(cmd_line, &jobs);
    else if (firstWord == "cat")
        return new CatCommand(cmd_line);
    else
        return new ExternalCommand(cmd_line);
    return nullptr;
}

void SmallShell::executeCommand(string &cmd_line) {
    cmd_line = _trim(cmd_line);
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
    delete cmd;
}

SmallShell::SmallShell() : prompt("smash> "), prev_wd(""), current_fg_pid(-1), my_smash_pid(getpid()),
                           curr_fg_command(nullptr) {
    char current_pwd[PATH_MAX_CD];
    if (getcwd(current_pwd, PATH_MAX_CD) != nullptr)
        current_wd = current_pwd;
}

void GetCurrDirCommand::execute() {
    char current_pwd[PATH_MAX_CD];
    if (getcwd(current_pwd, PATH_MAX_CD) != nullptr)
        cout << current_pwd << endl;
    else
        perror("smash error: pwd failed");
}

void ShowPidCommand::execute() {
    int ret_pid = 0;
    SYS_CALL(ret_pid, getpid());
    cout << "smash pid is " << ret_pid << endl;
}

void ChangePromptCommand::execute() {
    vector<string> args;
    if(_parseCommandLine(cmd_line, args) == 1)
        SmallShell::getInstance().prompt = "smash> ";
    else {
        SmallShell::getInstance().prompt = args[1];
        SmallShell::getInstance().prompt.append("> ");
    }
}

void ChangeDirCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    vector<string> args;
    int return_val;
    int num_of_args = _parseCommandLine(cmd_line, args);
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
        smash.prev_wd = string(current_pwd);
        SYS_CALL(return_val, chdir(args[1].c_str()));

    // change to prev dir.
    } else {
        if (smash.prev_wd.empty())
            perror("smash error: cd: OLDPWD not set");
        else {
            SYS_CALL(return_val, chdir(smash.prev_wd.c_str()));
            char current_pwd[PATH_MAX_CD];
            getcwd(current_pwd, PATH_MAX_CD);
            smash.prev_wd = string(current_pwd);
        }
    }
}

void KillCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    int job_id = -1, signum = -1;
    cmd_line = _trim(cmd_line.substr(4));
    if (cmd_line[0] != '-')
        perror("smash error: kill: invalid arguments");
    else {
        signum = stoi(_trim(cmd_line.substr(1, 2)));
        if (signum < 1 || signum > 64)
            perror("smash error: kill failed");
        else {
            if (signum < 10)
                cmd_line = _trim(cmd_line.substr(2));
            else
                cmd_line = _trim(cmd_line.substr(3));
            for (auto l : cmd_line) {
                if (!isdigit(l)) {
                    perror("smash error: kill: invalid arguments");
                    return;
                }
            }
            job_id = stoi(cmd_line);
            JobEntry *job_to_handle;
            if ((job_to_handle = smash.jobs.getJobById(job_id)) == nullptr) {
                string job_id_error = "smash error: kill: job-id ";
                job_id_error.append(to_string(job_id));
                job_id_error.append(" does not exist");
                perror(job_id_error.c_str());
            } else {
                if (kill(job_to_handle->process_id, signum) == -1)
                    perror("smash error: kill failed");
            }
        }
    }
}

JobEntry *JobsList::getJobById(int jobId) {
    for (auto &job : job_list) {
        if (job.job_id == jobId)
            return &job;
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    unsigned int pos;
    for (pos = 0; pos < job_list.size(); pos++) {
        if (job_list[pos].job_id == jobId) break;
    }
    job_list.erase(job_list.begin() + pos);
}

void JobsList::addJob(Command *cmd, int process_id) {
    time_t start_time = time(nullptr);
    JobEntry current_job(max_job_id + 1, process_id, cmd->cmd_line, start_time, false, false);
    max_job_id++;
    this->job_list.push_back(current_job);
}

bool JobsComparor(const JobEntry &first, const JobEntry &second) {
    return (first.job_id < second.job_id);
}

JobEntry *JobsList::getMaxJob() {
    std::sort(job_list.begin(), job_list.end(), JobsComparor);
    return &job_list.back();
}

JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    *jobId = 0;
    for (auto &it : job_list) {
        if (it.is_stopped) {
            *jobId = it.job_id;
            break;
        }
    }
    return getJobById(*jobId);
}

void JobsList::update_max_id() {
    int cur_max = 1;
    for (auto &it : job_list) {
        if (it.job_id > cur_max)
            cur_max = it.job_id;
    }
    max_job_id = cur_max;
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

void JobEntry::stop_job() {
    if (kill(process_id, SIGSTOP) == -1)
        perror("smash error: kill failed");
    else
        is_stopped = true;
}

void JobsCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    // removes all finished jobs from the list.
    smash.jobs.job_list.erase(
            std::remove_if(smash.jobs.job_list.begin(), smash.jobs.job_list.end(), [](JobEntry const &cur_job) {
                return cur_job.is_finished;
            }), smash.jobs.job_list.end());

    std::sort(smash.jobs.job_list.begin(), smash.jobs.job_list.end(), JobsComparor);
    for (auto &job : smash.jobs.job_list) {
        if (job.is_stopped)
            cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id
                 << job.calc_job_elapsed_time() << "(stopped)" << endl;
        else if (!job.is_finished)
            cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id
                 << job.calc_job_elapsed_time() << endl;
    }
}

void ForegroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    JobEntry *job_to_handle;
    vector<string> args;
    int num_of_args = _parseCommandLine(cmd_line, args);
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
        int job_id = stoi(args[1]);
        job_to_handle = smash.jobs.getJobById(job_id);
        // specific job does not exists in the list.
        if (job_to_handle == nullptr) {
            string error_str = "smash error: fg: job-id ";
            error_str.append(string(args[1]));
            error_str.append("does not exist");
            perror(error_str.c_str());
            return;
        } else {
            if (job_to_handle->is_stopped)
                job_to_handle->continue_job();
        }
    }
    smash.current_fg_pid = job_to_handle->process_id;
    smash.curr_fg_command = smash.CreateCommand(job_to_handle->job_command);
    cout << job_to_handle->job_command + " : " + to_string(job_to_handle->process_id) << endl;
    // wait until job_to_handled is finished or someone has stopped it (WUNTRACED).
    waitpid(job_to_handle->process_id, &status, WUNTRACED);
    smash.jobs.removeJobById(job_to_handle->job_id);
    smash.jobs.update_max_id();
}

void BackgroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    vector<string> args;
    int num_of_args = _parseCommandLine(cmd_line, args);
    JobEntry *job_to_handle;
    if (num_of_args > 2) {
        perror("smash error: bg: invalid arguments");
        return;
    } else if (num_of_args == 1) {
        job_to_handle = smash.jobs.getLastStoppedJob(nullptr);
        if (job_to_handle == nullptr) {
            perror("smash error: bg: there is no stopped jobs to resume");
            return;
        }
    } else {
        int job_id = stoi(string(args[1]));
        job_to_handle = smash.jobs.getJobById(job_id);
        if (job_to_handle == nullptr) {
            string error_str = "smash error: bg: job-id ";
            error_str.append(string(args[1]));
            error_str.append("does not exist");
            perror(error_str.c_str());
            return;
        }
        if (!job_to_handle->is_stopped) {
            string error_str = "smash error: bg: job-id ";
            error_str.append(string(args[1]));
            error_str.append("is already running in the background");
            perror(error_str.c_str());
            return;
        }
    }
    job_to_handle->continue_job();
}

void QuitCommand::execute() {
    vector<string> args;
    int num_of_args = _parseCommandLine(cmd_line, args);
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
    // without kill argument. Im not sure what to do here. According to piaaza, without kill we just need to exit and do nothing.
    /*else {
        for (auto &it : jobs_list->job_list) {
            if (kill(it.process_id, SIGKILL) == -1)
                perror("smash error: kill failed" << endl;
        }
    }*/
    exit(0);
}

void ExternalCommand::execute() {
    if (_isBackgroundCommand(cmd_line)) {
        is_forked = true;
        execute_forked();
    } else {
        is_forked = false;
        execute_non_forked();
    }
}

void ExternalCommand::execute_forked() {

}

void ExternalCommand::execute_non_forked() {

}


void RedirectionCommand::execute() {
    int flags = 0, red_pos = 0;
    string file_name = "";
    red_pos = cmd_line.find_first_of('>');
    if (first_redirection) {
        flags = O_RDWR | O_CREAT | O_TRUNC;
        file_name = _trim(cmd_line.substr(red_pos + 1));
    } else if (second_redirection) {
        flags = O_RDWR | O_CREAT | O_APPEND;
        file_name = _trim(cmd_line.substr(red_pos + 2));
    }

    string actual_command = _trim(cmd_line.substr(0, red_pos - 1));
    int new_out_fd;
    if ((new_out_fd = dup(STDOUT_FILENO)) == -1)
        perror("smash error: dup failed");
    if (close(STDOUT_FILENO) == -1)
        perror("smash error: close failed");

    int fd = open(file_name.c_str(), flags, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH | S_IWOTH);
    if (fd == -1)
        perror("smash error: open failed");
    SmallShell::getInstance().executeCommand(actual_command);
    if (close(STDOUT_FILENO) == -1)
        perror("smash error: close failed");
    if (dup(new_out_fd) == -1)
        perror("smash error: dup failed");
    if (close(new_out_fd) == -1)
        perror("smash error: close failed");
}

void CatCommand::execute() {
    vector<string> args;
    if (cmd_line[cmd_line.size() - 1] == '&')
        cmd_line = _trim(cmd_line.substr(0, cmd_line.size() - 1));
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args == 1) {
        perror("smash error: cat: not enough arguments");
        return;
    }

    char buff[BUFFER_SIZE];

    // start with 1 to skip the cat command itself.
    for (int i = 1; i < num_of_args; i++) {
        int fd = 0;
        if ((fd = open(args[i].c_str(), O_RDONLY)) == -1) {
            perror("smash error: open failed");
            continue;
        }
        ssize_t input_read = 0;
        while (true) {
            if ((input_read = read(fd, buff, sizeof(buff))) == -1) {
                perror("smash error: read failed");
                break;
            }
            if (input_read == 0) {
                break;
            } else {
                if (write(STDOUT_FILENO, buff, input_read) == -1) {
                    perror("smash error: write failed");
                    break;
                }
            }
        }
        if (close(fd) == -1)
            perror("smash error: close failed");
    }
}

void PipeCommand::execute() {
    // remove & if one exists.
    if (cmd_line[cmd_line.size() - 1] == '&')
        cmd_line = _trim(cmd_line.substr(0, cmd_line.size() - 1));
    int new_pipe[2], fd = 0, return_value;

    SYS_CALL(return_value, pipe(new_pipe));

    int left_command_pid = -1;
    int right_command_pid = -1;
    int del_pos = cmd_line.find_first_of('|');
    string left_command = _trim(cmd_line.substr(0, del_pos - 1));
    string right_command;
    if (first_pipe)
        right_command = _trim(cmd_line.substr(del_pos + 1));
    else if (second_pipe)
        right_command = _trim(cmd_line.substr(del_pos + 2));

    SYS_CALL(left_command_pid, fork());
    if (left_command_pid == 0) {
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
    } else {
        SYS_CALL(right_command_pid, fork());
        if (right_command_pid == 0) {
            SYS_CALL(return_value, close(new_pipe[1]));
            SYS_CALL(return_value, close(STDIN_FILENO));
            SYS_CALL(return_value, dup(new_pipe[0]));
            SYS_CALL(return_value, close(new_pipe[0]));
            SmallShell::getInstance().executeCommand(right_command);
            SYS_CALL(return_value, close(STDIN_FILENO));
        } else {
            SYS_CALL(return_value, close(new_pipe[0]));
            SYS_CALL(return_value, close(new_pipe[1]));
        }
    }

    if (left_command_pid == 0 || right_command_pid == 0) {
        exit(0);
    } else {
        SYS_CALL(return_value, waitpid(left_command_pid, nullptr, 0));
        SYS_CALL(return_value, waitpid(right_command_pid, nullptr, 0));
    }
}