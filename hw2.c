#include <sys/stat.h>

#include <sys/types.h>

#include <sys/mman.h>

#include <string.h>

#include <stdlib.h>

#include <stdio.h>

#include <fcntl.h>

#include <errno.h>

#include <sys/wait.h>

#include <signal.h>

#include <stdbool.h>

#include <unistd.h>

#include <time.h>

#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS (O_WRONLY | O_APPEND)
#define PERMS (S_IRUSR | S_IWUSR | S_IWGRP)

#define FIFO1 "fifo1.%ld"
#define FIFO2 "fifo2.%ld"
#define FIFO_NAME_LEN (sizeof(FIFO1) + 20)

int result = 0;
int counter = 0;

void signal_handler(int signum) {
        int status;
        pid_t pid;

        while ((pid = waitpid(-1, & status, WNOHANG)) > 0) {
                if (WIFEXITED(status)) {
                        printf("Child process with PID %d exited normally with status %d\n", pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                        printf("Child process with PID %d terminated by signal %d\n", pid, WTERMSIG(status));
                }

                counter += 1;
        }

        if (pid == -1 && counter == 2 && WIFEXITED(status)) {
                printf("Parent process exited normally with status %d\n", WEXITSTATUS(status));
        }
}

void alarm_handler(int signum) {
        printf("Proceeding...\n");
}

double op(int length, int * array, char * operation) {

        double res = 0.0;

        if (strcmp(operation, "sum") == 0) {

                for (int i = 0; i < length; i++) {
                        res += array[i];
                }

                return res;

        } else if (strcmp(operation, "mul") == 0) {

                res = 1.0;

                for (int i = 0; i < length; i++) {
                        res *= array[i];
                }

                return res;

        } else if (strcmp(operation, "ave") == 0) {

                for (int i = 0; i < length; i++) {
                        res += array[i];
                }

                res /= length;

                return res;

        } else {

                printf("\nThis operation isn't valid: %s\n", operation);
                signal_handler(1);
                exit(1);
        }

}

int main(int argc, char * argv[]) {

        int length;
        char * operation;
        char op_read[4];

        pid_t pid = getpid();

        if (argc != 3) {
                perror("\nFormat: ./hw2 array_length_as_integer operation");
                return 1;
        }

        if (argv[1]) {
                length = atoi(argv[1]);
        }

        if (argv[2]) {
                operation = argv[2];
        }

        struct sigaction sa;
        memset( & sa, 0, sizeof(sa));
        sa.sa_handler = & signal_handler;
        sigaction(SIGCHLD, & sa, NULL);

        struct sigaction sa_alarm;
        memset( & sa_alarm, 0, sizeof(sa_alarm));
        sa_alarm.sa_handler = & alarm_handler;
        sigaction(SIGALRM, & sa_alarm, NULL);

        sigset_t alarm_mask;
        sigemptyset( & alarm_mask);
        sigaddset( & alarm_mask, SIGALRM);
        sigprocmask(SIG_BLOCK, & alarm_mask, NULL);

        int * random = malloc(length * sizeof(int));

        if (random == NULL) {
                printf("Memory allocation failed\n");
                return 1;
        }

        srand(time(NULL));

        for (int i = 0; i < length; i++) {
                random[i] = rand() % 100;
        }

        printf("\nThe random array of length %d: ", length);

        for (int i = 0; i < length; i++) {
                printf("%d ", random[i]);
        }

        printf("\n");

        int fifo1, fifo2;
        char fifo1name[FIFO_NAME_LEN];
        char fifo2name[FIFO_NAME_LEN];

        snprintf(fifo1name, FIFO_NAME_LEN, FIFO1, (long) pid);
        snprintf(fifo2name, FIFO_NAME_LEN, FIFO2, (long) pid);

        ssize_t bytesRead;

        umask(0);

        if (mkfifo(fifo1name, PERMS) == -1 && errno != EEXIST) {
                perror("\nFIFO1 Error\n");
                exit(EXIT_FAILURE);
        }
        if (mkfifo(fifo2name, PERMS) == -1 && errno != EEXIST) {
                perror("\nFIFO2 Error\n");
                exit(EXIT_FAILURE);
        }

        pid_t child1, child2;

        child1 = fork();

        if (child1 > 0) {

                setbuf(stdout, NULL);

                fflush(stdout);

                alarm(2);

                fifo1 = open(fifo1name, WRITE_FLAGS, PERMS);
                if (fifo1 == -1) {
                        perror("Failed to open FIFO1");
                        exit(EXIT_FAILURE);
                } else {
                        printf("\nParent: FIFO1 opened successfully.\n");
                }

                fflush(stdout);

                if (write(fifo1, random, length * sizeof(int)) != length * sizeof(int)) {
                        perror("\nFIFO1 write error\n");
                        exit(EXIT_FAILURE);
                }

                if (unlink(fifo1name) == -1) {
                        exit(1);
                }

                if (close(fifo1) == -1) {
                        perror("\nFIFO1 close error\n");
                        exit(EXIT_FAILURE);
                }

                setbuf(stdout, NULL);

                fflush(stdout);

                sleep(10);

                fifo2 = open(fifo2name, WRITE_FLAGS, PERMS);
                if (fifo2 == -1) {
                        perror("Failed to open FIFO2");
                        exit(EXIT_FAILURE);
                } else {
                        printf("\nParent: FIFO2 opened successfully.\n");
                }

                fflush(stdout);

                printf("\nParent: Operation '%s' written to FIFO2.\n", operation);

                if (write(fifo2, operation, strlen(operation)) != strlen(operation)) {
                        perror("\nFIFO2 write error\n");
                        exit(EXIT_FAILURE);
                }

                if (unlink(fifo2name) == -1) {
                        exit(1);
                }

                if (close(fifo2) == -1) {
                        perror("\nFIFO2 close error\n");
                        exit(EXIT_FAILURE);
                }

                sigprocmask(SIG_UNBLOCK, & alarm_mask, NULL);

                setbuf(stdout, NULL);

                fflush(stdout);

        } else if (child1 == -1) {
                perror("Could not create child1");
                exit(1);
        } else if (child1 == 0) {

                child2 = fork();

                if (child2 > 0) {

                        sleep(10);

                        fflush(stdout);

                        fifo1 = open(fifo1name, READ_FLAGS, PERMS);
                        if (fifo1 == -1) {
                                perror("Failed to open FIFO1");
                                exit(EXIT_FAILURE);
                        } else {
                                printf("\nChild1: FIFO1 opened successfully.\n");
                        }

                        fflush(stdout);

                        bytesRead = 0;

                        bytesRead = read(fifo1, random, length * sizeof(int));
                        if (bytesRead == -1) {
                                perror("Error reading from FIFO1");
                                return 1;
                        }

                        unlink(fifo1name);

                        if (close(fifo1) == -1) {
                                perror("\nFIFO1 close error\n");
                                exit(EXIT_FAILURE);
                        }

                        fflush(stdout);

                        for (int i = 0; i < length; i++) {
                                result += random[i];
                        }

                        fifo2 = open(fifo2name, WRITE_FLAGS, PERMS);

                        setbuf(stdout, NULL);

                        fflush(stdout);

                        if (fifo2 == -1) {
                                perror("Failed to open FIFO2");
                                exit(EXIT_FAILURE);
                        } else {
                                printf("\nChild1: FIFO2 opened successfully.\n");
                        }

                        if (write(fifo2, & result, sizeof(int)) != sizeof(int)) {
                                perror("\nFIFO2 write error\n");
                                exit(EXIT_FAILURE);
                        }

                        fflush(stdout);

                        if (unlink(fifo2name) == -1) {
                                exit(1);
                        }

                        if (close(fifo2) == -1) {
                                perror("\nFIFO2 close error\n");
                                exit(EXIT_FAILURE);
                        }

                        sleep(5);

                        exit(0);

                } else if (child2 == -1) {
                        perror("Could not create child2");
                        exit(1);
                } else if (child2 == 0) {

                        sleep(10);

                        fflush(stdout);

                        fifo1 = open(fifo1name, READ_FLAGS, PERMS);
                        if (fifo1 == -1) {
                                perror("Failed to open FIFO1");
                                exit(EXIT_FAILURE);
                        } else {
                                printf("\nChild2: FIFO1 opened successfully.\n");
                        }

                        fflush(stdout);

                        bytesRead = 0;

                        bytesRead = read(fifo1, random, length * sizeof(int));
                        if (bytesRead == -1) {
                                perror("Error reading from FIFO1");
                                return 1;
                        }

                        fflush(stdout);

                        unlink(fifo1name);

                        if (close(fifo1) == -1) {
                                perror("\nFIFO1 close error\n");
                                exit(EXIT_FAILURE);
                        }

                        setbuf(stdout, NULL);

                        fflush(stdout);

                        fifo2 = open(fifo2name, READ_FLAGS, PERMS);
                        if (fifo2 == -1) {
                                perror("Failed to open FIFO2");
                                exit(EXIT_FAILURE);
                        } else {
                                printf("\nChild2: FIFO2 opened successfully.\n");
                        }

                        sleep(5);

                        bytesRead = 0;

                        int * res = malloc(sizeof(int));

                        bytesRead = read(fifo2, res, sizeof(int));
                        if (bytesRead == -1) {
                                perror("Error reading from FIFO2");
                                return 1;
                        }

                        fflush(stdout);

                        printf("\nRESULT OF CHILD1: %d\n", * res);

                        free(res);

                        bytesRead = 0;

                        bytesRead = read(fifo2, op_read, sizeof(op_read));

                        if (bytesRead == -1) {
                                perror("Error reading from FIFO2");
                                return 1;
                        }

                        op_read[bytesRead] = '\0';

                        fflush(stdout);

                        printf("\nop: %s\n", op_read);

                        printf("\nRESULT OF CHILD2: %.2lf\n", op(length, random, op_read));

                        unlink(fifo2name);

                        if (close(fifo2) == -1) {
                                perror("\nFIFO2 close error\n");
                                exit(EXIT_FAILURE);
                        }

                        sleep(1);

                        exit(0);

                }

        }

        free(random);

        sleep(4);

}
