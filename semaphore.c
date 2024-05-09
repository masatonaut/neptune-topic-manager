#include <stdio.h>          // Standard I/O operations
#include <unistd.h>         // Provides access to the POSIX operating system API
#include <stdlib.h>         // Standard library definitions
#include <string.h>         // String handling functions
#include <sys/wait.h>       // Declarations for waiting functions
#include <signal.h>         // Signal handling utilities
#include <sys/ipc.h>        // Interprocess communication access structure
#include <sys/shm.h>        // Shared memory facility
#include <sys/sem.h>        // Semaphore facility
#include <sys/msg.h>        // Message queue facility
#include <time.h>           // Time functions

#define BUFFER_SIZE 1024   // Define a buffer size for messages and shared memory

void signal_student_handler(int sig) {
    printf("Student has logged in.\n");  // Prints login confirmation for student
}

void signal_supervisor_handler(int sig) {
    printf("Supervisor has logged in.\n");  // Prints login confirmation for supervisor
}


// Semaphore union used to initialize the semaphore
// union semun {
//     int val;
//     struct semid_ds *buf;
//     unsigned short  *array;
// };

// P-operation (wait) on a semaphore
void p(int sem_id) {
    struct sembuf p_op = {0, -1, SEM_UNDO};  // Define a semaphore operation to decrement (wait)
    if (semop(sem_id, &p_op, 1) == -1) {     // Execute the semaphore operation
        perror("P-operation failed");
        exit(1);
    }
}

void v(int sem_id) {
    struct sembuf v_op = {0, 1, SEM_UNDO};   // Define a semaphore operation to increment (signal)
    if (semop(sem_id, &v_op, 1) == -1) {
        perror("V-operation failed");
        exit(1);
    }
}

struct messg {
    long mtype;         // Message type, a positive long used to identify messages
    char mtext[BUFFER_SIZE];  // Message text of BUFFER_SIZE
};


int main() {
    int pipe_student_to_neptun[2];
    int pipe_neptun_to_supervisor[2];
    pid_t student_pid, supervisor_pid;
    int shm_id, sem_id, msg_queue;
    char *shm_ptr;
    union semun sem_union;
    key_t key;

    // Create shared memory
    shm_id = shmget(IPC_PRIVATE, BUFFER_SIZE, IPC_CREAT | 0666);  // Allocate a shared memory segment
    if (shm_id < 0) {
        perror("Failed to create shared memory segment\n");
        return 1;
    }
    shm_ptr = (char *)shmat(shm_id, NULL, 0);  // Attach the shared memory segment to the process's address space
    if (shm_ptr == (char *) -1) {
        perror("Failed to attach shared memory segment\n");
        return 1;
    }

    // Create semaphore
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);  // Allocate a semaphore
    if (sem_id < 0) {
        perror("Failed to create semaphore\n");
        return 1;
    }
    sem_union.val = 1;  // Initialize semaphore value to 1
    if (semctl(sem_id, 0, SETVAL, sem_union) == -1) {
        perror("Failed to set semaphore value");
        return 1;
    }

    // Setup message queue
    key = ftok("progfile", 65);  // Generate a unique key for the message queue
    msg_queue = msgget(key, 0666 | IPC_CREAT);  // Create message queue
    if (msg_queue < 0) {
        perror("Failed to create message queue");
        return 1;
    }

    // Setup pipes
    if (pipe(pipe_student_to_neptun) == -1 || 
        pipe(pipe_neptun_to_supervisor) == -1) {  // Create two pipes for communication
        perror("Failed to create pipes");
        return 1;
    }

    // Setup signal handlers
    signal(SIGUSR1, signal_student_handler);
    signal(SIGUSR2, signal_supervisor_handler);

    // Create student process
    student_pid = fork();
    if (student_pid == 0) {  // Child process: student
        sleep(1);  // Delay to simulate realistic scenario
        close(pipe_student_to_neptun[0]);  // Close read end of pipe
        kill(getppid(), SIGUSR1);  // Send login signal to parent process

        // Send message through pipe to Neptun
        char message[BUFFER_SIZE] = "Thesis Title: AI in Medicine, Student: Alice, Year: 2024, Supervisor: Dr. Bob, Neptune: ALICE10\n";
        write(pipe_student_to_neptun[1], message, strlen(message) + 1);
        close(pipe_student_to_neptun[1]);  // Close write end of pipe after sending

        // Receive question from supervisor via message queue
        struct messg received_msg;
        if (msgrcv(msg_queue, &received_msg, sizeof(received_msg.mtext), 1, 0) < 0) {
            perror("Failed to receive message");
            exit(1);
        }
        printf("Student received question: %s\n", received_msg.mtext);

        // Access shared memory
        p(sem_id);  // Wait operation on semaphore
        printf("Student reads decision: %s\n", shm_ptr);
        v(sem_id);  // Signal operation on semaphore
        shmdt(shm_ptr);  // Detach shared memory

        exit(0);
    }

    // Create supervisor process
    supervisor_pid = fork();
    if (supervisor_pid == 0) {  // Child process: supervisor
        sleep(1);  // Delay for realistic timing
        close(pipe_neptun_to_supervisor[1]);  // Close write end of pipe
        kill(getppid(), SIGUSR2);  // Send login signal to parent

        // Read message from Neptun via pipe
        char read_message[BUFFER_SIZE];
        read(pipe_neptun_to_supervisor[0], read_message, BUFFER_SIZE);
        printf("Supervisor received: %s\n", read_message);
        close(pipe_neptun_to_supervisor[0]);  // Close read end of pipe after receiving

        srand(time(NULL));  // Seed for random number generator
        int accept = rand() % 5;  // Random decision, 20% chance to reject
        p(sem_id);  // Wait operation on semaphore before accessing shared memory
        if (accept == 0) {
            strcpy(shm_ptr, "Supervisor rejected the topic.\n");
        } else {
            strcpy(shm_ptr, "Supervisor accepted the topic.\n");
        }
        printf("Supervisor decision: %s\n", shm_ptr);
        v(sem_id);  // Signal operation on semaphore after accessing shared memory

        // Send question to student via message queue
        struct messg msg;
        msg.mtype = 1;  // Set message type to 1
        strcpy(msg.mtext, "What technology would you like to use to implement your task?\n");
        if (msgsnd(msg_queue, &msg, strlen(msg.mtext)+1, 0) < 0) {
            perror("Failed to send message");
            exit(1);
        }

        shmdt(shm_ptr); // Detach shared memory
        exit(0);
    }

    pause();
    pause();
    printf("Both student and supervisor have logged in\n");

    // Parent process: Neptun
    close(pipe_student_to_neptun[1]);  // Close write end of pipe to student
    close(pipe_neptun_to_supervisor[0]);  // Close read end of pipe from supervisor
    
    // Forward received message from student to supervisor
    char buffer[BUFFER_SIZE];
    read(pipe_student_to_neptun[0], buffer, BUFFER_SIZE);
    printf("Neptun received and forwards to supervisor: %s\n", buffer);
    write(pipe_neptun_to_supervisor[1], buffer, strlen(buffer) + 1);
    close(pipe_student_to_neptun[0]); // 読み取り完了後、Read端を閉じる
    close(pipe_neptun_to_supervisor[1]); // 書き込み完了後、Write端を閉じる

    // Wait for child processes to finish
    wait(NULL);
    wait(NULL);

    // Cleanup shared memory, semaphore, and message queue
    shmctl(shm_id, IPC_RMID, NULL);  // Remove shared memory segment
    semctl(sem_id, 0, IPC_RMID, sem_union);  // Remove semaphore
    msgctl(msg_queue, IPC_RMID, NULL);  // Remove the message queue

    return 0;
}
