#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

int g=0;
#define TOKEN_DELIMITERS " \t\r\n\a"

const char* STATUS_STRING[] = {
    "running",
    "done",
    "suspended",
    "continued",
    "terminated"
};

struct process {
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct job {
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
};

struct shell_info {
    char cur_user[64];
    char cur_dir[1024];
    char pw_dir[1024];
    struct job *jobs[20 + 1];
};

struct shell_info *shell;

int get_job_id_by_pid(int pid) {
    int i;
    struct process *proc;

    for (i = 1; i <= 20; i++) {
        if (shell->jobs[i] != NULL) {
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->pid == pid) {
                    return i;
                }
            }
        }
    }

    return -1;
}

struct job* get_job_by_id(int id) {
    if (id > 20) {
        return NULL;
    }

    return shell->jobs[id];
}

int get_pgid_by_job_id(int id) {
    struct job *job = get_job_by_id(id);

    if (job == NULL) {
        return -1;
    }

    return job->pgid;
}

int get_proc_count(int id, int filter) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    int count = 0;
    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (filter == 0 ||
                (filter == 1 && proc->status == 1) ||
                (filter == 2 && proc->status != 1)) {
            count++;
        }
    }

    return count;
}

int get_next_job_id() {
    int i;

    for (i = 1; i <= 20; i++) {
        if (shell->jobs[i] == NULL) {
            return i;
        }
    }

    return -1;
}

int print_processes_of_job(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf(" %d", proc->pid);
    }
    printf("\n");

    return 0;
}

int print_job_status(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    printf("[%d]", id);

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf("\t%d\t%s\t%s", proc->pid,
                STATUS_STRING[proc->status], proc->command);
        if (proc->next != NULL) {
            printf("|\n");
        } else {
            printf("\n");
        }
    }

    return 0;
}

int release_job(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    struct job *job = shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc->output_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

int insert_job(struct job *job) {
    int id = get_next_job_id();

    if (id < 0) {
        return -1;
    }

    job->id = id;
    shell->jobs[id] = job;
    return id;
}

int remove_job(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    release_job(id);
    shell->jobs[id] = NULL;

    return 0;
}

int is_job_completed(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return 0;
    }

    struct process *proc;
    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != 1) {
            return 0;
        }
    }

    return 1;
}

int set_process_status(int pid, int status) {
    int i;
    struct process *proc;

    for (i = 1; i <= 20; i++) {
        if (shell->jobs[i] == NULL) {
            continue;
        }
        for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;
                return 0;
            }
        }
    }

    return -1;
}

int set_job_status(int id, int status) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    int i;
    struct process *proc;

    for (proc = shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != 1) {
            proc->status = status;
        }
    }

    return 0;
}

int wait_for_pid(int pid) {
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
        set_process_status(pid, 1);
    } else if (WIFSIGNALED(status)) {
        set_process_status(pid, 4);
    } else if (WSTOPSIG(status)) {
        status = -1;
        set_process_status(pid, 2);
    }

    return status;
}

int wait_for_job(int id) {
    if (id > 20 || shell->jobs[id] == NULL) {
        return -1;
    }

    int proc_count = get_proc_count(id, 2);
    int wait_pid = -1, wait_count = 0;
    int status = 0;

    do {
        wait_pid = waitpid(-shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) {
            set_process_status(wait_pid, 1);
        } else if (WIFSIGNALED(status)) {
            set_process_status(wait_pid, 4);
        } else if (WSTOPSIG(status)) {
            status = -1;
            set_process_status(wait_pid, 2);
            if (wait_count == proc_count) {
                print_job_status(id);
            }
        }
    } while (wait_count < proc_count);

    return status;
}

int get_command_type(char *command) {
    if (strcmp(command, "quit") == 0) {
        return 1;
    } else if (strcmp(command, "cd") == 0) {
        return 2;
    } else if (strcmp(command, "jobs") == 0) {
        return 3;
    } else if (strcmp(command, "fg") == 0) {
        return 4;
    } else if (strcmp(command, "bg") == 0) {
        return 5;
    } else if (strcmp(command, "kill") == 0) {
        return 6;
    } else if (strcmp(command, "setenv") == 0) {
        return 7;
    } else if (strcmp(command, "unsetenv") == 0) {
        return 8;
    }
    else if (strcmp(command, "overkill") == 0) {
        return 9;
    }
    else if (strcmp(command, "kjob") == 0) {
        return 10;
    }
    else {
        return 0;
    }
}

char* helper_strtrim(char* line) {
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

void mysh_update_cwd_info() {
    getcwd(shell->cur_dir, sizeof(shell->cur_dir));
}

int mysh_cd(int argc, char** argv) {
    if (argc == 1) {
        chdir(shell->pw_dir);
        mysh_update_cwd_info();
        return 0;
    }

    if (chdir(argv[1]) == 0) {
        mysh_update_cwd_info();
        return 0;
    } else {
        printf("mysh: cd %s: No such file or directory\n", argv[1]);
        return 0;
    }
}

int mysh_jobs(int argc, char **argv) {
    int i;

    for (i = 0; i < 20; i++) {
        if (shell->jobs[i] != NULL) {
            print_job_status(i);
        }
    }

    return 0;
}

int mysh_fg(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: fg <pid>\n");
        return -1;
    }

    int status;
    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1]+1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: fg %s: no such job\n", argv[1]);
            return -1;
        }
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(-pid, SIGCONT) < 0) {
        printf("mysh: fg %d: job not found\n", pid);
        return -1;
    }

    tcsetpgrp(0, pid);

    if (job_id > 0) {
        set_job_status(job_id, 3);
        print_job_status(job_id);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    } else {
        wait_for_pid(pid);
    }

    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, getpid());
    signal(SIGTTOU, SIG_DFL);

    return 0;
}

int mysh_bg(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: bg <pid>\n");
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: bg %s: no such job\n", argv[1]);
            return -1;
        }
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(-pid, SIGCONT) < 0) {
        printf("mysh: bg %d: job not found\n", pid);
        return -1;
    }

    if (job_id > 0) {
        set_job_status(job_id, 3);
        print_job_status(job_id);
    }

    return 0;
}

int mysh_kill(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: kill <pid>\n");
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    if (argv[1][0] == '%') {
        job_id = atoi(argv[1] + 1);
        pid = get_pgid_by_job_id(job_id);
        if (pid < 0) {
            printf("mysh: kill %s: no such job\n", argv[1]);
            return -1;
        }
        pid = -pid;
    } else {
        pid = atoi(argv[1]);
    }

    if (kill(pid, SIGKILL) < 0) {
        printf("mysh: kill %d: job not found\n", pid);
        return 0;
    }

    if (job_id > 0) {
        set_job_status(job_id, 4);
        print_job_status(job_id);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    }

    return 1;
}


int mysh_overkill(int argc, char **argv) {
    if (argc != 1) {
        printf("usage: overkill\n");
        return -1;
    }
    int i;
    struct process *proc;
    for (i = 0; i < 20; i++) {
        if (shell->jobs[i] != NULL) {
            for (proc = shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                kill(proc->pid,SIGKILL);
                if (i> 0) {
                    set_job_status(i, 4);
                    print_job_status(i);
                    if (wait_for_job(i) >= 0) {
                        remove_job(i);
                    }
                }
                if (proc->next == NULL) {
                    break;
                }
            }
        }
    }
    return 1;

}

int mysh_kjob(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: kjob <jobid> <signum>\n");
        return -1;
    }
    struct process *proc;
    int jo = (atoi)(argv[1]);
    int ko = (atoi)(argv[2]);
    //printf("%d-%d\n",jo,ko);
        if(shell->jobs[jo] == NULL)
        {
            printf("No Such Process\n");
        }
        else
        {
            for(proc = shell->jobs[jo]->root;proc != NULL; proc = proc->next)
            {
                if(ko==9)
                {
                    kill(proc->pid,SIGKILL);
                    if (jo> 0) {
                        set_job_status(jo, 4);
                        print_job_status(jo);
                        if (wait_for_job(jo) >= 0) {
                            remove_job(jo);
                        }
                    }
                }
                else if(ko == 2)
                {
                    kill(proc->pid,SIGINT);
                }
                else if(ko == 20)
                {
                    //printf("20\n");
                    if (kill((proc->pid), SIGCONT) < 0) {
                        printf("mysh: bg %d: job not found\n", proc->pid);
                        return -1;
                    }

                    if (jo > 0) {
                        set_job_status(jo, 3);
                        print_job_status(jo);
                    }
                }
                else if(ko == 19)
                {
                    if (kill((proc->pid), SIGSTOP) < 0) {
                        printf("mysh: bg %d: job not found\n", proc->pid);
                        return -1;
                    }
                    if (jo > 0) {
                        set_job_status(jo, 2);
                        print_job_status(jo);
                    }

                }
            }
        }

    return 1;

}

int mysh_export(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: setenv KEY=VALUE\n");
        return -1;
    }

    return putenv(argv[1]);
}

int mysh_unset(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: unsetenv KEY\n");
        return -1;
    }

    return unsetenv(argv[1]);
}

int mysh_exit() {
    printf("Goodbye!\n");
    exit(0);
}

void check_zombie() {
    int status, pid;
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            set_process_status(pid, 1);
        } else if (WIFSTOPPED(status)) {
            set_process_status(pid, 2);
        } else if (WIFCONTINUED(status)) {
            set_process_status(pid, 3);
        }

        int job_id = get_job_id_by_pid(pid);
        if (job_id > 0 && is_job_completed(job_id)) {
            print_job_status(job_id);
            remove_job(job_id);
        }
    }
}

void sigint_handler(int signal) {
    printf("\n");
}

int mysh_execute_builtin_command(struct process *proc) {
    int status = 1;

    switch (proc->type) {
        case 1:
            mysh_exit();
            break;
        case 2:
            mysh_cd(proc->argc, proc->argv);
            break;
        case 3:
            mysh_jobs(proc->argc, proc->argv);
            break;
        case 4:
            mysh_fg(proc->argc, proc->argv);
            break;
        case 5:
            mysh_bg(proc->argc, proc->argv);
            break;
        case 6:
            mysh_kill(proc->argc, proc->argv);
            break;
        case 7:
            mysh_export(proc->argc, proc->argv);
            break;
        case 8:
            mysh_unset(proc->argc, proc->argv);
            break;
        case 9:
            mysh_overkill(proc->argc, proc->argv);
            break;
        case 10:
            mysh_kjob(proc->argc,proc->argv);
            break;
        default:
            status = 0;
            break;
    }

    return status;
}

int mysh_launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode) {
    proc->status = 0;
    if (proc->type != 0 && mysh_execute_builtin_command(proc)) {
        return 0;
    }

    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            printf("mysh: %s: command not found\n", proc->argv[0]);
            exit(0);
        }

        exit(0);
    } else {
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == 1) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }

    return status;
}

int mysh_launch_job(struct job *job) {
    struct process *proc;
    int status = 0, in_fd = 0, fd[2], job_id = -1;

    check_zombie();
    if (job->root->type == 0) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root && proc->input_path != NULL) {
            in_fd = open(proc->input_path, O_RDONLY);
            if (in_fd < 0) {
                printf("mysh: no such file or directory: %s\n", proc->input_path);
                remove_job(job_id);
                return -1;
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = mysh_launch_process(job, proc, in_fd, fd[1], 2);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                if(g==1)
                {
                    out_fd = open(proc->output_path, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                    g=0;
                }
                else
                {
                    out_fd = open(proc->output_path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                }
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            status = mysh_launch_process(job, proc, in_fd, out_fd, job->mode);
        }
    }

    if (job->root->type == 0) {
        if (status >= 0 && job->mode == 1) {
            remove_job(job_id);
        } else if (job->mode == 0) {
            print_processes_of_job(job_id);
        }
    }

    return status;
}

struct process* mysh_parse_command_segment(char *segment) {
    int bufsize = 64;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **tokens = (char**) malloc(bufsize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += 64;
            bufsize += glob_count;
            tokens = (char**) realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    int i = 0, argc = 0;
    char *input_path = NULL, *output_path = NULL;
    while (i < position) {
        if (tokens[i][0] == '<' || tokens[i][0] == '>') {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (tokens[i][0] == '<') {
            if (strlen(tokens[i]) == 1) {
                input_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(input_path, tokens[i + 1]);
                i++;
            } else {
                input_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(input_path, tokens[i] + 1);
            }
        } else if (tokens[i][0] == '>') {
            if(tokens[i][1] == '>' && strlen(tokens[i]) == 2)
            {
                g = 1;
            }
            if (strlen(tokens[i]) == 1 || g==1) {
                output_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(output_path, tokens[i + 1]);
                //	printf("%s\n",output_path);
                i++;
            } else {
                output_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(output_path, tokens[i] + 1);
            }
        } else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        tokens[i] = NULL;
    }

    struct process *new_proc = (struct process*) malloc(sizeof(struct process));
    new_proc->command = command;
    new_proc->argv = tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->pid = -1;
    new_proc->type = get_command_type(tokens[0]);
    new_proc->next = NULL;
    return new_proc;
}

struct job* mysh_parse_command(char *line) {
    line = helper_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL, *proc = NULL;
    char *line_cursor = line, *c = line, *seg;
    int seg_len = 0, mode = 1;

    if (line[strlen(line) - 1] == '&') {
        mode = 0;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = (char*) malloc((seg_len + 1) * sizeof(char));
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = mysh_parse_command_segment(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = (struct job*) malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    return new_job;
}

char* mysh_read_line() {
    int bufsize = 1024;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += 1024;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void mysh_print_promt(char buf[]) {
    printf("%s@%s:%s>", shell->cur_user,buf, shell->cur_dir);

}

void mysh_print_welcome() {
    printf("Welcome to Bash!!!\n");
}

void mysh_loop() {
    char *line; char buf[100000];
    struct job *job;
    int status = 1;
    FILE* fd;
    fd = fopen("/etc/hostname","r");
    fscanf(fd,"%s",buf);//fscaf is used to write opened file from fd to buff buf=hostname
    fclose(fd);  //cl

    while (1) {
        mysh_print_promt(buf);
        line = mysh_read_line();
        if (strlen(line) == 0) {
            check_zombie();
            continue;
        }
        job = mysh_parse_command(line);
        status = mysh_launch_job(job);
    }
}

void mysh_init() {
    struct sigaction sigint_action = {
        .sa_handler = &sigint_handler,
        .sa_flags = 0
    };
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    shell = (struct shell_info*) malloc(sizeof(struct shell_info));
    getlogin_r(shell->cur_user, sizeof(shell->cur_user));

    struct passwd *pw = getpwuid(getuid());
    strcpy(shell->pw_dir, pw->pw_dir);
    strcpy(shell->cur_user,pw->pw_name);

    int i;
    for (i = 0; i < 20; i++) {
        shell->jobs[i] = NULL;
    }

    mysh_update_cwd_info();
}

int main(int argc, char **argv) {
    mysh_init();
    mysh_print_welcome();
    mysh_loop();

    return EXIT_SUCCESS;
}
