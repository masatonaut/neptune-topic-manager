# neptune-topic-manager

We are soon approaching the period for declaring thesis topics. In Neptun (parent), an option will become available for the student (child) to upload their topic and invite a supervisor (child) who can either agree to oversee the topic or not.

1. This is a busy time of the semester, so both the student and the supervisor log into Neptun to manage the administrative tasks of topic declarations. Both the student and the supervisor send a signal to Neptun that they have logged in, which Neptun acknowledges and displays on the screen that both the student and the supervisor have successfully logged in. Afterward, the student 'uploads'—sends through a pipe to Neptun—the declaration document of their thesis topic, which includes the following data: Thesis title, Student name, Submission year, Supervisor's name. Neptun then forwards this information to the supervisor also through a pipe, who displays these details on the screen. Neptun waits until both the student and the supervisor have completed their administrative activities. (Parent waits for the child processes to end and maintains the connection throughout.)

2. The supervisor sends an interpretative question to the student through a message queue, asking, 'What technology would you like to use to implement your task?' The student does not respond, considering what the appropriate choice would be for them. The question received by the student is displayed on the screen.

3. Subsequently, the supervisor decides whether to accept the topic or not. There is a 20% probability that the supervisor will reject overseeing the topic. The decision is written into shared memory, which the student reads and then displays the result on the screen.

4. Protect the use of shared memory with a semaphore.
