/* Joshua Chen
 * 118004297
 * chenj8
 */
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "command.h"
#include "executor.h"

#define OPEN_FLAGS (O_WRONLY | O_TRUNC | O_CREAT)
#define DEF_MODE 0664

/*static void print_tree(struct tree *t);*/
static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd);

int execute(struct tree *t) {
   /*print_tree(t); */
   if (t != NULL) {             /*call auxiliary method if tree is not null */
      return execute_aux(t, STDIN_FILENO, STDOUT_FILENO);
   }
   return 0;
}

static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd) {
   pid_t pid_1, pid_2, pid_3;
   int status, file_in = 0, file_out = 0, pipe_fd[2];

   if (t->input != NULL) {      /*check if there is input redirection */
      if ((file_in = open(t->input, O_RDONLY)) == -1) {
         perror("File open failed");
         exit(EX_OSERR);
      }
   } else {
      file_in = p_input_fd;     /* if no redirection use parameter */
   }

   if (t->output != NULL) {     /*check if there is output redirection */
      if ((file_out = open(t->output, OPEN_FLAGS, DEF_MODE)) == -1) {
         perror("File open failed");
         exit(EX_OSERR);
      }
   } else {
      file_out = p_output_fd;
   }

   if (t->conjunction == NONE) {        /*NONE node/leaf node */
      if (!strcmp(t->argv[0], "cd")) {  /*check if argument is change directory */
         if (t->argv[1] == NULL) {
            if (chdir(getenv("HOME")) == -1) {  /*if no argument, then means change to home directory */
               perror(t->argv[-1]);
            }
         } else if (chdir(t->argv[1]) == -1) {
            perror(t->argv[1]);
         }
      } else if (!strcmp(t->argv[0], "exit")) { /*exit */
         exit(0);
      } else {
         pid_1 = fork();
         if (pid_1 > 0) {
            wait(&status);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
               return 0;
            } else {
               return 1;
            }
         } else if (pid_1 == 0) {
            if (file_in != STDIN_FILENO) {      /*redirect standard input to file */
               if (dup2(file_in, STDIN_FILENO) == -1) {
                  perror("dup2 failed");
                  exit(EX_OSERR);
               }
               if (close(file_in) == -1) {
                  perror("closer error");
                  exit(EX_OSERR);
               }
            }

            if (file_out != STDOUT_FILENO) {    /*redirect standard output to file */
               if (dup2(file_out, STDOUT_FILENO) == -1) {
                  perror("dup2 failed");
                  exit(EX_OSERR);
               }
               if (close(file_out) == -1) {
                  perror("close error");
                  exit(EX_OSERR);
               }
            }

            execvp(t->argv[0], t->argv);        /*call program */
            fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
            fflush(stdout);
            exit(EX_OSERR);
         } else if (pid_1 < 0) {
            perror("fork");
         }
      }
   } else if (t->conjunction == AND) {  /*AND node */
      if (execute_aux(t->left, file_in, file_out) == 0) {       /*if left tree processing fails, don't process right tree */
         return execute_aux(t->right, file_in, file_out);
      }
      return 1;
   } else if (t->conjunction == PIPE) { /*PIPE node */
      if (t->left->output != NULL) {    /*checks for ambigous output/input */
         printf("Ambiguous output redirect.\n");
         fflush(stdout);
         return 1;
      } else if (t->right->input != NULL) {
         printf("Ambiguous input redirect.\n");
         fflush(stdout);
         return 1;
      }

      if (pipe(pipe_fd) == -1) {
         perror("pipe error");
         exit(EX_OSERR);
      }
      pid_2 = fork();
      if (pid_2 < 0) {
         perror("fork");
         exit(EX_OSERR);
      } else if (pid_2 == 0) {
         if (close(pipe_fd[0]) == -1) { /*closes read end as we don't need */
            perror("close error");
            exit(EX_OSERR);
         }
         execute_aux(t->left, file_in, pipe_fd[1]);     /*calls auxiliary method on left branch, using pipe write end as output */
         if (close(pipe_fd[1]) == -1) {
            perror("close error");
            exit(EX_OSERR);
         }
      } else {
         wait(NULL);
         if (close(pipe_fd[1]) == -1) { /*closes write end of pipe as we don't need */
            perror("close error");
            exit(EX_OSERR);
         }
         execute_aux(t->right, pipe_fd[0], file_out);   /*calls auxiliary method on right branch, using pipe read end as input */
         if (close(pipe_fd[0]) == -1) {
            perror("close error");
            exit(EX_OSERR);
         }
      }
   } else if (t->conjunction == SUBSHELL) {     /*SUBSHELL node */
      if ((pid_3 = fork()) < 0) {
         perror("fork");
         exit(EX_OSERR);
      } else if (pid_3 == 0) {  /*child processes left branch of tree */
         wait(&status);
         if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return 0;
         } else {
            return 1;
         }
      } else {
         execute_aux(t->left, file_in, file_out);       /*calls auxiliary method on left tree, returns 1 to ensure right tree processing doesn't occur */
         return 1;
      }
   }
   return 0;
}

/*static void print_tree(struct tree *t) {
   if (t != NULL) {
      print_tree(t->left);

      if (t->conjunction == NONE) {
         printf("NONE: %s, ", t->argv[0]);
      } else {
         printf("%s, ", conj[t->conjunction]);
      }
      printf("IR: %s, ", t->input);
      printf("OR: %s\n", t->output);

      print_tree(t->right);
   }
}*/
