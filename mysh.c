#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"

BackgroundJob background_jobs[MAX_JOBS];
int job_count = 0;
int running_jobs = 0;

void free_token_arr(char **tokens, size_t token_count){
    for (size_t i = 0;i<token_count;i++){
            free(tokens[i]);
        }
}

void concatenate_tokens(char **tokens, char *res){
    res[0] = '\0';

    for (int i = 0; tokens[i] != NULL; i++) {
        strncat(res, tokens[i], MAX_CMD_LEN - strlen(res) - 1);

        if (tokens[i + 1] != NULL) {
            strncat(res, " ", MAX_CMD_LEN - strlen(res) - 1);
        }
    }
}

void execute_command(char **tokens, int is_background_task, size_t token_count, Var_Node *variables_ll){
    // just in case
    if (token_count == 0){
        display_error("ERROR: Builtin failed: ", "");
        return;
    } 

    // builtin commands
    bn_ptr builtin_fn = check_builtin(tokens[0]);
    if (builtin_fn != NULL) {
        if (!is_background_task){ //foreground builtin
            ssize_t err = builtin_fn(tokens);
            if (err == - 1) {
                display_error("ERROR: Builtin failed: ", tokens[0]);
            }
        } else { //background builtin
            int pid = fork();
            if (pid == 0){ //child
                ssize_t err = builtin_fn(tokens);
                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", tokens[0]);
                }
                free_token_arr(tokens, token_count);
                free_vars(variables_ll);
                exit(0);
        
            } else if (pid > 0){ //parent
                if (job_count < MAX_JOBS) {
                    background_jobs[job_count].pid = pid;
                    concatenate_tokens(tokens, background_jobs[job_count].command);
                    background_jobs[job_count].job_id = job_count + 1;
                    char message[MAX_STR_LEN];
                    snprintf(message, MAX_STR_LEN, "[%d] %d\n", background_jobs[job_count].job_id, pid);
                    display_message(message);
                    job_count++;
                    running_jobs++;
                } else {
                    display_error("ERROR: Too many background jobs", "");
                }
            } else {
                display_error("ERROR: Fork failed", "");
            }
        }
        return;  
    }


    // bin commands
    int pid = fork();
    if (pid == 0){ //child

        execvp(tokens[0], tokens);

        display_error("ERROR: Unknown command: ", tokens[0]);
        free_token_arr(tokens, token_count);
        free_vars(variables_ll);
        exit(1);

    } else if (pid > 0){ //parent
        if (!is_background_task){ // foreground
            int status;
            waitpid(pid, &status, 0);
        } else {    // background
            if (job_count < MAX_JOBS) {
                background_jobs[job_count].pid = pid;
                concatenate_tokens(tokens, background_jobs[job_count].command);
                background_jobs[job_count].job_id = job_count + 1;
                char message[MAX_STR_LEN];
                snprintf(message, MAX_STR_LEN, "[%d] %d\n", background_jobs[job_count].job_id, pid);
                display_message(message);
                job_count++;
                running_jobs++;
            } else {
                display_error("ERROR: Too many background jobs", "");
            }
        }   
    } else {
        display_error("ERROR: Fork failed", "");
    }
}

int is_background_command(char **tokens, size_t *token_count){
    if (*token_count == 0){
        return 0;
    }

    if (strcmp(tokens[(*token_count) - 1], "&") == 0){
        free(tokens[(*token_count) - 1]);
        tokens[(*token_count) - 1] = NULL;
        (*token_count)--;
        return 1;
    }

    return 0;
}

void handler_sigint(__attribute__((unused)) int code){
    display_message("\n");
}

void handler_sigchld(__attribute__((unused)) int code){
    int status;
    int pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < job_count; i++) {
            if (background_jobs[i].pid == pid) {
                char message[MAX_STR_LEN];
                snprintf(message, MAX_STR_LEN, "[%d]+ Done %s\n", background_jobs[i].job_id, background_jobs[i].command);
                display_message("\n");
                display_message(message);
                running_jobs--;
                break;
            }
        }
    }

    if (running_jobs == 0){
        job_count = 0;
    }
}

void set_sigactions(){
    struct sigaction sa_int;
    sa_int.sa_handler = handler_sigint;
    sa_int.sa_flags = 0;
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_chld;
    sa_chld.sa_handler = handler_sigchld;
    sa_chld.sa_flags = 0;
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);
}


typedef struct {
    char **tokens;
    size_t token_count;
} Command;


void execute_pipeline(Command *commands, size_t num_commands, Var_Node *variables_ll) {
    int pipes[2];
    int prev_pipe_read = -1;
    int pids[num_commands];

    for (size_t i = 0; i < num_commands; i++) {
        // Create new pipe if not last command
        if (i < num_commands - 1) {
            if (pipe(pipes) < 0) {
                display_error("ERROR: pipe() failed", "");
                return;
            }
        }

        pids[i] = fork();
        if (pids[i] == 0) { // CHILD
            // in from prev child
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }

            // write to next command, only if not last command
            if (i < num_commands - 1) {
                close(pipes[0]);
                dup2(pipes[1], STDOUT_FILENO);
                close(pipes[1]);
            }

            // command exec
            execute_command(commands[i].tokens, 0, commands[i].token_count, variables_ll);
            exit(0);
            
        } else if (pids[i] > 0) { //PARENT
            // close previous pipe read end (not needed in parent)
            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            // save read end for next command
            if (i < num_commands - 1) {
                prev_pipe_read = pipes[0];
                close(pipes[1]); // close write end 
            }
        } else {
            display_error("ERROR: fork() failed", "");
            return;
        }
    }

    // wait for children
    for (size_t i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}


int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {

    set_sigactions();
    char *prompt = "mysh$ ";

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};

    Var_Node *variables_ll = NULL;
        
    while (1) {
        display_message(prompt);

        int ret = get_input(input_buf);
        if (strcmp(input_buf, "\n") == 0) {
            continue;
        }

        // input with pipes
        if (strchr(input_buf, '|') != NULL) {
            char input_copy[MAX_STR_LEN];
            strncpy(input_copy, input_buf, MAX_STR_LEN);

            Command commands[MAX_STR_LEN] = {0};
            size_t num_commands = 0;
            
            // count commands
            char *count_ptr = input_copy;
            while (*count_ptr && num_commands < MAX_STR_LEN) {
                if (*count_ptr == '|') num_commands++;
                count_ptr++;
            }
            num_commands++;
            
            char *saveptr;
            char *cmd_str = strtok_r(input_buf, "|", &saveptr);
            for (size_t i = 0; cmd_str && i < num_commands; i++) {
                while (*cmd_str == ' ') cmd_str++; // trim whitespace
                char *end = cmd_str + strlen(cmd_str) - 1;
                while (end > cmd_str && *end == ' ') end--;
                *(end+1) = '\0';
                
                commands[i].tokens = malloc(MAX_STR_LEN * sizeof(char*));
                commands[i].token_count = tokenize_input(cmd_str, commands[i].tokens, variables_ll);
                
                cmd_str = strtok_r(NULL, "|", &saveptr);
            }

            execute_pipeline(commands, num_commands, variables_ll);
        
            for (size_t i = 0; i < num_commands; i++) {
                free_token_arr(commands[i].tokens, commands[i].token_count);
                free(commands[i].tokens);
            }
            continue;
        }

        size_t token_count = tokenize_input(input_buf, token_arr, variables_ll);

        // Clean exit
        if (ret != -1 && (token_count == 0 || (strncmp("exit", token_arr[0], 5) == 0))) {
            if (token_count == 0){
                display_message("\n");  // for ctrl + d
            }
            free_token_arr(token_arr, token_count);
            break;
        }

        int background_task = is_background_command(token_arr, &token_count);

        //check for var creation
        Var_Node *var = make_var_node(token_arr[0], token_count);
        if (var!=NULL){
            if (variables_ll == NULL){
                variables_ll = var;
            } else{
                var->next = variables_ll;
                variables_ll = var;
            }
            free_token_arr(token_arr, token_count);
            continue;
        }

        // Command execution
        if (token_count >= 1) {
            execute_command(token_arr, background_task, token_count, variables_ll);
        }

        free_token_arr(token_arr, token_count);
    }
    
    free_vars(variables_ll);
    return 0;
}


