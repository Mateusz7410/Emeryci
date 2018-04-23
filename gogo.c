#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <mpi.h>
#include <unistd.h>

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

pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t groupMoneyMutex = PTHREAD_MUTEX_INITIALIZER;


//ZMIENNE WPOLDZILEONE
int money;
int groupMoney;
int approveCount;
int status;
int tab*;
int N;
int M;
int K;
int clubNumber;
int rank;
long lamportClock;

typedef struct data_s {
        int lamportClock;
        int message;
        int rank;
        int clubNumber;
} data;

void isSomeoneToAsk(){
	for(int i=0;i<N;i++){
		if(*(tab+i)==0){
			return true;
		}
	}
	return false;
}

int getRandomFreeElder(){
	int rnd = rand() % N;
	if(*(tab+rnd)==0)
		return rnd;
	for(int i=rnd;i<N;i++){
		if(*(tab+i)==0){
			return i;
		}
	}
	for(int i=0;i<N;i++){
		if(*(tab+i)==0){
			return i;
		}
	}
	return -1;
}

void *ThreadBehavior()
{
    data recv;
    data send;
    while(true){
        //1
        MPI_Recv(&recv, 1, data_s, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORD, MPI_STATUS_IGNORE);
        pthread_mutex_lock(&statusMutex);

        //2
        if(status == ENOUGH_MONEY && recv.message == ENTER_CLUB_QUERY){
            if(recv.clubNumber != clubNumber){
                send.lamportClock = lamportClock;
                send.message = ENTER_PERMISSION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
            }
            else{
                if(recv.lamportClock < lamportClock){
                    send.lamportClock = lamportClock;
                    send.message = ENTER_PERMISSION;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
                }
            }
        }

        //3
        if(status != NO_GROUP && recv.message == GROUP_INVITE){
            send.lamportClock = lamportClock;
            send.message = REJECT_INVITE_MSG;
            send.rank = rank;
            send.clubNumber = clubNumber;
            MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
        }

        //4
        if(status == FOUNDER && recv.message == GROUP_CONFIRMATION){
            pthread_mutex_lock(&groupMoneyMutex);
            groupMoney += recv.lamportClock;
            pthread_mutex_unlock(&groupMoneyMutex);
            *(tab+recv.rank) = MY_GROUP;
            status = ACCEPT_INVITE;
        }

        //5
        if(status == FOUNDER && recv.message == REJECT_INVITE_MSG){
            *(tab+recv.rank) = NOT_MY_GROUP;
            status = REJECT_INVITE;
        }

        //6
        if(status == FOUNDER && recv.message == ENTER_PERMISSION){
            approveCount++;
            if(approveCount == N-1){
                status = ENTER_CLUB;
            }
        }

        //7
        if(status != ENOUGH_MONEY && status != ENTER_CLUB && recv.message == ENTER_CLUB_QUERY){
            send.lamportClock = lamportClock;
            send.message = ENTER_PERMISSION;
            send.rank = rank;
            send.clubNumber = clubNumber;
            MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
        }

        //8
        if(status == NO_GROUP && recv.message == GROUP_INVITE){
            if(recv.lamportClock < lamportClock){
                status = PARTICIPATOR;
                send.lamportClock = lamportClock;
                send.message = GROUP_CONFIRMATION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
            }
            else{
                send.lamportClock = lamportClock;
                send.message = REJECT_INVITE_MSG;
                send.rank = rank;
                send.clubNumber = clubNumber;
                MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
            }
        }

        //9
        if(status == NO_GROUP && recv.message == GROUP_CONFIRMATION){
            status = FOUNDER;
            *(tab+recv.rank) = MY_GROUP;
            pthread_mutex_lock(&groupMoneyMutex);
            groupMoney += recv.lamportClock;
            pthread_mutex_unlock(&groupMoneyMutex);
        }

        //10
        if(status == NO_GROUP && recv.message == REJECT_INVITE_MSG){
            status = GROUP_BREAK;
            *(tab+recv.rank) = NOT_MY_GROUP;
        }

        //11
        if(status == PARTICIPATOR && recv.message == GROUP_CONFIRMATION){
            send.lamportClock = lamportClock;
            send.message = GROUP_BREAK_MSG;
            send.rank = rank;
            send.clubNumber = clubNumber;
            MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
        }

        //12
        if(status == PARTICIPATOR && recv.message == GROUP_BREAK_MSG){
            status = GROUP_BREAK;
        }

        //13
        if(status == PARTICIPATOR && recv.message == EXIT_CLUB_MSG){
            status = EXIT_CLUB;
        }

        //14
        if(status == ENTER_CLUB && recv.message == ENTER_CLUB_QUERY){
            if(recv.clubNumber != clubNumber){
                send.lamportClock = lamportClock;
                send.message = ENTER_PERMISSION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                MPI_Send(&send, 1, data_s, recv.rank, MPI_ANY_TAG, MPI_COMM_WORLD);
            }
        }

        pthread_mutex_unlock(&statusMutex);
    }

    pthread_exit(NULL);
}


void createThread() {
    int create_result = 0;

    pthread_t thread1;

    create_result = pthread_create(&thread1, NULL, ThreadBehavior, NULL);
    if (create_result){
       printf("Błąd przy próbie utworzenia wątku, kod błędu: %d\n", create_result);
       exit(-1);
    }

}




int main (int argc, char *argv[])
{
    M = 100;
    K = 4;

    MPI_Init(&argc, &argv);

    MPI_Comm_size( MPI_COMM_WORLD, &N );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    srand( rank );

    while(true){
        bool restart = false;
        //Zmienne wspoldzielone
        sleep(rand%3);
        lamportClock = 0;
        money = rand() % (M - 1) + 1;
        groupMoney = 0;
        approveCount = 0;
        status = NO_GROUP;
        clubNumber = -1;
        tab = callock(N, sizeof(int));
        for(int i=0;i<N;i++){
            *(tab+i) = NOT_ASKED;
            if(i == rank)
                *(tab+i) = MY_GROUP;
        }


        createThread();
        data send;
        while(isSomeoneToAsk()){
            send.lamportClock = lamportClock;
            send.message = GROUP_INVITE;
            send.rank = rank;
            send.clubNumber = clubNumber;
            MPI_Send(&send, 1, data_s, getRandomFreeElder(), MPI_ANY_TAG, MPI_COMM_WORLD);
            while(status == NO_GROUP || status == PARTICIPATOR){
                //waiting for status update
            }
            pthread_mutex_lock(&statusMutex);
            if(status == ACCEPT_INVITE){
                status = FOUNDER;
                if(groupMoney >= M)
                    break;
            }
            if(status == REJECT_INVITE){
                status = FOUNDER;
            }
            if(status == GROUP_BREAK){
                status = NO_GROUP;
            }
            if(status == EXIT_CLUB){
                restart = true;
                break;
            }
            pthread_mutex_unlock(&statusMutex);
        }
        //Wychodzimy z klubu (dla emerytów nie będących założycielami)
        if(restart)
            break;

        //Jeżeli za mało pieniędzy oznacza że zapytał wszystkich i nie da rady więc rozwiązuje grupę
        if(groupMoney < M && status == FOUNDER){
            for(int i=0;i<N;i++){
                if(*(tab+i) == MY_GROUP && i != rank){
                    send.lamportClock = lamportClock;
                    send.message = GROUP_BREAK_MSG;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    MPI_Send(&send, 1, data_s, i, MPI_ANY_TAG, MPI_COMM_WORLD); //Wyślij do wszystkich którzy są w mojej grupie (oprócz mnie)
                }
            }
            break;
        }

        //Jeżeli mamy siano i możemy ubiegać się o wejście
        if(groupMoney >= M && status == FOUNDER){
            pthread_mutex_lock(&statusMutex);
            status = ENOUGH_MONEY;
            pthread_mutex_unlock(&statusMutex);
            clubNumber = rand() % K;
            for(int i=0;i<N;i++){
                send.lamportClock = lamportClock;
                send.message = ENTER_CLUB_QUERY;
                send.rank = rank;
                send.clubNumber = clubNumber;
                MPI_Send(&send, 1, data_s, i, MPI_ANY_TAG, MPI_COMM_WORLD); //Wyślij do wszystkich zapytanie o wejście do klubu
            }
            while(status != ENTER_CLUB){
                //waiting for perrmisions to go to club
            }

            if(status == ENTER_CLUB){
                sleep(3);
                for(int i=0;i<N;i++){
                    if(*(tab+i) == MY_GROUP && i != rank){
                        send.lamportClock = lamportClock;
                        send.message = EXIT_CLUB_MSG;
                        send.rank = rank;
                        send.clubNumber = clubNumber;
                        MPI_Send(&send, 1, data_s, i, MPI_ANY_TAG, MPI_COMM_WORLD); //Wyślij do wszystkich którzy są w mojej grupie info o wyjściu z klubu
                    }
                }
                for(int i=0;i<N;i++){
                    send.lamportClock = lamportClock;
                    send.message = ENTER_PERMISSION;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    MPI_Send(&send, 1, data_s, i, MPI_ANY_TAG, MPI_COMM_WORLD); //Wyślij do wszystkich info o możliwości wejścia do klubu w którym byliśmy
                }
            }

        }
    }


    MPI_Finalize();

    return 0;

}
