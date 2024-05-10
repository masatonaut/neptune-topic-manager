#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/msg.h>
#define MAX 256

void handler(int signumber)
{
    printf("Signal has arrived!\n");
}

struct Thesis
{
    char title[256];
    char student[256];
    char year[256];
    char supervisor[256];
} typedef Thesis;

struct message {
    long mtype;
    char mtext[256];
} typedef message;

void main(int argc, char **argv)
{
    srand(time(NULL));

    signal(SIGUSR1, handler);

    char buffer[MAX];
    char *msg = "Message";

    int ptoc[2]; // parent to child
    int ctop[2]; // child to parent

    key_t key = ftok("msgq", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);        

    if (pipe(ptoc) == -1)
    {
        perror("Error opening pipe!");
        exit(1);
    }
    if (pipe(ctop) == -1)
    {
        perror("Error opening pipe!");
        exit(1);
    }

    pid_t child1 = fork();
    if (child1 == 0)
    { // student
        sleep(1);
        kill(getppid(), SIGUSR1); // send signal to the parent
        Thesis thesis;
        strcpy(thesis.title, "Comparing World Cities");
        strcpy(thesis.student, "Jane Doe");
        strcpy(thesis.year, "2021");
        strcpy(thesis.supervisor, "Szabo David");
        write(ctop[1], &thesis, sizeof(thesis)); // send this to the parent
    
        close(ctop[1]);
        close(ctop[0]);
        message msg;
        msgrcv(msgid, &msg, sizeof(msg), 1, 0);
        printf("Supervisor asks: %s\n", msg.mtext);
        sleep(1);
        int shmid = shmget(key, sizeof(int), 0666 | IPC_CREAT);
        int *data = (int *)shmat(shmid, (void *)0, 0);
        if (*data == 0)
        {
            printf("Student: The supervisor did not undertake the thesis\n");
        }
        else
        {
            printf("Student: The supervisor undertook the thesis\n");
        }
    }
    else
    { // parent
        pid_t child2 = fork();
        if (child2 == 0)
        { // supervisor
            sleep(2);
            kill(getppid(), SIGUSR1); // send signal to the parent
            Thesis thesis;
            read(ptoc[0], &thesis, sizeof(thesis));
            printf("Supervisor received the thesis\n");
            printf("Thesis title: %s\n", thesis.title);
            printf("Student's name: %s\n", thesis.student);
            printf("Year: %s\n", thesis.year);
            printf("Supervisor's name: %s\n", thesis.supervisor); 
            close(ptoc[1]);
            close(ptoc[0]);
            message msg;
            msg.mtype = 1;
            strcpy(msg.mtext, "What technology would you like to use to implement your task?");
            msgsnd(msgid, &msg, sizeof(msg), 0);

            // decides whether to undertake it
            sleep(1);
            int random = rand() % 5;
            if (random == 0)
            {
                printf("The supervisor does not undertake the thesis\n");
            }
            else
            {
                printf("The supervisor undertakes the thesis\n");
            }
            int shmid = shmget(key, sizeof(int), 0666 | IPC_CREAT);
            int *data = (int *)shmat(shmid, (void *)0, 0);
            *data = random;
            shmdt(data);
        }
        else
        { // parent
            pause();
            pause();
            printf("Both student and supervisor have checked in\n");
            
            close(ctop[1]);
            Thesis thesis;
            read(ctop[0], &thesis, sizeof(thesis));
            printf("Neptun received the thesis\n");
        
            close(ctop[0]);

            write(ptoc[1], &thesis, sizeof(thesis)); // send this to the supervisor
            
            close(ptoc[1]);
            close(ptoc[0]);

            int status;
            waitpid(child1, &status, 0); // parent waits for the child to terminate
            waitpid(child2, &status, 0);
        }
    }
}
