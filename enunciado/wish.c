#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 64

char error_message[] = "An error has ocurred";

typedef struct {
    int noPermitido;
    int interactivo;
    char *comandos[MAX_ARGS];
    char *argumentos[MAX_ARGS];
    char *archivoSalida;
    int redireccionSalida;
    int multiplesArchivos;
    int comandoActual;
    FILE *flujo;
} InfoShell;

InfoShell shell;

void inicializarShell() {
    shell.noPermitido = 0;
    shell.interactivo = 1;
    shell.redireccionSalida = 0;
    shell.multiplesArchivos = 0;
    shell.comandoActual = 0;
    shell.flujo = stdin;
}

void ejecutar_cd(char **arg) {
    if (chdir(arg[1]) == -1) {
        fprintf(stderr, "%s\n", error_message);
    }
}

void ejecutar_exit(char **arg) {
    if (arg[1] != NULL) {
        fprintf(stderr, "%s\n", error_message);
    } else {
        exit(0);
    }
}

char *ejecutar_path(char **arg, char *path) {
    int num_path = 0;
    char *pathCopy = strdup(path);
    int i = 1;

    while (arg[i] != NULL) {
        pathCopy = realloc(pathCopy, strlen(pathCopy) + strlen(arg[i]) + 2);
        strcat(pathCopy, ":");
        strcat(pathCopy, arg[i]);
        setenv("PATH", pathCopy, 1);
        putenv("PATH");
        i++;
        num_path++;
    }

    shell.noPermitido = 0;
    return pathCopy;
}

void verificar_modo_bash(int argc, char *argv[]) {
    if (argc == 2) {
        shell.interactivo = 0;
        if ((shell.flujo = fopen(argv[1], "r")) == NULL) {
            fprintf(stderr, "%s\n", error_message);
            exit(1);
        }
    }
}

void imprimir_prompt(int interactivo) {
    if (interactivo) {
        printf("wish> ");
    }
}

int separar_comandos(char cmd[]) {
    char *comando = strtok(cmd, "&");
    int k = 0;
    while (comando != NULL) {
        shell.comandos[k] = comando;
        comando = strtok(NULL, "&");
        k++;
    }
    return k;
}

void dividir_comando(int p) {
    char *arg = strtok(shell.comandos[p], " \t\n");
    int i = 0;
    while (arg != NULL) {
        shell.argumentos[i] = arg;
        arg = strtok(NULL, " \t\n");
        i++;
    }
    shell.argumentos[i] = NULL;
}

void verificar_redireccion() {
    int j;
    shell.multiplesArchivos = 0;
    shell.redireccionSalida = 0;
    for (j = 0; shell.argumentos[j] != NULL; j++) {
        if ((strcmp(shell.argumentos[j], ">") == 0) && (strcmp(shell.argumentos[0], ">") != 0)) {
            if (shell.argumentos[j + 2] != NULL) {
                shell.multiplesArchivos = 1;
            } else {
                shell.redireccionSalida = 1;
                shell.archivoSalida = shell.argumentos[j + 1];
                shell.argumentos[j] = NULL;
            }
        } else {
            char *pos = strchr(shell.argumentos[j], '>');
            if (pos) {
                if (shell.argumentos[j + 1] != NULL) {
                    shell.multiplesArchivos = 1;
                } else {
                    char *arg = strtok(shell.argumentos[j], ">");
                    shell.argumentos[j] = arg;
                    shell.redireccionSalida = 1;
                    arg = strtok(NULL, ">");
                    shell.archivoSalida = arg;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char cmd[MAX_CMD_LEN];
    char *path = getenv("PATH");
    char *pathCopy = strdup(path);
    int num_cmd = 0;
    int breakLoop = 0;

    inicializarShell();

    if (argc > 2) {
        fprintf(stderr, "%s\n", error_message);
        free(pathCopy);
        exit(1);
    }

    verificar_modo_bash(argc, argv);

    while (1) {
        // Obtener entrada del usuario
        imprimir_prompt(shell.interactivo);

        if (fgets(cmd, MAX_CMD_LEN, shell.flujo) == NULL) {
            free(pathCopy);
            exit(0);
        }

        // Eliminar el carácter de nueva línea del final de la entrada
        cmd[strcspn(cmd, "\n")] = '\0';
        if (strlen(cmd) == 0) {
            continue;
        }

        num_cmd = separar_comandos(cmd);

        shell.comandoActual = 0;
        while (shell.comandoActual < num_cmd) {
            // Dividir la entrada en un array de argumentos
            dividir_comando(shell.comandoActual);

            if (shell.argumentos[0] == NULL) {
                shell.comandoActual++;
                continue;
            }

            verificar_redireccion();

            if (shell.multiplesArchivos) {
                fprintf(stderr, "%s\n", error_message);
                shell.comandoActual++;
                continue;
            }

            // Manejar el comando 'cd' por separado usando chdir()
            if (strcmp(shell.argumentos[0], "exit") == 0) {
                if (shell.argumentos[1] != NULL) {
                    fprintf(stderr, "%s\n", error_message);
                } else {
                    breakLoop = 1;
                    break;
                }
            } else if (strcmp(shell.argumentos[0], "cd") == 0) {
                ejecutar_cd(shell.argumentos);
            }

            // Manejar el comando 'path' por separado
            else if (strcmp(shell.argumentos[0], "path") == 0) {
                if (shell.argumentos[1] == NULL) {
                    shell.noPermitido = 1;
                } else {
                    pathCopy = ejecutar_path(shell.argumentos, path);
                }
            } else if (!shell.noPermitido) {
                // Fork de un proceso hijo para ejecutar el comando
                pid_t pid = fork();

                if (pid == 0) {
                    // Proceso hijo: ejecutar el comando
                    char *fullpath;
                    char *token;
                    int i = 0;
                    int encontrado = 0;
                    while (!encontrado && (token = strtok(pathCopy, ":")) != NULL) {
                        i++;
                        pathCopy = NULL;
                        fullpath = malloc(strlen(token) + strlen(shell.argumentos[0]) + 2);
                        sprintf(fullpath, "%s/%s", token, shell.argumentos[0]);
                        if (access(fullpath, X_OK) == 0) {
                            if (shell.redireccionSalida) {
                                int output_fd = open(shell.archivoSalida, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                                if (output_fd == -1) {
                                    fprintf(stderr, "%s\n", error_message);
                                    free(pathCopy);
                                    exit(EXIT_FAILURE);
                                }
                                if (dup2(output_fd, STDOUT_FILENO) == -1) {
                                    fprintf(stderr, "%s\n", error_message);
                                    free(pathCopy);
                                    exit(EXIT_FAILURE);
                                }
                            }
                            execv(fullpath, shell.argumentos);
                            perror(shell.argumentos[0]);
                            free(fullpath);
                            free(pathCopy);
                            exit(EXIT_FAILURE);
                        }
                        free(fullpath);
                    }
                    if (i == 0) {
                        fprintf(stderr, "%s\n", error_message);
                    } else {
                        fprintf(stderr, "%s\n", error_message);
                    }
                    free(pathCopy);
                    exit(EXIT_FAILURE);
                } else if (pid < 0) {
                    // Error en el fork
                    fprintf(stderr, "%s\n", error_message);
                }
            } else {
                fprintf(stderr, "%s\n", error_message);
            }
            shell.comandoActual++;
        }
        if (breakLoop) {
            break;
        }
        while (wait(NULL) > 0);
    }
    free(pathCopy);
    return 0;
}