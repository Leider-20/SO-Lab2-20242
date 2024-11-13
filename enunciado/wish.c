#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define ERROR_MESSAGE "An error has occurred\n"

// Prototipos de funciones
void execute_command(char **args, int background, char *output_file);
void parse_command(char *line);
void run_shell_interactive();
void run_shell_batch(const char *filename);

// Variables globales para el path
char *path[2] = {"/bin", NULL};

// Función de error para simplificar el código
void print_error() {
    write(STDERR_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
}

// Implementación de comandos internos
int handle_builtin(char **args) {
    if (strcmp(args[0], "exit") == 0) {
        if (args[1] != NULL) {
            print_error();
            return -1;
        }
        exit(0);
    } else if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            print_error();
            return -1;
        }
        if (chdir(args[1]) != 0) {
            print_error();
        }
        return 1;
    } else if (strcmp(args[0], "path") == 0) {
        int i;
        for (i = 0; args[i + 1] != NULL && i < 2; i++) {
            path[i] = args[i + 1];
        }
        path[i] = NULL;
        return 1;
    }
    return 0;
}

// Ejecución de comandos externos o internos con redirección y modo background
void execute_command(char **args, int background, char *output_file) {
    if (handle_builtin(args)) return;

    pid_t pid = fork();
    if (pid == 0) {  // Proceso hijo
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                print_error();
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        for (int i = 0; path[i] != NULL; i++) {
            char cmd_path[MAX_LINE];
            snprintf(cmd_path, sizeof(cmd_path), "%s/%s", path[i], args[0]);
            if (access(cmd_path, X_OK) == 0) {
                execv(cmd_path, args);
                print_error();
                exit(1);
            }
        }
        print_error();
        exit(1);
    } else if (pid > 0) {  // Proceso padre
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    } else {
        print_error();
    }
}

// Parseo de la línea de comando para identificar redirección y comandos en paralelo
void parse_command(char *line) {
    char *commands[MAX_LINE / 2];
    int num_commands = 0;
    char *cmd = strtok(line, "&");
    while (cmd != NULL) {
        commands[num_commands++] = cmd;
        cmd = strtok(NULL, "&");
    }

    for (int i = 0; i < num_commands; i++) {
        char *args[MAX_LINE / 2];
        int background = (i < num_commands - 1) ? 1 : 0;
        char *output_file = NULL;

        char *redir = strstr(commands[i], ">");
        if (redir != NULL) {
            *redir = '\0';
            output_file = strtok(redir + 1, " \t\n");
            if (strtok(NULL, " \t\n") != NULL) {
                print_error();
                continue;
            }
        }

        int arg_count = 0;
        char *token = strtok(commands[i], " \t\n");
        while (token != NULL && arg_count < MAX_LINE / 2) {
            args[arg_count++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[arg_count] = NULL;

        if (arg_count > 0) {
            execute_command(args, background, output_file);
        }
    }
}

// Modo interactivo
void run_shell_interactive() {
    char line[MAX_LINE];
    while (1) {
        printf("wish> ");
        fflush(stdout);
        if (fgets(line, MAX_LINE, stdin) == NULL) {
            break;
        }
        parse_command(line);
    }
}

// Modo batch
void run_shell_batch(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        print_error();
        exit(1);
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file) != NULL) {
        parse_command(line);
    }

    fclose(file);
}

// Función principal
int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_shell_interactive();
    } else if (argc == 2) {
        run_shell_batch(argv[1]);
    } else {
        print_error();
        exit(1);
    }
    return 0;
}
