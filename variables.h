#ifndef __VARIABLES_H__
#define __VARIABLES_H__

#include <sys/types.h>
#include <stdbool.h>


#define MAX_STR_LEN 128
#define DELIMITERS " \t\n"     // Assumption: all input tokens are whitespace delimited

typedef struct node{
    char *name;
    char *data;
    struct node *next;
} Var_Node;

Var_Node *make_var_node(char *str, size_t token_count);
void free_vars(Var_Node *front);
char *expand_vars(char *input_buf, Var_Node *vars);
char *find_var(char *var_name, Var_Node *vars);

#endif
