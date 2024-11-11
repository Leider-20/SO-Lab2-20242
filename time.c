#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


//Escriba un programa en C llamado time.c que determine la cantidad de tiempo necesaria para correr un comando desde la línea de comandos. 
//Este programa será ejecutado como "time <command>" y mostrará la cantidad de tiempo gastada para ejecutar el comando especificado. 

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <comando>\n", argv[0]);
        return 1;
    }

    struct timeval inicio, fin;

    // Se obtiene el tiempo de inicio
    if (gettimeofday(&inicio, NULL) != 0) {
        perror("Error al obtener el tiempo de inicio");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al hacer fork");
        return 1;
    } else if (pid == 0) {
        // Proceso hijo que ejecuta el comando especificado con execvp
        execvp(argv[1], &argv[1]);
        perror("Error al ejecutar el comando");  // En caso de fallo
        exit(1);
    } else {
        // Proceso padre que espera a que termine el hijo
        int estado;
        waitpid(pid, &estado, 0);

        // Se obtiene el tiempo final
        if (gettimeofday(&fin, NULL) != 0) {
            perror("Error al obtener el tiempo final");
            return 1;
        }

        // Se calcula el tiempo transcurrido en segundos y microsegundos
        double tiempo_transcurrido = (fin.tv_sec - inicio.tv_sec) + (fin.tv_usec - inicio.tv_usec) / 1000000.0;
        printf("Tiempo transcurrido: %.5f segundos\n", tiempo_transcurrido);
    }

    return 0;
}
