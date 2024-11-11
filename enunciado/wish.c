#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define LONG_MAX_CMD 256
#define MAX_ARGS 64

char mensaje_error[] = "An error has occurred";

typedef struct {
    int modo_interactivo;
    char *lista_comandos[MAX_ARGS];
    char *lista_argumentos[MAX_ARGS];
    char *archivo_salida;
    int usar_redireccion;
    int numero_comando;
    FILE *entrada;
} InfoShell;

InfoShell shell;

void inicializarShell() {
    shell.modo_interactivo = 1;
    shell.usar_redireccion = 0;
    shell.numero_comando = 0;
    shell.entrada = stdin;
}

void cambiarDirectorio(char **arg) {
    if (chdir(arg[1]) == -1) {
        fprintf(stderr, "%s\n", mensaje_error);
    }
}

void salirShell(char **arg) {
    if (arg[1] != NULL) {
        fprintf(stderr, "%s\n", mensaje_error);
    } else {
        exit(0);
    }
}

void verificarModo(int argc, char *argv[]) {
    if (argc == 2) {
        shell.modo_interactivo = 0;
        if ((shell.entrada = fopen(argv[1], "r")) == NULL) {
            fprintf(stderr, "%s\n", mensaje_error);
            exit(1);
        }
    }
}

void imprimirPrompt() {
    if (shell.modo_interactivo) {
        printf("wish> ");
    }
}

int separarComandos(char comando[]) {
    char *partes = strtok(comando, "&");
    int k = 0;
    while (partes != NULL) {
        shell.lista_comandos[k] = partes;
        partes = strtok(NULL, "&");
        k++;
    }
    return k;
}

void dividirComando(int indice_comando) {
    char *arg = strtok(shell.lista_comandos[indice_comando], " \t\n");
    int i = 0;
    while (arg != NULL) {
        shell.lista_argumentos[i] = arg;
        arg = strtok(NULL, " \t\n");
        i++;
    }
    shell.lista_argumentos[i] = NULL;
}

void revisarRedireccion() {
    int j;
    shell.usar_redireccion = 0;
    for (j = 0; shell.lista_argumentos[j] != NULL; j++) {
        if (strcmp(shell.lista_argumentos[j], ">") == 0) {
            shell.usar_redireccion = 1;
            shell.archivo_salida = shell.lista_argumentos[j + 1];
            shell.lista_argumentos[j] = NULL;
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    char comando[LONG_MAX_CMD];
    char *ruta = getenv("PATH");
    char *copia_ruta = strdup(ruta);

    inicializarShell();

    if (argc > 2) {
        fprintf(stderr, "%s\n", mensaje_error);
        free(copia_ruta);
        exit(1);
    }

    verificarModo(argc, argv);

    while (1) {
        imprimirPrompt();

        if (fgets(comando, LONG_MAX_CMD, shell.entrada) == NULL) {
            free(copia_ruta);
            exit(0);
        }

        comando[strcspn(comando, "\n")] = '\0';
        if (strlen(comando) == 0) {
            continue;
        }

        int numero_comandos = separarComandos(comando);

        shell.numero_comando = 0;
        while (shell.numero_comando < numero_comandos) {
            dividirComando(shell.numero_comando);

            if (shell.lista_argumentos[0] == NULL) {
                shell.numero_comando++;
                continue;
            }

            revisarRedireccion();

            if (strcmp(shell.lista_argumentos[0], "exit") == 0) {
                salirShell(shell.lista_argumentos);
            } else if (strcmp(shell.lista_argumentos[0], "cd") == 0) {
                cambiarDirectorio(shell.lista_argumentos);
            } else {
                pid_t pid = fork();

                if (pid == 0) {
                    char *directorio;
                    char *token = strtok(copia_ruta, ":");
                    int comando_encontrado = 0;

                    while (token != NULL && !comando_encontrado) {
                        directorio = malloc(strlen(token) + strlen(shell.lista_argumentos[0]) + 2);
                        sprintf(directorio, "%s/%s", token, shell.lista_argumentos[0]);

                        if (access(directorio, X_OK) == 0) {
                            if (shell.usar_redireccion) {
                                int fd_salida = open(shell.archivo_salida, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                                if (fd_salida == -1 || dup2(fd_salida, STDOUT_FILENO) == -1) {
                                    fprintf(stderr, "%s\n", mensaje_error);
                                    exit(EXIT_FAILURE);
                                }
                            }
                            execv(directorio, shell.lista_argumentos);
                            perror(shell.lista_argumentos[0]);
                            free(directorio);
                            exit(EXIT_FAILURE);
                        }
                        free(directorio);
                        token = strtok(NULL, ":");
                    }
                    if (!comando_encontrado) {
                        fprintf(stderr, "%s\n", mensaje_error);
                    }
                    exit(EXIT_FAILURE);
                } else if (pid < 0) {
                    fprintf(stderr, "%s\n", mensaje_error);
                }
            }
            shell.numero_comando++;
        }
        while (wait(NULL) > 0);
    }
    free(copia_ruta);
    return 0;
}
