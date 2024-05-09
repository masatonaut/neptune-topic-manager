#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define BUFFER_SIZE 1024

void signal_student_handler(int sig) {
    printf("Student has logged in.\n");
}

void signal_supervisor_handler(int sig) {
    printf("Supervisor has logged in.\n");
}

int main() {
    int pipe_student_to_neptun[2];
    int pipe_neptun_to_supervisor[2];
    int pipe_supervisor_to_student[2];  // 新たに追加するパイプ
    pid_t student_pid, supervisor_pid;
    int shm_id;
    char *shm_ptr;

    // Create shared memory
    shm_id = shmget(IPC_PRIVATE, BUFFER_SIZE, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Failed to create shared memory segment");
        return 1;
    }
    shm_ptr = (char *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (char *) -1) {
        perror("Failed to attach shared memory segment");
        return 1;
    }

    if (pipe(pipe_student_to_neptun) == -1 || 
        pipe(pipe_neptun_to_supervisor) == -1 || 
        pipe(pipe_supervisor_to_student) == -1) {  // パイプの作成
        perror("Failed to create pipes");
        return 1;
    }

    // シグナルハンドラを設定
    signal(SIGUSR1, signal_student_handler);
    signal(SIGUSR2, signal_supervisor_handler);

    student_pid = fork();
    if (student_pid == 0) { // 学生プロセス
        sleep(1); // Ensure slight delay for realistic scenario
        close(pipe_student_to_neptun[0]); // Read端を閉じる
        close(pipe_supervisor_to_student[1]); // 新たに追加されたパイプのWrite端を閉じる
        kill(getppid(), SIGUSR1); // ログイン信号を送る

        char message[BUFFER_SIZE] = "Thesis Title: AI in Medicine, Student: Alice, Year: 2024, Supervisor: Dr. Bob, Neptune: ALICE10";
        write(pipe_student_to_neptun[1], message, strlen(message) + 1);
        close(pipe_student_to_neptun[1]); // 書き込み完了後、Write端を閉じる

        // 学生プロセスが問い合わせを受け取る部分
        char question[BUFFER_SIZE];
        read(pipe_supervisor_to_student[0], question, BUFFER_SIZE);
        printf("Student received question: %s\n", question);
        close(pipe_supervisor_to_student[0]); // 読み取り完了後、Read端を閉じる
        
        // Read the decision from shared memory
        printf("Student reads decision: %s\n", shm_ptr);
        shmdt(shm_ptr); // Detach shared memory

        exit(0);
    }

    supervisor_pid = fork();
    if (supervisor_pid == 0) { // 指導教員プロセス
        sleep(1); // Ensure slight delay for realistic scenario
        close(pipe_neptun_to_supervisor[1]); // Write端を閉じる
        close(pipe_supervisor_to_student[0]); // Read端を閉じる
        kill(getppid(), SIGUSR2); // ログイン信号を送る

        char read_message[BUFFER_SIZE];
        read(pipe_neptun_to_supervisor[0], read_message, BUFFER_SIZE);
        printf("Supervisor received: %s\n", read_message);
        close(pipe_neptun_to_supervisor[0]); // 読み取り完了後、Read端を閉じる

        srand(time(NULL)); // Initialize random seed
        int accept = rand() % 5; // Generate a number between 0 and 4
        if (accept == 0) {
            strcpy(shm_ptr, "Supervisor rejected the topic.");
        } else {
            strcpy(shm_ptr, "Supervisor accepted the topic.");
        }
        printf("Supervisor decision: %s\n", shm_ptr);

        // 問い合わせを学生に送る部分
        char message_to_student[BUFFER_SIZE] = "What technology would you like to use to implement your task?";
        write(pipe_supervisor_to_student[1], message_to_student, strlen(message_to_student) + 1);
        close(pipe_supervisor_to_student[1]); // 書き込み完了後、Write端を閉じる

        shmdt(shm_ptr); // Detach shared memory
        exit(0);
    }

    // ネプチューン(親プロセス)
    close(pipe_student_to_neptun[1]); // Write端を閉じる
    close(pipe_neptun_to_supervisor[0]); // Read端を閉じる
    close(pipe_supervisor_to_student[0]); // 追加されたパイプのRead端を閉じる
    close(pipe_supervisor_to_student[1]); // Write端を閉じる
    
    char buffer[BUFFER_SIZE];
    read(pipe_student_to_neptun[0], buffer, BUFFER_SIZE);
    printf("Neptun received and forwards to supervisor: %s\n", buffer);
    write(pipe_neptun_to_supervisor[1], buffer, strlen(buffer) + 1);
    close(pipe_student_to_neptun[0]); // 読み取り完了後、Read端を閉じる
    close(pipe_neptun_to_supervisor[1]); // 書き込み完了後、Write端を閉じる

    // 子プロセスの終了を待つ
    wait(NULL);
    wait(NULL);

    shmctl(shm_id, IPC_RMID, NULL); // Remove shared memory segment

    return 0;
}
