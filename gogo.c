#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <mpi.h>

//STATUS
#define NO_GROUP 0
#define FOUNDER 1
#define PARTICIPATOR -1
#define GROUP_BREAK 2
#define ENTER_CLUB 3
#define EXIT_CLUB 4
#define ACCEPT_INVITE 5
#define REJECT_INVITE 6
#define ENOUGH_MONEY 7

//MESSAGE
#define ENTER_CLUB_QUERY 0
#define GROUP_INVITE 1
#define GROUP_CONFIRMATION 2
#define GROUP_BREAK_MSG 3
#define REJECT_INVITE_MSG 4
#define ENTER_PERMISSION 5
#define EXIT_CLUB_MSG 6

//TAB
#define NOT_ASKED 0
#define MY_GROUP 1
#define NOT_MY_GROUP -1


//ZMIENNE WPOLDZILEONE
int money;
int groupMoney;
int approveCount;
int status;
int tab*;


struct thread_data_t
{
	//TODO
};


void *ThreadBehavior(void *t_data)
{
    struct thread_data_t *th_data = (struct thread_data_t*)t_data;


    pthread_exit(NULL);
}


void createThread() {
    int create_result = 0;

    pthread_t thread1;

    struct thread_data_t t_data;

    create_result = pthread_create(&thread1, NULL, ThreadBehavior, (void *)&t_data);
    if (create_result){
       printf("Błąd przy próbie utworzenia wątku, kod błędu: %d\n", create_result);
       exit(-1);
    }

}


int main (int argc, char *argv[])
{
    int N, M = 100, tid;

    MPI_Init(&argc, &argv);

    MPI_Comm_size( MPI_COMM_WORLD, &N );
    MPI_Comm_rank( MPI_COMM_WORLD, &tid );

    srand( tid );

    //Zmienne wspoldzielone
    money = rand() % (M - 1) + 1;
    groupMoney = 0;
    approveCount = 0;
    status = NO_GROUP;
    tab = callock(N, sizeof(int)); 
    for(int i=0;i<N;i++)
      *(tab+i) = 0;

    createThread();         
   

    MPI_Finalize();

    return 0;

}
