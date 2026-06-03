#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory"},
    {cmd_cd, "cd", "change to a target directory"}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

// Prints the current working directory to stdout
int cmd_pwd(unused struct tokens* tokens) {
  char buf[4096];
  // printf("%p\n", buf);
  char* return_address = getcwd(buf, sizeof(buf));
  if (return_address == NULL) {
    printf("Error while trying to get current working directory\n");
    return -1;
  }
  printf("%s\n", buf);
  return 1;
}

int cmd_cd(unused struct tokens* tokens) {
  // retrieve argument from tokens
  char* new_directory = tokens_get_token(tokens, 1);
  if (chdir(new_directory) == -1) {
    printf("Error while trying to change to target directory: %s\n", new_directory);
    return -1;
  } else {
    return 1;
  }
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

// Forks a new child process and runs execv in it to run tokens[0] as executable path and tokens[1-N] as arguments to that executable
int runProgram(unused struct tokens* tokens) {
  // create an array of tokens_size strings and iterate and store each one
  // include an element for the NULL pointer as well
  size_t num_tokens = tokens_get_length(tokens);
  // Here - check if the path is in CWD
  // YES - continue 
  // NO - use path resolution to match potential directory with this executable and if a match is found - pass that resolved path to execv
  char* executable_path = tokens_get_token(tokens, 0);
  // PATH to executable is not provided and we must resolve it via PATH
  if (access(executable_path, X_OK) == -1) {
    char* path = getenv("PATH");
    char* save_ptr;
    char* copied_path = strdup(path);
    char* dir = strtok_r(copied_path, ":", &save_ptr);
    bool hasResolvedPath = false;
    char* resolved_path_pass_through;
    
    while (dir != NULL) {
      // join this dir with the executable name - need to create a char array with dir + "/" + executable name
      int resolved_path_length = (int) (strlen(dir) + 1 + strlen(executable_path));
      // + 1 is for the /
      int executablePathPointer = 0;
      char resolved_path[resolved_path_length + 1];
      for (int i = 0; i < resolved_path_length; i++) {
        if (i == strlen(dir)) {
          resolved_path[i] = '/';
        } else if (i >=0 && i< strlen(dir)){
          resolved_path[i] = dir[i];
        } else {
          resolved_path[i] = executable_path[executablePathPointer];
          executablePathPointer++;
        }
      }
      resolved_path[resolved_path_length] = '\0';
      // if this resolved_path works, break the loop
      // if we haven't found any resolved path, print an error mentioning that there is no resolvable path to executable
      if (access(resolved_path, X_OK) == 0) {
        hasResolvedPath = true;
        resolved_path_pass_through = strdup(resolved_path);
        break;
      }
      dir = strtok_r(NULL, ":", &save_ptr);
    }
    free(copied_path);
    if (!hasResolvedPath) {
      printf("error: path could not be resolved for executable\n");
      return -1;
    }
    // printf("[after strtok_r]: resolved path: %s\n", resolved_path_pass_through);
    char* argv[num_tokens+1];
    char *stdout_redirection = NULL;
    char *stdin_redirection = NULL;
    bool tracked_stdout_redirection_symbol = false;
    bool tracked_stdin_redirection_symbol = false;
    for (int i = 0; i < num_tokens; i++) {
      if (i == 0) {
        argv[i] = resolved_path_pass_through;
      } else {
        char* current_arg = tokens_get_token(tokens, i);
        if (tracked_stdin_redirection_symbol) {
          // printf("tracked stdin branch %s\n", current_arg);
          stdin_redirection = current_arg;
          tracked_stdin_redirection_symbol = false;
          // array contents: ./words [NULL]
        }
        else if (tracked_stdout_redirection_symbol) {
          stdout_redirection = current_arg;
          tracked_stdout_redirection_symbol = false;
        }
        else if (strcmp(current_arg, "<") == 0) {
          tracked_stdin_redirection_symbol = true;
          argv[i] = NULL;
        } else if (strcmp(current_arg, ">")== 0) {
          printf("Goes into > detected branch\n");
          tracked_stdout_redirection_symbol = true;
          argv[i] = NULL;
        } else {
           // printf("current arg: %s\n", current_arg);
          argv[i] = current_arg;
        }
     
      }
    }
    argv[num_tokens] = NULL;
    for (int i = 0; argv[i] != NULL; i++) {
        // printf("within argv to child p: %s\n", argv[i]);
      }
    pid_t pid = fork();
    if (pid == 0) {
      if (stdin_redirection != NULL) {
        int new_stdin_redirect_fd = open(stdin_redirection, O_RDONLY);
        dup2(new_stdin_redirect_fd, 0);
        close(new_stdin_redirect_fd);
      }

      if (stdout_redirection != NULL) {
        int new_stdout_redirect_fd = open(stdout_redirection,O_WRONLY | O_CREAT, 0644);
        dup2(new_stdout_redirect_fd, 1);
        close(new_stdout_redirect_fd);
      }
      
      // child process execution
      execv(resolved_path_pass_through, argv);
      printf("executing program caused error\n");
      exit(1);
    } else {
      // parent process execution
      waitpid(pid, NULL, 0);
      free(resolved_path_pass_through);
    }
    return 1;


  } else {
    // PATH TO executable is provided
    // Parse for >, < characters
    // If <, set new child process's stdin FD to point to this file path
    // Same with >
    char* argv[num_tokens+1];
    // track stdin redirection path
    // track stdout redirection path
    char *stdout_redirection = NULL;
    char *stdin_redirection = NULL;
    bool tracked_stdout_redirection_symbol = false;
    bool tracked_stdin_redirection_symbol = false;
    for (int i = 0; i < num_tokens; i++) {
      char* current_arg = tokens_get_token(tokens, i);
      if (tracked_stdin_redirection_symbol) {
        // printf("tracked stdin branch %s\n", current_arg);
        stdin_redirection = current_arg;
        tracked_stdin_redirection_symbol = false;
        // array contents: ./words [NULL]
      }
      else if (tracked_stdout_redirection_symbol) {
        stdout_redirection = current_arg;
        tracked_stdout_redirection_symbol = false;
        // argv[i] = NULL;
      }
      else if (strcmp(current_arg, "<") == 0) {
        // printf("found < branch %s\n", current_arg);
        tracked_stdin_redirection_symbol = true;
        argv[i] = NULL;
      }
      else if (strcmp(current_arg, ">")== 0) {
        tracked_stdout_redirection_symbol = true;
        argv[i] = NULL;
      } else {
        argv[i] = current_arg;
      }


      
    }
    argv[num_tokens] = NULL;
    // printf("stdin_redirection %s\n", stdin_redirection);
    // set the respective STDIN and STDOUT for this new process
    pid_t pid = fork();
    if (pid == 0) {
      if (stdin_redirection != NULL) {
        int new_stdin_redirect_fd = open(stdin_redirection, O_RDONLY);
        dup2(new_stdin_redirect_fd, 0);
        close(new_stdin_redirect_fd);
      }

      if (stdout_redirection != NULL) {
        int new_stdout_redirect_fd = open(stdout_redirection,O_WRONLY | O_CREAT, 0644);
        dup2(new_stdout_redirect_fd, 1);
      }
      // child process execution
      execv(executable_path, argv);
      printf("executing program caused error\n");
      exit(1);
    } else {
      // parent process execution
      waitpid(pid, NULL, 0);
    }
    return 1;
  }
  
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      runProgram(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
