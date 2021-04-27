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

const std::string WHITESPACE = " \n\r\t\f\v";
#define PATH_MAX_CD 1024

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool checkFirstRedirection(const char *cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find('>') != std::string::npos) && (cmd_str.find(">>") == std::string::npos);
}

bool checkSecondRedirection(const char *cmd_line) {
    string cmd_str = _trim(cmd_line);
    return (cmd_str.find(">>") != std::string::npos);
}

/*
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {

    string cmd_s = _trim(string(cmd_line));
    string firstWord = _trim(cmd_s.substr(0, cmd_s.find_first_of(" \n")));
    // These are no arguments commands.
    if (checkFirstRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, true, false);
    else if (checkSecondRedirection(cmd_line))
        return new RedirectionCommand(cmd_line, false, true);
    else if (firstWord == "pwd")
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord == "showpid")
        return new ShowPidCommand(cmd_line);
    else if (firstWord.compare("jobs") == 0)
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
    else
        return new ExternalCommand(cmd_line);
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
    delete cmd;
}

SmallShell::SmallShell() : prompt("smash> "), prev_wd(""), current_fg_pid(-1), my_smash_pid(getpid()),
                           curr_fg_command(nullptr) {}


void GetCurrDirCommand::execute() {
    char current_pwd[PATH_MAX_CD];
    if (getcwd(current_pwd, PATH_MAX_CD) != nullptr)
        std::cout << current_pwd << std::endl;
    else
        std::cout << "error in pwd";
}

void ShowPidCommand::execute() {
    std::cout << "smash pid is " << getpid() << std::endl;
}

void ChangePromptCommand::execute() {
    string new_prompt = _trim(cmd_line.substr(8));
    new_prompt = _trim(new_prompt.substr(0, cmd_line.find_first_of(" \n")));
    SmallShell &smash = SmallShell::getInstance();
    if (new_prompt.empty())
        smash.prompt = "smash> ";
    else
        smash.prompt = new_prompt + "> ";
}

void ChangeDirCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    cmd_line = _trim(cmd_line);
    if (cmd_line == "cd -") {
        if (smash.prev_wd.empty())
            perror("smash error: cd: OLDPWD not set");
        else {
            if (chdir(smash.prev_wd.c_str()) == 0) {
                char current_pwd[PATH_MAX_CD];
                getcwd(current_pwd, PATH_MAX_CD);
                smash.prev_wd = string(current_pwd);
            }
            else {
                perror("smash error: chdir failed");
            }
        }
    } else {
        int spaces = 0;
        string new_dir = _trim(_trim(cmd_line).substr(2, _trim(cmd_line).find_first_of('\n')));
        for (auto &letter : new_dir) {
            if (letter == ' ')
                spaces++;
        }
        // if there are any spaces after all the trimming it means two arguments.
        if (spaces != 0)
            perror("smash error: cd: too many arguments");
        else {
            if (chdir(new_dir.c_str()) == 0) {
                smash.prev_wd = smash.current_wd;
                smash.current_wd = new_dir;
            } else
                perror("smash error: chdir failed");
        }
    }
}

void KillCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    int job_id = -1, signum = -1, i = 0;
    cmd_line = _trim(cmd_line);
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
    int pos;
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
            std::cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id
                      << job.calc_job_elapsed_time() << "(stopped)" << std::endl;
        else if (!job.is_finished)
            std::cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id
                      << job.calc_job_elapsed_time() << std::endl;
    }
}

void ForegroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    JobEntry *job_to_handle;
    char **args = new char *[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line.c_str(), args);
    int status;
    // too many arguments.
    if (num_of_args > 2) {
        perror("smash error: fg: invalid arguments");
        delete[]  args;
        return;
    } else if (num_of_args == 1) {
        // no arguments but job list is emtpy.
        if (smash.jobs.job_list.empty()) {
            perror("smash error: fg: jobs list is empty");
            delete[]  args;
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
            delete[]  args;
            return;
        } else {
            if (job_to_handle->is_stopped)
                job_to_handle->continue_job();
        }
    }
    smash.current_fg_pid = job_to_handle->process_id;
    smash.curr_fg_command = smash.CreateCommand(job_to_handle->job_command.c_str());
    cout << job_to_handle->job_command + " : " + to_string(job_to_handle->process_id) << std::endl;
    // wait until job_to_handled is finished or someone has stopped it (WUNTRACED).
    waitpid(job_to_handle->process_id, &status, WUNTRACED);
    smash.jobs.removeJobById(job_to_handle->job_id);
    smash.jobs.update_max_id();
    delete[] args;
}

void BackgroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    char **args = new char *[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line.c_str(), args);
    JobEntry *job_to_handle;
    if (num_of_args > 2) {
        perror("smash error: bg: invalid arguments");
        delete[] args;
        return;
    } else if (num_of_args == 1) {
        job_to_handle = smash.jobs.getLastStoppedJob(nullptr);
        if (job_to_handle == nullptr) {
            perror("smash error: bg: there is no stopped jobs to resume");
            delete[] args;
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
            delete[] args;
            return;
        }
        if (!job_to_handle->is_stopped) {
            string error_str = "smash error: bg: job-id ";
            error_str.append(string(args[1]));
            error_str.append("is already running in the background");
            perror(error_str.c_str());
            delete[] args;
            return;
        }
    }
    job_to_handle->continue_job();
    delete[] args;
}

void QuitCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    char **args = new char *[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line.c_str(), args);
    // with kill argument.
    if (num_of_args >= 2 && (strcmp(args[1], "kill") == 0)) {
        std::cout << "smash: sending SIGKILL signal to " << jobs_list->job_list.size() << " jobs:" << std::endl;
        for (auto &it : jobs_list->job_list) {
            if (kill(it.process_id, SIGKILL) == -1)
                perror("smash error: kill failed");
            else
                std::cout << it.process_id << ": " << it.job_command << std::endl;
        }
    }
    // without kill argument.
    else {
        for (auto &it : jobs_list->job_list) {
            if (kill(it.process_id, SIGKILL) == -1)
                perror("smash error: kill failed");
        }
    }
    delete[] args;
    exit(0);
}

void ExternalCommand::execute() {

}

void RedirectionCommand::execute() {
    if (first_redirection)
        execute_first();
    else if (second_redirection)
        execute_second();
}

void RedirectionCommand::execute_first() {

}

void RedirectionCommand::execute_second() {

}
