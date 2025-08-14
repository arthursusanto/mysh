#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "builtins.h"
#include "io_helpers.h"


// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}

// ===== Builtin Helpers =====

ssize_t list_dir(char *path, char *substr, int recursive, int depth, int curr_depth){
    if (depth == 0){
        display_message(".\n");
        return 0;
    }
    
    if(depth != -1 && curr_depth >= depth){
        return 0;
    }

    DIR *dir = opendir(path);
    if(dir == NULL){
        display_error("ERROR: Invalid path: ", path);
        return -1;
    }

    struct dirent *item;
    while ((item = readdir(dir)) != NULL){

        if(substr == NULL || strstr(item->d_name, substr) != NULL){
            display_message(item->d_name);
            display_message("\n");
        }
        
        if(strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0){
            continue;
        }

        if (recursive && (depth == -1 || curr_depth + 1 < depth)){
            struct stat statbuf;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, item->d_name);

            if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)){
                list_dir(full_path, substr, recursive, depth, curr_depth + 1);
            }
        }
    }
    closedir(dir);
    return 0;
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;

    if (tokens[index] != NULL) {
        display_message(tokens[index]);
        index++;
    }

    while (tokens[index] != NULL) {
        display_message(" ");
        display_message(tokens[index]);
        index += 1;
    }
    display_message("\n");

    return 0;
}

ssize_t bn_ls(char **tokens){

    char *path = "./"; //default is curr dir
    char *substr = NULL;
    int rec = 0; //bool
    int depth = -1; //inf

    //check flags
    for (int i = 1; tokens[i] != NULL; i++){
        if (strcmp(tokens[i], "--f") == 0 && tokens[i+1] != NULL){
            i++;
            substr = tokens[i];
        } else if (strcmp(tokens[i], "--rec") == 0){
            rec = 1;
        } else if (strcmp(tokens[i], "--d") == 0 && tokens[i+1] != NULL){
            i++;
            depth = atoi(tokens[i]);

            if (depth<0){
                display_error("ERROR: Invalid depth: ", tokens[i]);
                return -1;
            }
        } else if(strcspn(tokens[i], "-") != 0){
            if (strcmp(path, "./") != 0){
                display_error("ERROR: More than one path specified: ", "");
                return -1;
            }
            path = tokens[i];
        } else {
            display_error("ERROR: Invalid argument: ", tokens[i]);
            return -1;
        }
    }

    if (depth != -1 && !rec){
        display_error("ERROR: --d provided without --rec", "");
        return -1;
    }

    return list_dir(path, substr, rec, depth, 0);
}


ssize_t bn_cd(char **tokens){
    if(tokens[1] == NULL){
        display_error("ERROR: No path provided", "");
        return -1;
    } else if(tokens[2] != NULL){
        display_error("ERROR: Too many arguments: cd takes a single path", "");
        return -1;
    }

    char *path = tokens[1];
    if(strcmp(path, "...") == 0){
        path = "../..";
    } else if(strcmp(path, "....") == 0){
        path = "../../..";
    }

    if(chdir(path) != 0){
        display_error("ERROR: Invalid path: ", path);
        return -1;
    }
    return 0;
}


ssize_t bn_cat(char **tokens){
    FILE *input = stdin;
    int close = 0;

    if(tokens[1] != NULL && tokens[2] != NULL){
        display_error("ERROR: Too many arguments: cat takes a single file", "");
        return -1;
    }
    
    if(tokens[1] != NULL){
        input = fopen(tokens[1], "r");
        if (input == NULL){
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
        close = 1;
    } else if (isatty(STDIN_FILENO)){
        display_error("ERROR: No input source provided", "");
        return -1;
    }
    
    char buf[MAX_STR_LEN];
    while(fgets(buf, MAX_STR_LEN, input) != NULL){
        display_message(buf);
    }

    if (close){
        fclose(input);
    }
    return 0;
}


ssize_t bn_wc(char **tokens){
    FILE *input = stdin;
    int close = 0;

    if(tokens[1] != NULL && tokens[2] != NULL){
        display_error("ERROR: Too many arguments: wc takes a single file", "");
        return -1;
    }
    
    if(tokens[1] != NULL){
        input = fopen(tokens[1], "r");
        if (input == NULL){
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
        close = 1;
    } else if (isatty(STDIN_FILENO)){
        display_error("ERROR: No input source provided", "");
        return -1;
    }

    int word_count = 0;
    int char_count = 0;
    int newline_count = 0;
    int during_word = 0;

    char c;
    while((c=fgetc(input)) != EOF){
        char_count++;

        if(c==' ' || c=='\n'){
            if(c=='\n'){
                newline_count++;
            }
            during_word = 0;
        } else if(during_word == 0){
            during_word = 1;
            word_count++;
        }
    }

    char buf[MAX_STR_LEN];
    snprintf(buf, sizeof(buf), "word count %d\n", word_count);
    display_message(buf);

    snprintf(buf, sizeof(buf), "character count %d\n", char_count);
    display_message(buf);

    snprintf(buf, sizeof(buf), "newline count %d\n", newline_count);
    display_message(buf);

    if (close){
        fclose(input);
    }
    return 0;
}


ssize_t bn_kill(char **tokens){
    if (tokens[1] == NULL) {
        display_error("ERROR: Usage: kill [pid] [signum]", "");
        return -1;
    }

    pid_t pid = atoi(tokens[1]);
    int signum = SIGTERM;

    if (tokens[2] != NULL) {
        signum = atoi(tokens[2]);

        if (signum <= 0 || signum >= NSIG) {
            display_error("ERROR: Invalid signal specified", "");
            return -1;
        }
    }

    if (kill(pid, 0) == -1) { // check if process exists
        if (errno == ESRCH) {
            display_error("ERROR: The process does not exist", "");
        } else {
            display_error("ERROR: Cannot send signal to process", "");
        }
        return -1;
    }

    if (kill(pid, signum) == -1) {
        display_error("ERROR: Cannot send signal to process", "");
        return -1;
    }

    return 0;
}


ssize_t bn_ps(__attribute__((unused)) char **tokens){
    char message[MAX_STR_LEN];
    for (int i = 0; i < job_count; i++){
        char *space_ptr = strchr(background_jobs[i].command, ' ');
        int length = space_ptr - background_jobs[i].command;

        snprintf(message, MAX_STR_LEN, "%.*s %d\n", length, background_jobs[i].command, background_jobs[i].pid);
        display_message(message);
    }
    return 0;
}


ServerState server_state = {0}; //set all fields to 0


void handle_server_activity(fd_set *readfds) {
    char buffer[BUFFER_SIZE];
    Client *prev = NULL;
    Client *curr = server_state.clients;
    
    while (curr != NULL) {
        Client *next = curr->next; // need next in case of removal
        
        if (FD_ISSET(curr->socket, readfds)) {
            int valread = read(curr->socket, buffer, BUFFER_SIZE - 1);
            
            if (valread <= 0) { // disconnected
                close(curr->socket);
                char msg[128];
                snprintf(msg, sizeof(msg), "client%d disconnected\n", curr->id);
                display_message(msg);
                
                if (prev) {
                    prev->next = next;
                } else {
                    server_state.clients = next;
                }
                free(curr);
                server_state.client_count--;

            } else {
                buffer[valread] = '\0';
                
                if (strcmp(buffer, "\\connected") == 0) {
                    char msg[MAX_STR_LEN];
                    snprintf(msg, sizeof(msg), "client%d: %d clients connected\n", curr->id, server_state.client_count);
                    write(curr->socket, msg, strlen(msg));
                } else {
                    // write to all clients
                    char msg[BUFFER_SIZE + 128];
                    snprintf(msg, sizeof(msg), "client%d: %s", curr->id, buffer);
                    
                    Client *receiver = server_state.clients;
                    while (receiver) {
                        if (receiver != curr) { // except sender
                            write(receiver->socket, msg, strlen(msg));
                        }
                        receiver = receiver->next;
                    }
                    display_message(msg);
                }
            }
        } else {
            prev = curr; // advance prev if no deletion
        }
        curr = next;
    }
}


void server_loop() {
    fd_set readfds;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    server_state.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(server_state.port);
    
    bind(server_state.server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_state.server_fd, 10);
    
    while (server_state.running) {
        FD_ZERO(&readfds);
        FD_SET(server_state.server_fd, &readfds);
        int max_sd = server_state.server_fd;
        
        // fdset all clients
        Client *curr = server_state.clients;
        while (curr) {
            FD_SET(curr->socket, &readfds);
            if (curr->socket > max_sd) max_sd = curr->socket;
            curr = curr->next;
        }
        
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            display_error("ERROR: select failed", "");
            continue;
        }
        
        // connection
        if (FD_ISSET(server_state.server_fd, &readfds)) {
            int new_socket = accept(server_state.server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
            if (new_socket < 0) {
                display_error("ERROR: accept failed", "");
                continue;
            }
            
            Client *new_client = malloc(sizeof(Client));
            new_client->socket = new_socket;
            new_client->id = ++server_state.client_count;
            new_client->next = server_state.clients;
            server_state.clients = new_client;
            
            char welcome[MAX_STR_LEN];
            snprintf(welcome, sizeof(welcome), "client%d: connected\n", new_client->id);
            write(new_socket, welcome, strlen(welcome));
            display_message(welcome);
        }

        handle_server_activity(&readfds);
    }
    
    // server cleanup after close
    Client *curr = server_state.clients;
    while (curr) {
        Client *next = curr->next;
        close(curr->socket);
        free(curr);
        curr = next;
    }
    close(server_state.server_fd);
}


ssize_t bn_start_server(char **tokens){
    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    }
    
    server_state.port = atoi(tokens[1]);
    server_state.running = 1;
    server_state.client_count = 0;
    server_state.clients = NULL;
    
    pid_t pid = fork();
    if (pid == 0) {
        server_loop();
        exit(0);
    } 
    else if (pid > 0) {
        server_state.server_pid = pid;
        char msg[128];
        snprintf(msg, sizeof(msg), "Server started on port %d\n", server_state.port);
        display_message(msg);
        return 0;
    } 
    else {
        display_error("ERROR: fork failed", "");
        return -1;
    }
}


ssize_t bn_close_server(char **tokens) {
    (void)tokens;
    if (!server_state.running) {
        display_error("ERROR: No server running", "");
        return -1;
    }
    
    server_state.running = 0;
    kill(server_state.server_pid, SIGTERM);
    
    int status;
    waitpid(server_state.server_pid, &status, 0);
    
    display_message("Server stopped\n");
    return 0;
}


ssize_t bn_send(char **tokens) {
    if (!tokens[1] || !tokens[2] || !tokens[3]) {
        display_error("ERROR: Need port, host and message", "");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        display_error("ERROR: Socket creation failed", "");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(tokens[1]));
    serv_addr.sin_addr.s_addr = inet_addr(tokens[2]);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        display_error("ERROR: Connection failed", "");
        close(sock);
        return -1;
    }

    char *message = tokens[3];
    write(sock, message, strlen(message));
    close(sock);
    return 0;
}


ssize_t bn_start_client(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    } else if (tokens[2] == NULL){
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        display_error("ERROR: Socket creation failed", "");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(tokens[1]));
    serv_addr.sin_addr.s_addr = inet_addr(tokens[2]);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        display_error("ERROR: Connection failed", "");
        close(sock);
        return -1;
    }

    char welcome[128];
    if (read(sock, welcome, sizeof(welcome)) <= 0) {
        display_error("ERROR: Server disconnected", "");
        close(sock);
        return -1;
    }
    display_message(welcome);

    fd_set readfds;
    char buffer[BUFFER_SIZE];
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);  // stdin
        FD_SET(sock, &readfds);          // server socket

        if (select(sock + 1, &readfds, NULL, NULL, NULL) < 0) {
            display_error("ERROR: select failed", "");
            break;
        }

        // check stdin
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (!fgets(buffer, BUFFER_SIZE, stdin)) break;  // EOF (Ctrl+D)
            
            if (strcmp(buffer, "\\connected\n") == 0) {
                write(sock, "\\connected", 10);
            } else {
                write(sock, buffer, strlen(buffer));
            }
        }

        // check server messages
        if (FD_ISSET(sock, &readfds)) {
            int bytes = read(sock, buffer, BUFFER_SIZE - 1);
            if (bytes <= 0) {
                display_error("ERROR: Server disconnected", "");
                break;
            }
            buffer[bytes] = '\0';
            display_message(buffer);
        }
    }

    close(sock);
    return 0;
}