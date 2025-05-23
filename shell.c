/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "OS Concepts Essentials", Silberschatz et al.
 *
 * To compile and run the program:
 *   $ gcc shell.c job_control.c -o shell
 *   $ ./shell
 *	(then type ^D to exit program)
 **/

 #include "job_control.h"   /* Remember to compile with module job_control.c */
 #include <fcntl.h>

 #define MAX_LINE 256 /* 256 chars per line, per command, should be enough */

 job * my_job_list;      /* Job list pointer */

 void signal_handler(int num_sig)
 {
	 printf("Signal %d received\n", num_sig);
	 int status, info;
	 pid_t pid_wait;
	 job_iterator iter = get_iterator(my_job_list);

	 while(has_next(iter)){
		 job * item = next(iter);
		 pid_wait = waitpid(item->pgid, &status, WNOHANG | WCONTINUED | WUNTRACED);
		 if (pid_wait == item->pgid){	
			 enum status status_res = analyze_status(status, &info);
			 printf("Background pid: %d, command: %s, %s, info: %d\n", item->pgid, item->command, status_strings[status_res], info);
			 if (status_res == SUSPENDED){
				 item->state = STOPPED;
			 } else if (status_res == CONTINUED){
				 item->state = BACKGROUND;
			 } else {
				delete_job(my_job_list, item);
			 }
		 } else if (pid_wait == -1){
			perror("Wait error");
		 }
	}
 }
	 

 
 /**
  * MAIN
  **/
 int main(void)
 {
	 char inputBuffer[MAX_LINE]; /* Buffer to hold the command entered */
	 int background;             /* Equals 1 if a command is followed by '&' */
	 char *args[MAX_LINE/2];     /* Command line (of 256) has max of 128 arguments */
	 /* Probably useful variables: */
	 int pid_fork, pid_wait; /* PIDs for created and waited processes */
	 int status;             /* Status returned by wait */
	 enum status status_res; /* Status processed by analyze_status() */
	 int info;				/* Info processed by analyze_status() */
 
	 ignore_terminal_signals();
	 my_job_list = new_list("Job list"); /* Job list pointer */
	 signal(SIGCHLD, signal_handler); /* Catch SIGCHLD signal */
	 

	 while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	 {
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* Get next command */

		char *file_in, *file_out;
		parse_redirections(args, &file_in, &file_out);
		 
		if(args[0]==NULL) continue;   /* Do nothing if empty command */

		if (!strcmp(args[0], "exit")){
			printf("Bye\n");
			exit(EXIT_SUCCESS);
		} 
		else if (!strcmp(args[0], "cd")){
			if (args[1] == NULL) {
				char *home = getenv("HOME"); // Obtener el directorio home desde las variables de entorno
				if (home == NULL) {
					printf("Error: HOME not set\n");
					continue;
				}
				if (chdir(home) != 0) { // Cambiar al directorio HOME
					perror("cd Error");
				}
			} else {
				if (chdir(args[1]) != 0) { // Cambiar al directorio especificado
					perror("cd Error");
				}
			}
		}
		else if (!strcmp(args[0], "jobs")){
			print_list(my_job_list, print_item);
		}
		else if (!strcmp(args[0], "bg")){
			job *item;
			if (my_job_list->next == NULL){
				printf("Error: job list is empty\n");
				continue;
			}
			if (args[1] == NULL){
				item = get_item_bypos(my_job_list, 1);
			}else {
				int number = atoi(args[1]);
				item = get_item_bypos(my_job_list, number);
			}
			kill(item->pgid, SIGCONT);
			item->state = BACKGROUND;
		}
		else if (!strcmp(args[0], "fg")){ 
			job *item;
			if (args[1] == NULL){
				item = get_item_bypos(my_job_list, 1);
			}else {
				int number = atoi(args[1]);
				item = get_item_bypos(my_job_list, number);
			}
			if (item == NULL) {
    			printf("Error: no such job\n");
    			continue;
			}		
			set_terminal(item->pgid);
			killpg(item->pgid, SIGCONT);
			item->state = FOREGROUND;
			int pgid = item->pgid;
			char* name = strdup(item->command);
			delete_job(my_job_list, item);
			pid_wait = waitpid(-pgid, &status, WUNTRACED);
			set_terminal(getpid());
			if (pid_wait > 0){
				status_res = analyze_status(status, &info);
				if (status_res == SUSPENDED){
					add_job(my_job_list, new_job(pgid, name, STOPPED));
				} 
				printf("Foreground pid: %d, command: %s, %s, info: %d\n", pgid, name, status_strings[status_res], info);
			}
			else if (pid_wait == -1){
				perror("Wait error");
			}
			free(name);
		}
		else {
			pid_fork = fork();
			if (pid_fork > 0){
				//Padre
				new_process_group(pid_fork);
				if (!background){
					//Foreground
					set_terminal(pid_fork);
					pid_wait = waitpid(pid_fork, &status, WUNTRACED);
					set_terminal(getpid());
					if (pid_fork == pid_wait){
						status_res = analyze_status(status, &info);
						if (status_res == SUSPENDED){
							add_job(my_job_list, new_job(pid_fork, args[0], STOPPED));
						} 
						printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0], status_strings[status_res], info);
					}
					else if (pid_wait == -1){
						perror("Wait error");
					}
				} else {
					//Background
					add_job(my_job_list, new_job(pid_fork, args[0], BACKGROUND));
					printf("Background job running... pid: %d, command: %s\n", pid_fork, args[0]);
				}
			}
			else if (pid_fork == 0){
				//Hijo
				new_process_group(getpid());
				if (!background) {
					set_terminal(getpid());
				}
				restore_terminal_signals();

				// Redirection de entrada con SYSCALL open
				if (file_in != NULL) {
					int fd_in = open(file_in, O_RDONLY);
					if (fd_in < 0) {
						perror("Error opening input file");
						exit(EXIT_FAILURE);
					}
					dup2(fd_in, STDIN_FILENO);
					close(fd_in);
				}

				// Redirection de salida con fopen
				if (file_out != NULL) {
					FILE* fd_out = fopen(file_out, "w");
					if (fd_out == NULL) {
						perror("Error opening output file");
						exit(EXIT_FAILURE);
					}
					dup2(fileno(fd_out), STDOUT_FILENO);
					fclose(fd_out);
				}

				execvp(args[0], args);
				printf("Error, command not found: %s", args[0]);
				exit(EXIT_FAILURE);
			} else {
				perror("Fork error");
			}
		}
			
		 /** The steps are:
		  *	 (1) Fork a child process using fork()
		  *	 (2) The child process will invoke execvp()
		  * 	 (3) If background == 0, the parent will wait, otherwise continue
		  *	 (4) Shell shows a status message for processed command
		  * 	 (5) Loop returns to get_commnad() function
		  **/
 
	 } /* End while */
 }
 