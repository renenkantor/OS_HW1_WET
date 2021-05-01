#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {



    if (signal(SIGTSTP, ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    struct sigaction sig_action {};
    sig_action.sa_flags = SA_RESTART;
    sig_action.sa_handler = alarmHandler;

    if (sigaction(SIGALRM, &sig_action, nullptr)) {
        perror("smash error: failed to set alarm handler");
    }

    SmallShell &smash = SmallShell::getInstance();
    while (true) {
        std::cout << smash.prompt;
        string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line);
    }
    return 0;
}