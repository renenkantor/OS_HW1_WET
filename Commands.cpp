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

#define WHITESPACE ' '
#define PATH_MAX_CD 1024

string _ltrim(const std::string& s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for(std::string s; iss >> s; ) {
        args[i] = (char*)malloc(s.length()+1);
        memset(args[i], 0, s.length()+1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
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

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  // These are no arguments commands.
  if (firstWord == "pwd")
      return new GetCurrDirCommand(cmd_line);
  else if (firstWord == "showpid")
      return new ShowPidCommand(cmd_line);
  else if (firstWord.compare("jobs") == 0)
      return new JobsCommand(cmd_line, &jobs);
  // These are commands with arguments.
  else if (firstWord == "chprompt")
      return new ChangePromptCommand(cmd_line);
  else if (firstWord == "cd")
      return new ChangeDirCommand(cmd_line);
  else if (firstWord == "kill")
      return new KillCommand(cmd_line, &jobs);

  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

SmallShell::SmallShell() {
    SmallShell::prompt = "smash> ";
}

SmallShell::~SmallShell() = default;

Command::Command(const char *cmd_line) {
    this->cmd_line = cmd_line;
}

BuiltInCommand::BuiltInCommand(const char *cmdLine, const char *cmd_line) : Command(cmd_line) {

}

ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand(nullptr, cmd_line) {

}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line) : BuiltInCommand(nullptr, cmd_line) {

}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(nullptr, cmd_line) {

}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(nullptr, cmd_line) {
    // nothing to do here
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(nullptr, cmd_line) {
    // nothing to do here
}

void GetCurrDirCommand::execute() {
    char current_pwd[PATH_MAX_CD];
    if(getcwd(current_pwd, PATH_MAX_CD) != nullptr)
        std::cout << current_pwd << std::endl;
    else
        std::cout << "error in pwd";
}

void ShowPidCommand::execute() {
    std::cout << "smash pid is" << getpid() << std::endl;
}

void ChangePromptCommand::execute() {
    string new_prompt = _trim(cmd_line.substr(8));
    new_prompt = _trim(new_prompt.substr(0, cmd_line.find_first_of(" \n")));
    SmallShell& smash = SmallShell::getInstance();
    if(new_prompt.empty())
        smash.prompt = "smash> ";
    else
        smash.prompt = new_prompt + "> ";
}

void ChangeDirCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    if(cmd_line == "cd -") {
        if(smash.prev_wd.empty())
            perror("smash error: cd: OLDPWD not set");
        else {
            if(chdir(smash.prev_wd.c_str()) == 0)
                smash.prev_wd = smash.current_wd;
            else {
                perror("smash error: chdir failed");
            }

        }
    } else {
        int spaces = 0;
        string new_dir = _trim(_trim(cmd_line).substr(2, _trim(cmd_line).find_first_of('\n')));
        for (auto& letter : new_dir) {
            if (letter == ' ')
                spaces++;
        }
        // if there are any spaces after all the trimming it means two arguments.
        if (spaces != 0)
            perror("smash error: cd: too many arguments");
        else {
            if(chdir(new_dir.c_str()) == 0) {
                smash.prev_wd = smash.current_wd;
                smash.current_wd = new_dir;
            }
            else
                perror("smash error: chdir failed");
        }
    }
}

void KillCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    int job_id = -1, signum = -1, i = 0;
    cmd_line = _trim(cmd_line);
    cmd_line = _trim(cmd_line.substr(4));
    if(cmd_line[0] != '-')
        perror("smash error: kill: invalid arguments");
    else {
        signum = stoi(_trim(cmd_line.substr(1, 2)));
        if(signum < 1 || signum > 64)
            perror("smash error: kill failed");
        else {
            if(signum < 10)
                cmd_line = _trim(cmd_line.substr(2));
            else
                cmd_line = _trim(cmd_line.substr(3));
            for(auto l : cmd_line) {
                if (!isdigit(l)) {
                    perror("smash error: kill: invalid arguments");
                    return;
                }
            }
            job_id = stoi(cmd_line);
            JobEntry *job_to_handle;
            if((job_to_handle = smash.jobs.getJobById(job_id)) == nullptr) {
                string job_id_error = "smash error: kill: job-id ";
                job_id_error.append(to_string(job_id));
                job_id_error.append(" does not exist");
                perror(job_id_error.c_str());
            }
            else {
                if(kill(job_to_handle->process_id, signum) == -1)
                    perror("smash error: kill failed");
            }
        }
    }
}

JobEntry *JobsList::getJobById(int jobId) {
    for(auto &job : job_list) {
        if(job.job_id == jobId)
            return &job;
    }
    return nullptr;
}




JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(nullptr, cmd_line) {


}

int JobEntry::calc_job_elapsed_time() const {
    time_t *timer = nullptr;
    return (int)difftime(time(timer), start_time);
}

JobEntry::JobEntry(int job_id, int process_id, std::string job_command) {
    this->job_id = job_id;
    this->process_id = process_id;
    this->job_command = job_command;
    time_t *timer = nullptr;
    start_time = time(timer);
}

bool JobsComparor(const JobEntry& first, const JobEntry& second) {
    return (first.job_id < second.job_id);
}

void JobsCommand::execute() {
    SmallShell& smash = SmallShell::getInstance();
    // removes all finished jobs from the list.
    smash.jobs.job_list.erase(std::remove_if(smash.jobs.job_list.begin(), smash.jobs.job_list.end(), [](JobEntry const & cur_job) {
        return cur_job.job_status == JobEntry::JOB_STATUS::FINISHED;
    }),  smash.jobs.job_list.end());

    std::sort(smash.jobs.job_list.begin(), smash.jobs.job_list.end(), JobsComparor);
    for(auto &job : smash.jobs.job_list) {
        if(job.job_status == JobEntry::JOB_STATUS::STOPPED)
            std::cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id << job.calc_job_elapsed_time() << "(stopped)" << std::endl;
        else if (job.job_status == JobEntry::JOB_STATUS::UNFINISHED)
            std::cout << "[" << job.job_id << "]" << job.job_command << " : " << job.process_id << job.calc_job_elapsed_time() << std::endl;
    }
}


