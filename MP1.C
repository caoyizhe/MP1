#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include<signal.h>
#include<fcntl.h>
#include <limits.h>
#include <libgen.h>

#define MAXLINE    1024   /* max line size */
#define MAXARGS    64    /* max args on a command line */

struct command  // command Struct contains commandline and # of arguments
{
    int argc;
    char **argv;
};

struct job      // job Struct contains the information of a process
{
    int id;	// job id
    pid_t pid;	// process id
    char *cmdl; // command line
    command **cmd; // parsed command line
    int ncmd;   //the number of commands
    int isbg; //is it a background process?
    int stat;   //the status of this process
    job *next;  //pointer to the next job
};

char stat[][10] = {"Running", "Done", "Stopped"};
char prompt[] = "CSC2/456Shell$ ";
int CUR_JOB_ID;
job *head;
pid_t PGRP;

/*
 *Function: read_command(char * cmd, int &ncmd, int &isbg)
 *
 *Description:
 * Read the command line
 *
 *Input:
 * command line, number of commands, background status
 *
 *Output:
 * the readable command line
 * number of commands in ncmd
 * background job:isbg=1 otherwise,isbg=0
 */
char * read_command(char * cmd, int &ncmd, int &isbg);

/*
 *Function: make_command(char *cmdl, int ncmd)
 *
 *Description:
 * put the command line into a command struct for future use
 *
 *Input:
 * readable command line, number of commands
 *
 *Output:
 * A command struct contains the command and number of commands
 *
 */
command **make_command(char *cmdl, int ncmd);

/*
 *Function: do_internal(command *cmd)
 *
 *Description:
 * execute internal commands
 *
 *Input:
 * command which will be execute
 *
 *Output:
 * if success returns 1
 * if fail returns -1
 * Otherwise returns 0
 */
int do_internal(command *cmd);

/*
 *Function: do_pipes(const command **cmd, const int ncmd, const int isbg)
 *
 *Description:
 * execute commands in pipe
 *
 *Input:
 * command which will be execute, number of commands, background status
 *
 *Output:
 * if success returns 0
 * if fail returns -1
 */
int do_pipes(const command **cmd, const int ncmd, const int isbg);

/*
 *Function: print_job(job *job)
 *
 *Description:
 * printf the job
 *
 *Input:
 * the job which will be printed
 *
 *Output:
 * if success returns 0
 * if fail returns -1
 */
int print_job(job *job);

/*
 *Function: add_job(job *target)
 *
 *Description:
 * add a new job to job list
 *
 *Input:
 * the job which will be added
 */
void add_job(job *target);

/*
 *Function: free_job(job *job)
 *
 *Description:
 * free the job memory
 *
 *Input:
 * the job which will be freed
 */
void free_job(job *job);

/*
 *Function: search_job(pid_t pid)
 *
 *Description:
 * use pid to search job
 *
 *Input:
 * the process id of the job
 *
 *Output:
 * the job that being searched
 */
job *search_job(pid_t pid);

/*
 *Function: delete_job(pid_t pid)
 *
 *Description:
 * use pid to delete job
 *
 *Input:
 * the process id of the job
 *
 *Output:
 * if success returns 0
 * if fail returns -1
 */
int delete_job(pid_t pid);
void sigchld_handler(int sig);

int main(int argc, char** argv)
{
    head = (job*)calloc(1, sizeof(job));
    head->id = 0;
    head->pid = -1;
    head->cmdl = NULL;
    head->cmd = NULL;
    head->ncmd = 0;
    head->isbg = -1;
    head->stat = -1;
    head->next = NULL;

    PGRP = getpgrp();
    char cmdline[MAXLINE];
    
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);
    
    while(1)
    {
        bzero(cmdline, sizeof(cmdline));
        printf("%s", prompt);
        if(fgets(cmdline, MAXLINE, stdin)==NULL) {
        	printf("\n");
        	exit(0);
        }
        cmdline[strlen(cmdline) - 1] = '\0';
        
        job *cur_job = (job*)calloc(1, sizeof(job));
        cur_job->cmdl = read_command(cmdline, cur_job->ncmd, cur_job->isbg);
        cur_job->cmd = make_command(cur_job->cmdl, cur_job->ncmd);
        
        if(cur_job->cmd == NULL || cur_job->ncmd == 0)
        {
            continue;
        }
        
        if(cur_job->ncmd == 1)
        {
            if(do_internal(cur_job->cmd[0]) != 0)
            {
                continue;
            }
            
            pid_t pid = fork();

            if(pid == 0)
            {
                setpgid(0, 0);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
               
                if(execvp(cur_job->cmd[0]->argv[0], cur_job->cmd[0]->argv) == -1)
                {
                    printf("Execute Error: Wrong Command.\n");
                }
                exit(0);
            }
            else
            {
                setpgid(pid, pid);
                cur_job->pid = pid;
                add_job(cur_job);
                CUR_JOB_ID = cur_job->id;
                if(!cur_job->isbg)
                {
                    tcsetpgrp(STDIN_FILENO, pid);
                    tcsetpgrp(STDOUT_FILENO, pid);
                    int chld_stat;
                    waitpid(pid, &chld_stat, WUNTRACED);
                    if(WIFSTOPPED(chld_stat) != 0)
                    {
                        job *p = search_job(pid);
                        if(p != NULL)
                        {
                            p->stat = 2;
                            print_job(p);
                        }
                    }
		    else
		    {
			delete_job(pid);
		    }

                    tcsetpgrp(STDIN_FILENO, PGRP);
                    tcsetpgrp(STDOUT_FILENO, PGRP);
                }
            }
        }
        else
        {
            pid_t pid = fork();

            if(pid == 0)
            {
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                setpgid(0, 0);
                
                if(do_pipes((const command **)cur_job->cmd, cur_job->ncmd, cur_job->isbg) != 0)
                {
                    printf("execute pipes failed\n");
                }     
                exit(0);
            }
            else
            {
                setpgid(pid, pid);
                cur_job->pid = pid;
                add_job(cur_job);
                CUR_JOB_ID = cur_job->id;
                if(!cur_job->isbg)
                {
                    tcsetpgrp(STDIN_FILENO, pid);
                    tcsetpgrp(STDOUT_FILENO, pid);
                    int chld_stat;
                    waitpid(pid, &chld_stat, WUNTRACED);
                    if(WIFSTOPPED(chld_stat) != 0)
                    {
                        job *p = search_job(pid);
                        if(p != NULL)
                        {
                            p->stat = 2;
                            print_job(p);
                        }
                    }
		    else
		    {
			delete_job(pid);
		    }

                    tcsetpgrp(STDIN_FILENO, PGRP);
                    tcsetpgrp(STDOUT_FILENO, PGRP);
                }
            }
         
            
        }
  
    }
}

char * read_command(char * cmd, int &ncmd, int &isbg)
{
    isbg = 0;
    char * t = (char *)calloc(MAXLINE + 1, sizeof(char));
    int k = 0;
    int i = 0;
    ncmd = 0;
    long len = strlen(cmd);
    int b = 1;
    
    while(i <= len)
    {
        while(i < len && (cmd[i] == ' ' || cmd[i] == '\t'))
        {
            i++;
            
        }
        if( i < len && (cmd[i] == '|' || cmd[i] == '&'))
        {
            if(b == 1 || b == 2)
            {
                printf("Unexpected character: %c\n", cmd[i]);
                return NULL;
            }
            else
            {
                if(k > MAXLINE)
                {
                    break;
                }
                t[k++] = cmd[i];
                t[k++] = ' ';
                if(cmd[i] == '|')
                    b = 1;
                else
                    b = 2;
                ncmd ++;
            }
            i++;
            continue;
        }
        while(i < len && cmd[i] != ' ' && cmd[i] != '\t' && cmd[i] != '|' && cmd[i] != '&')
        {
            if(b == 2)
            {
                printf("Unexpected character: &\n");
                return NULL;
            }
            b = 0;
            if(cmd[i] == '\\' && i < len - 1)
            {
                if(k >= MAXLINE)
                {
                    break;
                }
                t[k++] = '\\';
                t[k++] = cmd[++i];
            }
            else
            {
                if(k > MAXLINE)
                {
                    break;
                }
                t[k++] = cmd[i];
            }
            i++;
        }
        if( i <= len && (cmd[i] == '|' || cmd[i] == '&' || cmd[i] == '\0'))
        {
            if( (b == 2||b == 1) && cmd[i] != '\0')
            {
                printf("Unexpected character: %c\n", cmd[i]);
                return NULL;
            }
            else
            {
                if(k >= MAXLINE)
                {
                    break;
                }
                if(b == 0 || (b != 1 && b != 2 && cmd[i] == '\0'))
                {
                    ncmd++;
                }
                t[k++] = ' ';
                t[k++] = cmd[i];
                t[k++] = ' ';
                if(cmd[i] == '&')
                    b = 2;
                else
                    b = 1;
            }
            i++;
        }
        else if( i < len && (cmd[i] == ' '|| cmd[i] == '\t' ))
        {
            if(k >= MAXLINE)
            {
                break;
            }
            t[k++] = ' ';
            i++;
        }
    }
    if(i < len)
    {
        printf("Command is too long.\n");
        return NULL;
    }
    
    for(int i = k - 1; i >= 0; i--)
    {
        if(t[i] == '&')
        {
            isbg = 1;
        }
        if(t[i] != ' ' && t[i] != '\0' && t[i] != '&')
        {
            t[i+1] = ' ';
            t[i+2] = '\0';
            break;
        }
    }
    return t;
}

command **make_command(char *cmdl, int ncmd)
{
    if(cmdl == NULL || strlen(cmdl) == 0)
        return NULL;
    
    command **cmd = (command**)calloc(ncmd, sizeof(command*));
    for(int i = 0;i < ncmd; i++)
    {
        cmd[i] = (command*)calloc(1, sizeof(command));
        cmd[i]->argv = (char **)calloc(MAXARGS, sizeof(char *));
        cmd[i]->argc = 0;
    }
    
    char temp_arg[512];
    bzero(temp_arg, sizeof(temp_arg));
    int temp_arg_i = 0;;
   
    long len = strlen(cmdl);
    
    int cmd_i = 0;
    int argc = 0;
    
    int i = 0;
    int es = 0;
    while(i <= len)
    {
        if(es)
        {
            temp_arg[temp_arg_i++] = cmdl[i++];
            es = 0;
        }
        else if(cmdl[i] == ' ')
        {
            if(temp_arg_i != 0)
            {
                cmd[cmd_i]->argv[argc] = (char*)calloc(temp_arg_i + 1, sizeof(char));
                memcpy(cmd[cmd_i]->argv[argc], temp_arg, temp_arg_i * sizeof(char));
                cmd[cmd_i]->argv[argc][temp_arg_i] = '\0';
                argc++;
                temp_arg_i = 0;
                bzero(temp_arg, sizeof(temp_arg));
            }
            i++;
        }
        else if(cmdl[i] == '|' || cmdl[i] == '&' || cmdl[i] == '\0')
        {
            if(argc != 0)
            {
                cmd[cmd_i]->argc = argc;
                cmd[cmd_i]->argv[argc++] = NULL;
                cmd_i++;
                argc = 0;
            }
            i++;
        }
        else
        {
            temp_arg[temp_arg_i++] = cmdl[i];
            if(cmdl[i] == '\\')
            {
                es = 1;
            }
            i++;
        }
    }
    
    return cmd;
}

int do_internal(command *cmd)
{
    if(strcmp(cmd->argv[0], "exit") == 0)
    {
        exit(0);
    }
    else if(strcmp(cmd->argv[0], "cd") == 0)
    {
        if(chdir(cmd->argv[1]) < 0)
        {
            printf("change directory failed\n");
            return -1;
        }
        return 1;
    }
    else if(strcmp(cmd->argv[0], "jobs") == 0)
    {
        job *p = head;
        
        while(p->next != NULL)
        {
            print_job(p->next);
            p = p->next;
        }
        return 1;
    }
    else if(strcmp(cmd->argv[0], "fg") == 0)
    {
        if(cmd->argv[1] != NULL)
        {
            for(int i = 0; i < strlen(cmd->argv[1]); i++)
            {
                if(cmd->argv[1][i] > '9' || cmd->argv[1][i] < '0')
                {
                    printf("fg usage: fg [job_id]\n");
                    return -1;
                }
            }
        }
        
        job *p = head->next;
        pid_t pid = -1;
        
        int id;
        if(cmd->argv[1] == NULL)
        {
            id = CUR_JOB_ID;
        }
        else
        {
            id = atoi(cmd->argv[1]);
        }
        while(p != NULL)
        {
            if(p->id == id)
            {
                pid = p->pid;
                break;
            }
            else
                p = p->next;
        }
        if(pid == -1)
        {
            printf("fg %d: no such job.\n", id);
            return -1;
        }
        p->stat = 0;
        p->isbg = 0;
        
        print_job(p);
        kill(-p->pid, SIGCONT);
        tcsetpgrp(STDIN_FILENO, pid);
        tcsetpgrp(STDOUT_FILENO, pid);
        CUR_JOB_ID = p->id;
        int chld_stat;
        waitpid(pid, &chld_stat, WUNTRACED);
        if(WIFSTOPPED(chld_stat) != 0)
        {
            job *p = search_job(pid);
            if(p != NULL)
            {
                p->stat = 2;
            }
        }
        tcsetpgrp(STDIN_FILENO, PGRP);
        tcsetpgrp(STDOUT_FILENO, PGRP);
        return 1;
    }
    else if(strcmp(cmd->argv[0], "bg") == 0)
    {
        if(cmd->argv[1] != NULL)
        {
            for(int i = 0; i < strlen(cmd->argv[1]); i++)
            {
                if(cmd->argv[1][i] > '9' || cmd->argv[1][i] < '0')
                {
                    printf("Error! Correct formate: bg [job_id]\n");
                    return -1;
                }
            }
        }
        
        job *p = head->next;
        pid_t pid = -1;
        
        int id;
        if(cmd->argv[1] == NULL)
        {
            id = CUR_JOB_ID;
        }
        else
        {
            id = atoi(cmd->argv[1]);
        }
        while(p != NULL)
        {
            if(p->id == id)
            {
                pid = p->pid;
                //printf("pid : %d\n", pid);
                break;
            }
            else
                p = p->next;
        }
        if(pid == -1)
        {
            printf("bg %d: no such job.\n", id);
            return -1;
        }
        p->stat = 0;
        p->isbg = 1;
        print_job(p);
        kill(-p->pid, SIGCONT);
        CUR_JOB_ID = p->id;
        return 1;
    }
    else return 0;

}

int do_pipes(const command **cmd, const int ncmd, const int isbg)
{
    int fd[ncmd - 1][2];
    for(int i = 0 ; i < ncmd - 1; i++)
    {
        if(pipe(fd[i]) < 0)
        {
            printf("execute pipe failed\n");
            return -1;
        }
    }
    
    for(int i = 0; i < ncmd; i++)
    {
        
        pid_t pid = fork();

        if(pid == 0)
        {
            if(i == 0)
            {
                dup2(fd[i][1], STDOUT_FILENO);
                close(fd[i][0]);
                for(int j = 1 ; j < ncmd - 1; j++)
                {
                    close(fd[j][0]);
                    close(fd[j][1]);
                }
            }
            else if(i == ncmd - 1)
            {
                dup2(fd[i-1][0], STDIN_FILENO);
                close(fd[i-1][1]);
                
                for(int j = 0 ; j < i - 1; j++)
                {
                    close(fd[j][0]);
                    close(fd[j][1]);
                }
                
            }
            else
            {
                dup2(fd[i][1], STDOUT_FILENO);
                close(fd[i][0]);
                dup2(fd[i-1][0], STDIN_FILENO);
                close(fd[i-1][1]);
                for(int j = 0; j < ncmd - 1;j ++)
                {
                    if(j != i && j != i-1)
                    {
                        close(fd[j][0]);
                        close(fd[j][1]);
                    }
                }
            }
        
            if(execvp(cmd[i]->argv[0], cmd[i]->argv) == -1)
            {
                printf("%s\n", cmd[i]->argv[0]);
                printf("execvp failed\n");
                return -1;
            }
        
        }
    }

    for(int i = 0 ; i < ncmd - 1; i++)
    {
        close(fd[i][0]);
        close(fd[i][1]);
    }

    for(int i = 0 ; i < ncmd; i++)
        wait(NULL);
    
    return 0;
}

int print_job(job *job)
{
    if(job == NULL)
        return -1;
    printf("%d: %s", job->id, job->cmdl);
    if(job->isbg)  printf(" [&]");
    printf("\n");
    return 0;
}

void add_job(job *target)
{
    job *p = head;
    while(p->next != NULL)
    {
        p = p->next;
    }
    p->next = target;
    target->next = NULL;
    target->id = p->id + 1;
}

void free_job(job *job)
{
    if(job != NULL)
    {
        if(job->cmdl != NULL)
        {
            free(job->cmdl);
        }
        if(job->cmd != NULL)
        {
            for(int i = 0 ; i < job->ncmd; i++)
            {
                if(job->cmd[i] != NULL)
                {
                    free(job->cmd[i]);
                }
            }
            free(job->cmd);
        }
        free(job);
    }
}
job * search_job(pid_t pid)
{
    job *p = head;
    while(p->next!= NULL)
    {
        if(p->next->pid == pid)
        {
            return p->next;
        }
        p = p->next;
    }
    return NULL;
}

int delete_job(pid_t pid)
{
    job *p = head;
    while(p->next != NULL)
    {
        if(p->next->pid == pid)
        {
            job * temp = p->next;
            p->next = p->next->next;
            free_job(temp);
            return 0;
        }
        else
            p = p->next;
    }
    return -1;
}

void sigchld_handler(int sig)
{
    pid_t pid;
    while( (pid = waitpid( (pid_t)-1, NULL, WNOHANG)) > 0 )
    {
        job *p = search_job(pid);
        p->stat = 1;
        if(p->isbg)
        {
            print_job(p);
        }
        delete_job(pid);
    }

}
