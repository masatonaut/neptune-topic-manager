#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h> 
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/msg.h> 
#include <wait.h> 

int pipefd[2];  //pipe from student to neptun  
int pipefd2[2]; //pipe from neptun to advisor 
int messg; //for msg queue
char *s; //for shared memory
int sh_mem_id;
sem_t* semid; //for semaphore
char* sem_name="mango";

void handler(int signumber) {
    printf("\nSignal with number %i has arrived\n", signumber);
}

struct messg { 
     long mtype;//this is a free value e.g for the address of the message
     char mtext [ 1024 ]; //this is the message itself
}; 

int send( int mqueue ) 
{ 
     const struct messg m = { 5, "What technology would you like to use to implement your project?" }; 
     int status;      
     status = msgsnd( mqueue, &m, strlen ( m.mtext ) + 1 , 0 ); 
     if ( status < 0 ) 
          perror("msgsnd error"); 
     return 0; 
} 
     
int receive( int mqueue ) 
{ 
     struct messg m; 
     int status; 
     status = msgrcv(mqueue, &m, 1024, 5, 0 ); 
     if ( status < 0 ) 
          perror("msgsnd error"); 
     else
          printf( "student got a question from advisor: %s\n", m.mtext ); 
     return 0; 
}

sem_t* semaphore_create(char*name,int semaphore_value) 
{
// creates a new semaphore or opens an existing one with the specified initial value 
// (semaphore_value). It uses sem_open to achieve this.
    sem_t *semid=sem_open(name,O_CREAT,S_IRUSR|S_IWUSR,semaphore_value );
	if (semid==SEM_FAILED)
	perror("sem_open");
       
    return semid;
}

void semaphore_delete(char* name)
{
      sem_unlink(name);
}

// 2) The thesis advisor asks a clarifying question to the student through a message queue, "What technology would you like to use to
//  implement your project?". The student does not respond, considering what would be the right choice for them.
// The question received by the student is displayed on the screen.

void neptun()
{
     pause(); //wait for signals from child processes
     pause();
     printf("both the student and the thesis advisor have successfully logged in\n");

     close(pipefd[1]);
     char upload[120];
     read(pipefd[0], upload, sizeof(upload));
     close(pipefd[0]);

     close(pipefd2[0]);
     write(pipefd2[1], upload, sizeof(upload));
     close(pipefd2[1]);

     int status; //which status child finished 
     wait(&status);
     wait(&status);

     status = msgctl( messg, IPC_RMID, NULL );  //remove the message queue
    if ( status < 0 ) 
        perror("msgctl error"); 
     semaphore_delete(sem_name); //delete sem
     status = shmctl(sh_mem_id,IPC_RMID,NULL); //remove sharer memory
     if ( status < 0 ) 
        perror("shmctl error"); 
}

void student()
{
     sleep(1);
     kill(getppid(), SIGTERM); 

     close(pipefd[0]);
     char upload[120] = "Thesis title, Student's name, Year of submission, Advisor's name\n";
     write(pipefd[1], upload, sizeof(upload));
     close(pipefd[1]);

     close(pipefd2[1]);
     close(pipefd2[0]);

    //get question from advisor
    receive( messg ); 

    //get chance from advisor
     sleep(1);	              
     /*  critical section   */
//	printf("Child tries to close semaphore!\n");
	sem_wait(semid);	// semaphore down - Decrements (locks) the semaphore. If the semaphore's value is zero, 
	//the calling process blocks until the semaphore count is greater than zero
     printf("Student got decision from advisor: %s",s);
     sem_post(semid);      
     /*  end of critical section  */  
     shmdt(s); 	// release the memory 
}

// 3) Subsequently, the thesis advisor decides whether to accept the topic or not. There is a 20% chance that the advisor rejects 
// the supervision, writes this decision into shared memory which the student reads, and then this result is displayed on the screen.
// 4) Protect the use of shared memory with a semaphore.
void advisor()
{
     sleep(2);
     kill(getppid(), SIGUSR1);  

    close(pipefd[1]);
    close(pipefd[0]);

    close(pipefd2[1]);
     char upload[120];
    read(pipefd2[0], upload, sizeof(upload));
    close(pipefd2[0]);

    printf("advisor received from neptun: %s", upload);

    send( messg ); 

     //20% chance that the advisor rejects 
     srand(getpid());

    // Generate a random number and check for 20% chance
    char* chance;
    if (rand() % 100 < 20) {
         chance= "Accepted! \n";   
    } else {
        chance= "Rejected! \n";   
    }
     
   //  printf("Advisor writes into the shared memory: %s\n",chance);                
     strcpy(s,chance); // copy the value into the shared memory
   //  printf("Advisor: semaphore up!\n");
     sem_post(semid); //semaphore up
     shmdt(s);	    // release the memory
}

// We are soon approaching the period for declaring thesis topics. In the Neptun system (parent), an option will be available for the student 
// (child) to upload their topic and invite a thesis advisor (child) who can either accept to supervise the topic or not.

// 1) This is a busy period of the semester, so both the student and the thesis advisor log into Neptun to handle the administrative tasks of 
// topic declarations. Both the student and the thesis advisor send a signal to Neptun that they have logged in, which Neptun "acknowledges" 
// and displays on the screen that both the student and the thesis advisor have successfully logged in. Subsequently, the student "uploads" 
// - sends through a pipe to Neptun - the declaration document of their thesis topic, which contains the following data: Thesis title, Student
// 's name, Year of submission, Advisor's name. Neptun then forwards this to the thesis advisor also through a pipe, who displays these 
// details on the screen. Neptun waits until both the student and the thesis advisor have finished their administrative activities. 
// (The parent waits for the child processes to finish and maintains the connection throughout.)

int main(int argc,char** args)
{   
    //create pipes
     if (pipe(pipefd) < 0) {
        perror("Pipe failed");
        exit(1);
     }

     if (pipe(pipefd2) < 0) {
        perror("Pipe failed");
        exit(1);
     }

    //child
     pid_t child=fork(); 

    //get signals
     signal(SIGTERM, handler); 
     signal(SIGUSR1, handler);

     //create msg queue
     key_t key; 
     key = ftok("exam",1); //generates unique key 
     messg = msgget( key, 0600 | IPC_CREAT ); 
     if ( messg < 0 ) { 
          perror("msgget error"); 
          return 1; 
     } 

     //create shared memory
     sh_mem_id=shmget(key,500,IPC_CREAT|S_IRUSR|S_IWUSR); // create shared memory for reading and writing (500 bytes )
     s = shmat(sh_mem_id,NULL,0); // to connect the shared memory

     //create semaphore
     semid = semaphore_create(sem_name,0); 

     if (child>0)
     { 
        pid_t child2=fork();
        if (child2>0)
        {
            neptun();    
        }
        else 
        {
            advisor();
        }
     }
     else 
     {
        student();
     }
     return 0;
} 
