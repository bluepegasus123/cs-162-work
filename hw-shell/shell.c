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

// Executes a program segment delimited by start_index and end_index.
// This function assumes it is running within a child process.
// It resolves the executable path, handles < and > redirections, and calls execv.
void execProgramSegment(struct tokens* tokens, int start_index, int end_index) {
  int segment_length = end_index - start_index;
  if (segment_length == 0) {
    exit(0);
  }

  char* executable_path = tokens_get_token(tokens, start_index);
  char* resolved_path_pass_through = executable_path;
  bool needs_free = false;

  // PATH to executable is not provided explicitly, resolve via PATH
  if (access(executable_path, X_OK) == -1) {
    char* path = getenv("PATH");
    if (path) {
      char* copied_path = strdup(path);
      char* save_ptr;
      char* dir = strtok_r(copied_path, ":", &save_ptr);
      bool hasResolvedPath = false;
      
      while (dir != NULL) {
        int resolved_path_length = strlen(dir) + 1 + strlen(executable_path);
        char resolved_path[resolved_path_length + 1];
        sprintf(resolved_path, "%s/%s", dir, executable_path);
        
        if (access(resolved_path, X_OK) == 0) {
          hasResolvedPath = true;
          resolved_path_pass_through = strdup(resolved_path);
          needs_free = true;
          break;
        }
        dir = strtok_r(NULL, ":", &save_ptr);
      }
      free(copied_path);
      
      if (!hasResolvedPath) {
        printf("error: path could not be resolved for executable\n");
        exit(1);
      }
    } else {
      printf("error: path could not be resolved for executable\n");
      exit(1);
    }
  }

  // Parse arguments and standard I/O redirection
  char* argv[segment_length + 1];
  char *stdout_redirection = NULL;
  char *stdin_redirection = NULL;
  bool tracked_stdout_redirection_symbol = false;
  bool tracked_stdin_redirection_symbol = false;
  
  int arg_idx = 0;
  for (int i = start_index; i < end_index; i++) {
    char* current_arg = tokens_get_token(tokens, i);
    
    if (tracked_stdin_redirection_symbol) {
      stdin_redirection = current_arg;
      tracked_stdin_redirection_symbol = false;
    } else if (tracked_stdout_redirection_symbol) {
      stdout_redirection = current_arg;
      tracked_stdout_redirection_symbol = false;
    } else if (strcmp(current_arg, "<") == 0) {
      tracked_stdin_redirection_symbol = true;
    } else if (strcmp(current_arg, ">") == 0) {
      tracked_stdout_redirection_symbol = true;
    } else {
      if (arg_idx == 0) {
        argv[arg_idx++] = resolved_path_pass_through;
      } else {
        argv[arg_idx++] = current_arg;
      }
    }
  }
  argv[arg_idx] = NULL;

  // Apply I/O redirections
  if (stdin_redirection != NULL) {
    int new_stdin_redirect_fd = open(stdin_redirection, O_RDONLY);
    if (new_stdin_redirect_fd >= 0) {
      dup2(new_stdin_redirect_fd, 0);
      close(new_stdin_redirect_fd);
    }
  }

  if (stdout_redirection != NULL) {
    int new_stdout_redirect_fd = open(stdout_redirection, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (new_stdout_redirect_fd >= 0) {
      dup2(new_stdout_redirect_fd, 1);
      close(new_stdout_redirect_fd);
    }
  }

  // Execute child process
  execv(resolved_path_pass_through, argv);
  printf("executing program caused error\n");
  if (needs_free) free(resolved_path_pass_through);
  exit(1);
}

// Forks a single child process to run the command segment.
// This preserves existing functionality while isolating execution in execProgramSegment
int runProgram(unused struct tokens* tokens) {
  size_t num_tokens = tokens_get_length(tokens);
  
  pid_t pid = fork();
  if (pid == 0) {
    // Child process executes the entire token sequence
    execProgramSegment(tokens, 0, num_tokens);
  } else if (pid > 0) {
    // Parent process waits for child
    waitpid(pid, NULL, 0);
  } else {
    perror("fork error");
    return -1;
  }
  
  return 1;
}

pid_t execProgramSegmentWithPipes(struct tokens* tokens, int start_index, int end_index,
                                  int num_pipes, int pipes_array[][2], int process_index,
                                  bool dup_stdin, bool dup_stdout) {
  int segment_length = end_index - start_index;
  if (segment_length == 0) {
    return -1;
  }

  char* executable_path = tokens_get_token(tokens, start_index);
  char* resolved_path_pass_through = executable_path;
  bool needs_free = false;

  // PATH to executable is not provided explicitly, resolve via PATH
  if (access(executable_path, X_OK) == -1) {
    char* path = getenv("PATH");
    if (path) {
      char* copied_path = strdup(path);
      char* save_ptr;
      char* dir = strtok_r(copied_path, ":", &save_ptr);
      bool hasResolvedPath = false;
      
      while (dir != NULL) {
        int resolved_path_length = strlen(dir) + 1 + strlen(executable_path);
        char resolved_path[resolved_path_length + 1];
        sprintf(resolved_path, "%s/%s", dir, executable_path);
        
        if (access(resolved_path, X_OK) == 0) {
          hasResolvedPath = true;
          resolved_path_pass_through = strdup(resolved_path);
          needs_free = true;
          break;
        }
        dir = strtok_r(NULL, ":", &save_ptr);
      }
      free(copied_path);
      
      if (!hasResolvedPath) {
        printf("error: path could not be resolved for executable\n");
        return -1;
      }
    } else {
      printf("error: path could not be resolved for executable\n");
      return -1;
    }
  }

  // Parse arguments and standard I/O redirection
  char* argv[segment_length + 1];
  
  int arg_idx = 0;
  for (int i = start_index; i < end_index; i++) {
    char* current_arg = tokens_get_token(tokens, i);
    if (arg_idx == 0) {
        argv[arg_idx++] = resolved_path_pass_through;
    } else {
      argv[arg_idx++] = current_arg;
    }
  }
  argv[arg_idx] = NULL;

  pid_t pid = fork();
  if (pid == 0) {
    // Retrieve the necessary FDs to dupe
    if (dup_stdin) {
      dup2(pipes_array[process_index - 1][0], STDIN_FILENO);
    }
    if (dup_stdout) {
      dup2(pipes_array[process_index][1], STDOUT_FILENO);
    }

    // Close all pipes in child process
    for (int i = 0; i < num_pipes; i++) {
      close(pipes_array[i][0]);
      close(pipes_array[i][1]);
    }

    // Apply I/O redirections from tokens, since they override pipes
    // if (stdin_redirection != NULL) {
    //   int new_stdin_redirect_fd = open(stdin_redirection, O_RDONLY);
    //   if (new_stdin_redirect_fd >= 0) {
    //     dup2(new_stdin_redirect_fd, 0);
    //     close(new_stdin_redirect_fd);
    //   }
    // }

    // if (stdout_redirection != NULL) {
    //   int new_stdout_redirect_fd = open(stdout_redirection, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    //   if (new_stdout_redirect_fd >= 0) {
    //     dup2(new_stdout_redirect_fd, 1);
    //     close(new_stdout_redirect_fd);
    //   }
    // }

    // Execute child process
    execv(resolved_path_pass_through, argv);
    printf("executing program caused error\n");
    if (needs_free) free(resolved_path_pass_through);
    exit(1);
  } else if (pid > 0) {
    // Parent process codepath
    if (needs_free) free(resolved_path_pass_through);
    return pid;
  } else {
    perror("fork error");
    if (needs_free) free(resolved_path_pass_through);
    return -1;
  }
}

int runProgramWithPipes(struct tokens* tokens, int num_pipes) {
  int pipes_array[num_pipes][2];

  for (int i = 0; i < num_pipes; i++) {
    int result = pipe(pipes_array[i]);
    if (result == -1) {
      perror("pipe");
      return -1;
    }
  }

  size_t curr_index = 0;
  int current_process_start_index = 0;
  size_t num_tokens = tokens_get_length(tokens);
  
  int process_index = -1;
  pid_t pids[num_pipes + 1];

  while (curr_index <= num_tokens) {
    char* token = NULL;
    if (curr_index < num_tokens) {
      token = tokens_get_token(tokens, curr_index);
    }
    
    if (curr_index == num_tokens || strcmp("|", token) == 0) {
      process_index++;
      bool dup_stdin = false;
      bool dup_stdout = false;
      
      if (process_index == 0) {
        dup_stdout = true;
      } else if (process_index == num_pipes) {
        dup_stdin = true;
      } else {
        dup_stdin = true;
        dup_stdout = true;
      }

      pids[process_index] = execProgramSegmentWithPipes(
        tokens, current_process_start_index, curr_index,
        num_pipes, pipes_array, process_index,
        dup_stdin, dup_stdout
      );

      current_process_start_index = curr_index + 1;
    }
    curr_index++;
  }

  // Close all FDs in the parent process outer function
  for (int i = 0; i < num_pipes; i++) {
    close(pipes_array[i][0]);
    close(pipes_array[i][1]);
  }

  // Wait for all cloned child processes
  for (int i = 0; i <= num_pipes; i++) {
    if (pids[i] > 0) {
      waitpid(pids[i], NULL, 0);
    }
  }

  return 1;
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
      // check if and how many pipes there are
      // validate that there are n - 1 pipes for n processes we want to run
      // call separate helper that runs these processes and passes the relevant tokens into executeProgramSegment to only 
      // use the program name and its relevant args when calling execv
      size_t num_tokens = tokens_get_length(tokens);
      int num_pipes = 0;
      for (size_t i = 0; i < num_tokens; i++) {
        char* curr_token = tokens_get_token(tokens, i);
        if (strcmp("|", curr_token) == 0) {
          num_pipes++;
        }
      }
      if (num_pipes != 0) {
        // printf("Goes into pipes codepath\n");
        runProgramWithPipes(tokens, num_pipes);
      } else {
        // printf("Goes into non-pipes codepath\n");
        runProgram(tokens);
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
