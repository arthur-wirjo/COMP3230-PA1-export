/*
 * Student Name: Wirjolukito Arthur
 * Student Number: 3036445804
 * Development Platform: Ubuntu 24.04 (native)
 * Remarks: Completed all features including process creation and execution,
 *          pipelining (up to 5 commands), SIGINT signal handling, and built-in
 *          commands (exit and watch).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

struct ProcessInformation {
    char state[4];
    int cpuid;
    float utime;
    float stime;
    unsigned long vsize;
    unsigned long minflt;
    unsigned long majflt;
};

void sigint_handler(int sig) {
    printf("\n## 3230yash >> ");
    fflush(stdout);
}

void child_snapshot(pid_t pid, struct ProcessInformation *process_information) {
    char path[64];
    sprintf(path, "/proc/%d/stat", pid);
    FILE *stat_file = fopen(path, "r");
    if (stat_file == NULL) { // edge case handling
        return;
    }
    char stat_buffer[2048];
    if (fgets(stat_buffer, sizeof(stat_buffer), stat_file) != NULL) {
        // find closing parenthesis of command name since the stats start after it
        char *pointer = strrchr(stat_buffer, ')');
        if (pointer != NULL) {
            pointer += 2; // move to the 3rd field since 1st and 2nd are ignored
            char *token = strtok(pointer, " ");
            int field = 3;
            unsigned long utime_ticks = 0, stime_ticks = 0;
            long clk_tck = sysconf(_SC_CLK_TCK);
            // field 3(state), 10(minflt), 12(majflt), 14(utime), 15(stime), 23(vsize), 39(cpuid), else ignore
            while (token != NULL) {
                switch (field) {
                    case 3:  strcpy(process_information->state, token); break;
                    case 10: process_information->minflt = strtoul(token, NULL, 10); break;
                    case 12: process_information->majflt = strtoul(token, NULL, 10); break;
                    case 14: process_information->utime = (float)strtoul(token, NULL, 10) / clk_tck; break;
                    case 15: process_information->stime = (float)strtoul(token, NULL, 10) / clk_tck; break;
                    case 23: process_information->vsize = strtoul(token, NULL, 10); break;
                    case 39: process_information->cpuid = atoi(token); break;
                }
                token = strtok(NULL, " ");
                field++;
            }
        }
    }
    fclose(stat_file);
}

int main() {
    char buffer[1025];
    const char *delimiter = " ";
    while (1) {
        signal(SIGINT, sigint_handler); 
        printf("## 3230yash >> ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strstr(buffer, "||") != NULL) { // edge case of double pipeline
            printf("3230yash: should not have two consecutive | without in-between command\n");
            continue;
        }
        char *token = strtok(buffer, delimiter);
        if (token == NULL) {
            continue;
        } 
        // put all programs in an array
        char *command_queue[31] = {NULL};
        int command_index = 0;
        while (token != NULL) {
            command_queue[command_index] = token;
            command_index++;
            token = strtok(NULL, delimiter);
        }
        // check for incorrect pipe sequence in front and back
        if (strcmp(command_queue[0], "|") == 0) {
            printf("3230yash: Incorrect pipe sequence\n");
            continue;
        }
        if (strcmp(command_queue[command_index - 1], "|") == 0) {
            printf("3230yash: Incorrect pipe sequence\n");
            continue;
        }
        // check for incorrect pipe sequence in front and back even without space
        if (command_queue[0][0] == '|' || 
            command_queue[command_index - 1][strlen(command_queue[command_index - 1]) - 1] == '|') {
            printf("3230yash: Incorrect pipe sequence\n");
            continue;
        }
        // check for any invalid consecutive pipelines
        int no_consecutive_pipeline = 1; // flag
        for (int i = 0; command_queue[i] != NULL; i++) {
            // since we already checked for pipes in the start
            // we can assume that checking command_queue[i - 1] will not occur at i = 0
            if (strcmp(command_queue[i], "|") == 0) {
                if (strcmp(command_queue[i - 1], "|") == 0) {
                    printf("3230yash: should not have two consecutive | without in-between command\n");
                    no_consecutive_pipeline = 0;
                    break;
                }
            }
        }
        if (no_consecutive_pipeline != 1) {
            continue;
        }
        // handling exit
        if (strcmp(command_queue[0], "exit") == 0) {
            if (command_queue[1] == NULL) {
                printf("3230yash: Terminated\n");
                break;
            } else {
                printf("3230yash: \"exit\" with other arguments!!!\n");
                continue;
            }
        }
        // command seems valid
        // filter into execute_queue
        char *execute_list[5][31] = {NULL};
        int execute_index = 0;
        int argument_index = 0;
        for (int i = 0; command_queue[i] != NULL; i++) {
            if (strcmp(command_queue[i], "|") == 0) {
                execute_list[execute_index][argument_index] = NULL; // end last command arg with NULL
                execute_index++;
                argument_index = 0;
            } else {
                execute_list[execute_index][argument_index] = command_queue[i];
                argument_index++;
            }
        }
        execute_list[execute_index][argument_index] = NULL; // do last NULL because no pipeline to trigger it
        int num_of_commands = execute_index + 1;
        // handling watch
        if (strcmp(execute_list[0][0], "watch") == 0) {
            if (num_of_commands > 1) {
                printf("3230yash: Cannot watch a pipe sequence\n");
                continue;
            }
            if (execute_list[0][1] == NULL) {
                // handling watch alone is not really specified so just gonna skip since you watch nothing
                continue;
            }
            // need to remove watch from argument since its built in command and real program is after it
            for (int i = 0; execute_list[0][i] != NULL; i++) {
                execute_list[0][i] = execute_list[0][i+1];
            }
            int sync_pipe[2];
            pipe(sync_pipe);
            signal(SIGINT, SIG_IGN);
            struct ProcessInformation snapshots[1024];
            int snap_counter = 0;
            pid_t pid = fork();
            if (pid == 0) { // child
                close(sync_pipe[1]);
                char dummy;
                read(sync_pipe[0], &dummy, 1); // wait for parent initial snapshot
                close(sync_pipe[0]);
                signal(SIGINT, SIG_DFL);
                execvp(execute_list[0][0], execute_list[0]);
                // if we reach this line then execvp failed
                printf("3230yash: '%s': %s\n", execute_list[0][0], strerror(errno));
                exit(127); // kill failed child
            } else if (pid > 0) { // parent
                close(sync_pipe[0]);
                child_snapshot(pid, &snapshots[snap_counter]);
                snap_counter++;
                int child_status;
                // wake up child
                write(sync_pipe[1], "x", 1);
                close(sync_pipe[1]);
                // loop every 500ms until child terminates
                while (1) {
                    usleep(500000);
                    siginfo_t info;
                    memset(&info, 0, sizeof(info));
                    // leave child as zombie to read stats
                    waitid(P_PID, pid, &info, WEXITED | WNOHANG | WNOWAIT);
                    child_snapshot(pid, &snapshots[snap_counter]);
                    snap_counter++;
                    // if si_pid is set then child has terminated
                    if (info.si_pid == pid) {
                        waitpid(pid, &child_status, 0); // clean zombie
                        break;
                    }
                }
                // check if the watched process was killed by a signal
                if (WIFSIGNALED(child_status)) {
                    int sig = WTERMSIG(child_status);
                    char *program_name = strrchr(execute_list[0][0], '/');
                    if (program_name != NULL) {
                        program_name++;
                    } else {
                        program_name = execute_list[0][0];
                    }
                    if (sig == SIGINT) { // explicitly just in case
                        printf("%s: Interrupt\n", program_name);
                    } else if (sig == SIGKILL) {
                        printf("%s: Killed\n", program_name);
                    } else {
                        printf("%s: %s\n", program_name, strsignal(sig));
                    }
                }
                // print table
                // check for child error code 127 meaning execvp failed
                if (!(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 127)) {
                    printf("STATE\tCPUID\tUTIME\tSTIME\tVSIZE\tMINFLT\tMAJFLT\n");
                    for (int s = 0; s < snap_counter; s++) {
                        printf("%s\t%d\t%.2f\t%.2f\t%lu\t%lu\t%lu\n", 
                            snapshots[s].state, snapshots[s].cpuid, 
                            snapshots[s].utime, snapshots[s].stime, 
                            snapshots[s].vsize, snapshots[s].minflt, snapshots[s].majflt);
                    }
                }
                continue;
            }
        }
        int pfd[4][2]; // max 4 pipes
        pid_t child_pids[num_of_commands];
        for (int i = 0; i < num_of_commands - 1; i++) { // make pipes
            pipe(pfd[i]);
        }
        for (int i = 0; i < num_of_commands; i++) {
            signal(SIGINT, SIG_IGN); // don't refresh the terminal while child is still running
            pid_t pid = fork();
            if (pid < 0) { // fork fail
                // idk how to handle fork fail in this context
                perror("fork failed");
            } else if (pid == 0) { // child
                if (i > 0) { // check if first command or not (for pipeline)
                    dup2(pfd[i-1][0], STDIN_FILENO);
                }
                if (i < num_of_commands - 1) { // if not last command need to chain output to current pipe
                    dup2(pfd[i][1], STDOUT_FILENO);
                }
                for (int j = 0; j < num_of_commands - 1; j++) { // close file descriptors
                    close(pfd[j][0]);
                    close(pfd[j][1]);
                }
                signal(SIGINT, SIG_DFL); // restore SIGINT behavior for child
                // execute command
                execvp(execute_list[i][0], execute_list[i]);
                // if we reach this line then execvp failed
                printf("3230yash: '%s': %s\n", execute_list[i][0], strerror(errno));
                exit(1); // kill failed child
            } else { // parent
                child_pids[i] = pid;
            }
        }
        for (int i = 0; i < num_of_commands - 1; i++) { // close pipes for parents
            close(pfd[i][0]);
            close(pfd[i][1]);
        }
        for (int i = 0; i < num_of_commands; i++) {
            int status;
            waitpid(child_pids[i], &status, 0);
            if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                char *program_name = strrchr(execute_list[i][0], '/'); // find latest '/' to filter program name from path
                if (program_name != NULL) {
                    program_name++; // move pointer forward by one letter
                } else {
                    program_name = execute_list[i][0]; // no '/' path
                }
                if (sig == SIGINT) { // explicitly just in case
                    printf("%s: Interrupt\n", program_name);
                } else if (sig == SIGKILL) {
                    printf("%s: Killed\n", program_name);
                } else {
                    printf("%s: %s\n", program_name, strsignal(sig));
                }
            }
        }
    }
}