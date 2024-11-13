#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_ARGS 100
#define MAX_PATHS 100
#define MAX_BUFFER 1024

// Estructura para manejar errores comunes
char error_message[] = "An error has occurred\n";

// Función para imprimir errores
void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

// Función para dividir un string por delimitadores
char **parse_input(char *input, int *num_args) {
    char **args = malloc(MAX_ARGS * sizeof(char *));
    char *token = strtok(input, " \t\n");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    *num_args = i;
    return args;
}

// Comando incorporado: "exit"
void handle_exit(char *args[]) {
    if (args[1] != NULL) {
        print_error();
    } else {
        exit(0);
    }
}

// Comando incorporado: "cd"
void handle_cd(char *args[]) {
    if (args[1] == NULL || chdir(args[1]) != 0) {
        print_error();
    }
}

// Comando incorporado: "path"
void handle_path(char *args[], char *paths[]) {
    int i = 1;
    while (args[i] != NULL && i < MAX_PATHS) {
        paths[i - 1] = args[i];
        i++;
    }
    paths[i - 1] = NULL;
}

// Verifica si un comando es incorporado
int is_builtin(char *args[], char *paths[]) {
    if (strcmp(args[0], "exit") == 0) {
        handle_exit(args);
        return 1;
    } else if (strcmp(args[0], "cd") == 0) {
        handle_cd(args);
        return 1;
    } else if (strcmp(args[0], "path") == 0) {
        handle_path(args, paths);
        return 1;
    }
    return 0;
}

// Función para ejecutar comandos externos
void execute_external(char *args[], char *paths[], int redirect, char *filename) {
    pid_t pid = fork();
    if (pid == -1) {
        print_error();
        return;
    }

    if (pid == 0) { // Proceso hijo
        if (redirect) { // Manejo de redirección
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1 || dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
                print_error();
                exit(1);
            }
            close(fd);
        }

        // Buscar el comando en las rutas proporcionadas
        for (int i = 0; paths[i] != NULL; i++) {
            char exec_path[MAX_BUFFER];
            snprintf(exec_path, MAX_BUFFER, "%s/%s", paths[i], args[0]);
            if (access(exec_path, X_OK) == 0) {
                execv(exec_path, args);
                break;
            }
        }
        print_error();
        exit(1);
    }
}

// Procesar los comandos
void process_command(char *input, char *paths[]) {
    char *command = strtok(input, "&");
    while (command != NULL) {
        int num_args;
        char **args = parse_input(command, &num_args);

        // Verificar si es un comando incorporado
        if (!is_builtin(args, paths)) {
            int redirect = 0;
            char *filename = NULL;

            // Verificar si hay redirección
            char *redir = strchr(command, '>');
            if (redir) {
                *redir = '\0';
                filename = strtok(redir + 1, " \t\n");
                redirect = 1;
            }

            // Ejecutar el comando externo
            execute_external(args, paths, redirect, filename);
        }

        // Esperar a que el proceso hijo termine si no es en segundo plano
        while (wait(NULL) > 0);

        command = strtok(NULL, "&");
    }
}

// Función principal del shell
int main(int argc, char *argv[]) {
    char *paths[MAX_PATHS] = {"/bin", NULL}; // Rutas por defecto
    char buffer[MAX_BUFFER];
    FILE *input = stdin;

    // Verificar si se está ejecutando en modo batch
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            print_error();
            exit(1);
        }
    } else if (argc > 2) {
        print_error();
        exit(1);
    }

    // Bucle principal
    while (1) {
        if (input == stdin) {
            printf("wish> ");
        }

        if (!fgets(buffer, MAX_BUFFER, input)) {
            break;
        }

        process_command(buffer, paths);
    }

    if (input != stdin) fclose(input);
    return 0;
}
