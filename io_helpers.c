#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "io_helpers.h"


// ===== Output helpers =====

/* Prereq: str is a NULL terminated string
 */
void display_message(char *str) {
    write(STDOUT_FILENO, str, strnlen(str, MAX_STR_LEN));
}


/* Prereq: pre_str, str are NULL terminated string
 */
void display_error(char *pre_str, char *str) {
    write(STDERR_FILENO, pre_str, strnlen(pre_str, MAX_STR_LEN));
    write(STDERR_FILENO, str, strnlen(str, MAX_STR_LEN));
    write(STDERR_FILENO, "\n", 1);
}


// ===== Input tokenizing =====

/* Prereq: in_ptr points to a character buffer of size > MAX_STR_LEN
 * Return: number of bytes read
 */
ssize_t get_input(char *in_ptr) {
    int retval = read(STDIN_FILENO, in_ptr, MAX_STR_LEN+1); // Not a sanitizer issue since in_ptr is allocated as MAX_STR_LEN+1
    int read_len = retval;
    if (retval == -1) {
        read_len = 0;
    }
    if (read_len > MAX_STR_LEN) {
        read_len = 0;
        retval = -1;
        write(STDERR_FILENO, "ERROR: input line too long\n", strlen("ERROR: input line too long\n"));
        int junk = 0;
        while((junk = getchar()) != EOF && junk != '\n');
    }
    in_ptr[read_len] = '\0';
    return retval;
}

/* Prereq: in_ptr is a string, tokens is of size >= len(in_ptr)
 * Warning: in_ptr is modified
 * Return: number of tokens.
 */
size_t tokenize_input(char *in_ptr, char **tokens, Var_Node* vars) {
    
    char *curr_ptr = strtok (in_ptr, DELIMITERS);
    size_t token_count = 0;
    size_t total_len = 0;
    

    while (curr_ptr != NULL) {
        char *copy = strdup(curr_ptr);  
        char *expanded = expand_vars(curr_ptr, vars);
        size_t exp_len = strlen(expanded);

        // truncate last token before MAX_STR_LEN is hit
        if (total_len + exp_len > MAX_STR_LEN){
            size_t rem = MAX_STR_LEN - total_len;
            if (rem > 0) {
                expanded[rem] = '\0';
                tokens[token_count] = expanded;
                total_len += rem;
                token_count++;
            }
            free(copy);
            break;
        }

        tokens[token_count] = expanded;
        total_len += exp_len;
        token_count++;
        curr_ptr = strtok(NULL, DELIMITERS);
        free(copy);
    }
    tokens[token_count] = NULL;
    return token_count;
}
