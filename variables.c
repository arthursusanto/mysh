#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "variables.h"
#include "io_helpers.h"


Var_Node *make_var_node(char *str, size_t token_count){
    if(token_count != 1){
        return NULL;
    }

    char var_name[128];
    char var_data[128];
    char *curr = strchr(str, '=');

    if (curr){
        int var_name_len = curr - str;
        strncpy(var_name, str, var_name_len);
        var_name[var_name_len] = '\0';

        strcpy(var_data, curr + 1);
    } else {
        return NULL;
    }

    Var_Node *var = malloc(sizeof(Var_Node));
    var->name = malloc(strlen(var_name) + 1);
    var->data = malloc(strlen(var_data) + 1);
    var->next = NULL;

    strcpy(var->name, var_name);
    strcpy(var->data, var_data);
    return var;
}


void free_vars(Var_Node *front){
    Var_Node *next;
    while (front!=NULL){
        next = front->next;
        free(front->name);
        free(front->data);
        free(front);
        front = next;
    }
}


char *expand_vars(char *input_buf, Var_Node *vars){
    if (!input_buf){
        return NULL;
    }
    
    char ret_buf[MAX_STR_LEN+1];
    ret_buf[128] = '\0';
    size_t remaining = MAX_STR_LEN;

    char *curr = input_buf;
    while (*curr && remaining > 0){
        if (*curr == '$'){
            char *end = curr + 1;

            // check for treating $ as a character
            if((*end == ' ') || (*end == '\n') || (*end == '\0')){ 
                strcat(ret_buf, "$");
                curr++;
                continue;
            }

            // find end of var name to expand
            while((*end != '\n') && (*end != ' ') && (*end != '$') && (*end != '\0')){
                end++;
            }

            int var_len = end - curr - 1;
            char var_name[var_len + 1];
            strncpy(var_name, curr + 1, var_len);
            var_name[var_len] = '\0';
            
            char *expanded = find_var(var_name, vars);
            
            // check for max length expansion
            if (remaining < strlen(expanded)){
                strncat(ret_buf, expanded, remaining);
                break;
            } else {
                strcat(ret_buf, expanded);
                remaining -= strlen(expanded);
            }

            curr = end;

        } else {
            strncat(ret_buf, curr, 1);
            curr++;
            remaining--;
        }
    }
    char *ptr = strdup(ret_buf);
    return ptr;
}


char *find_var(char *var_name, Var_Node *vars){
    Var_Node *curr = vars;
    while(curr != NULL){
        if (strcmp(curr->name, var_name) == 0){
            return curr->data;
        }
        curr = curr->next;
    }
    return "";
}

