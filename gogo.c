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

//TAG
#define TAG 22
pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t groupMoneyMutex = PTHREAD_MUTEX_INITIALIZER;


//ZMIENNE WPOLDZILEONE
int money;
int groupMoney;
int approveCount;
int status;
int *tab;
int N;
int M;
int K;
int clubNumber;
int rank;
long lamportClock; //dopisać inkrementowanie zegara
MPI_Datatype mpi_data;

typedef struct data_s {
        int lamportClock;
        int message;
        int rank;
        int clubNumber;
        int money;
} data;

typedef int bool;
enum { false, true };

bool isSomeoneToAsk(){
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

int max(int a, int b){
	if(a>b)
		return a;
	else
		return b;
}

void *ThreadBehavior()
{
    data recv;
    data send;
    while(true){
        //1
        MPI_Recv(&recv, 1, mpi_data, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	//printf("[%d][lamport: %d]Otrzymalem wiadomosc od %d, nr wiad: %d, nr klubu: %d, money: %d, moj status: %d\n",rank, lamportClock,recv.rank, recv.message, recv.clubNumber, recv.money, status);
        lamportClock = max(recv.lamportClock, lamportClock) + 1;
       // pthread_mutex_lock(&statusMutex);

        //2
        if(status == ENOUGH_MONEY && recv.message == ENTER_CLUB_QUERY){
            if(recv.clubNumber != clubNumber){
                lamportClock++;
                send.lamportClock = lamportClock;
                send.message = ENTER_PERMISSION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                send.money = money;
                MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
		printf("[%d][%d]Pozwolenie na wejscie do klubu o nr: %d dla RANK: %d\n",rank, lamportClock, recv.clubNumber, recv.rank);
            }
            else{
                if(recv.lamportClock < lamportClock){
                    lamportClock++;
                    send.lamportClock = lamportClock;
                    send.message = ENTER_PERMISSION;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    send.money = money;
                    MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
		    printf("[%d][%d]Pozwolenie na wejscie do klubu o nr: %d dla RANK: %d\n",rank, lamportClock, recv.clubNumber, recv.rank);
                }
            }
        }

        //3
        if((status != NO_GROUP && status != GROUP_BREAK)&& recv.message == GROUP_INVITE){
            lamportClock++;
            send.lamportClock = lamportClock;
            send.message = REJECT_INVITE_MSG;
            send.rank = rank;
            send.clubNumber = clubNumber;
            send.money = money;
            MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
	    printf("[%d][%d]Odrzucenie proponowanej grupy od RANK: %d\n",rank, lamportClock, recv.rank);
        }

        //4
        if(status == FOUNDER && recv.message == GROUP_CONFIRMATION){
            pthread_mutex_lock(&groupMoneyMutex);
            groupMoney += recv.money;
            pthread_mutex_unlock(&groupMoneyMutex);
            *(tab+recv.rank) = MY_GROUP;
            status = ACCEPT_INVITE;
	    printf("[%d][%d]RANK: %d dalacza do grupy!\n",rank, lamportClock, recv.rank);
	    printf("[%d][%d]Jestem kapitanem, mamy na razie: %d a potrzeba %d pieniedzy.\n", rank, lamportClock, groupMoney, M);
        }

        //5
        if(status == FOUNDER && recv.message == REJECT_INVITE_MSG){
            *(tab+recv.rank) = NOT_MY_GROUP;
            status = REJECT_INVITE;
	    printf("[%d][%d]Odrzucenie proponowanej grupy od RANK: %d(jestem Kapitanem)\n",rank, lamportClock, recv.rank);
        }

        //6
        if(status == ENOUGH_MONEY && recv.message == ENTER_PERMISSION){
            approveCount++;
	    printf("[%d][%d]Pozwolenie dla mnie na wejscie do klubu o nr: %d od RANK: %d\n",rank, lamportClock, clubNumber, recv.rank);
            if(approveCount == N-1){
                status = ENTER_CLUB;
            }
        }

        //7
        if(status != ENOUGH_MONEY && status != ENTER_CLUB && recv.message == ENTER_CLUB_QUERY){
            lamportClock++;
            send.lamportClock = lamportClock;
            send.message = ENTER_PERMISSION;
            send.rank = rank;
            send.clubNumber = clubNumber;
            send.money = money;
            MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
	    printf("[%d][%d]Pozwolenie na wejscie do klubu o nr: %d dla RANK: %d\n",rank, lamportClock, recv.clubNumber, recv.rank);
        }

        //8
        if((status == NO_GROUP || status == GROUP_BREAK) && recv.message == GROUP_INVITE){
            if(recv.lamportClock < lamportClock){
                status = PARTICIPATOR;
                lamportClock++;
                send.lamportClock = lamportClock;
                send.message = GROUP_CONFIRMATION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                send.money = money;
                MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
		printf("[%d][%d]Akceptuje zaproszenie do grupy od RANK: %d\n",rank, lamportClock, recv.rank);
            }
            else{
                lamportClock++;
                send.lamportClock = lamportClock;
                send.message = REJECT_INVITE_MSG;
                send.rank = rank;
                send.clubNumber = clubNumber;
                send.money = money;
                MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
		printf("[%d][%d]Odrzucam zaproszenie do grupy od RANK: %d\n",rank, lamportClock, recv.rank);
            }
        }

        //9
        if(status == NO_GROUP && recv.message == GROUP_CONFIRMATION){
            status = ACCEPT_INVITE;
            *(tab+recv.rank) = MY_GROUP;
            pthread_mutex_lock(&groupMoneyMutex);
            groupMoney += recv.money;
            pthread_mutex_unlock(&groupMoneyMutex);
	    printf("[%d][%d]RANK: %d dalacza do grupy!\n",rank, lamportClock,  recv.rank);
	    printf("[%d][%d]Jestem kapitanem, mamy na razie: %d a potrzeba %d pieniedzy.\n", rank, lamportClock,  groupMoney, M);
        }

        //10
        if(status == NO_GROUP && recv.message == REJECT_INVITE_MSG){
            status = GROUP_BREAK;
            *(tab+recv.rank) = NOT_MY_GROUP;
	    printf("[%d][%d]Moje zaproszenie zostalo odrzucone od RANK: %d\n",rank, lamportClock,  recv.rank);
        }

        //11
        if(status == PARTICIPATOR && recv.message == GROUP_CONFIRMATION){
            lamportClock++;
            send.lamportClock = lamportClock;
            send.message = GROUP_BREAK_MSG;
            send.rank = rank;
            send.clubNumber = clubNumber;
            send.money = money;
            MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
	    printf("[%d][%d]Zrywam grupe z RANK: %d (moje zaproszenie jest juz nie aktualne)\n",rank, lamportClock, recv.rank);
        }

        //12
        if(status == PARTICIPATOR && recv.message == GROUP_BREAK_MSG){
            status = GROUP_BREAK;
	    printf("[%d][%d]Grupa zostala rozwiazana przez RANK: %d!\n",rank, lamportClock, recv.rank);
        }

        //13
        if(recv.message == EXIT_CLUB_MSG){
	      clubNumber = recv.clubNumber;
              printf("[%d][%d]        Wychodze z klubu jako czlonek grupy! Nr klubu: %d\n", rank, lamportClock, clubNumber);
	      status = EXIT_CLUB;
        }

        //14
        if(status == ENTER_CLUB && recv.message == ENTER_CLUB_QUERY){
            if(recv.clubNumber != clubNumber){
                lamportClock++;
                send.lamportClock = lamportClock;
                send.message = ENTER_PERMISSION;
                send.rank = rank;
                send.clubNumber = clubNumber;
                send.money = money;
                MPI_Send(&send, 1, mpi_data, recv.rank, TAG, MPI_COMM_WORLD);
		printf("[%d][%d]Pozwolenie na wejscie do klubu o nr: %d  dla RANK: %d\n",rank, lamportClock, recv.rank,  recv.clubNumber);
            }
        }
	
       // pthread_mutex_unlock(&statusMutex);
	//sleep(1);
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


	const int nitems = 5;
	int blocklengths[5] = {1, 1, 1, 1, 1};
	MPI_Datatype types[5] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
	MPI_Aint offsets[5];

	offsets[0] = offsetof(data, lamportClock);
	offsets[1] = offsetof(data, message);
	offsets[2] = offsetof(data, rank);
	offsets[3] = offsetof(data, clubNumber);
	offsets[4] = offsetof(data, money);

    MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_data);
	MPI_Type_commit(&mpi_data);
    srand( time(0) + rank );
	createThread();
	lamportClock = rand() % 4;
    while(true){
        bool restart = false;
        //Zmienne wspoldzielone
	int r = rand() % 4;
        sleep(r);
        //lamportClock = r;
        money = rand() % (M - 2) + 1;
        groupMoney = money;
        approveCount = 0;
        status = NO_GROUP;
        clubNumber = -1;
        tab = calloc(N, sizeof(int));
        for(int i=0;i<N;i++){
            *(tab+i) = NOT_ASKED;
            if(i == rank)
                *(tab+i) = MY_GROUP;
        }


        data send;
        while(isSomeoneToAsk()){
            lamportClock++;
            send.lamportClock = lamportClock;
            send.message = GROUP_INVITE;
            send.rank = rank;
            send.clubNumber = clubNumber;
            send.money = money;
	    int random = getRandomFreeElder();
            MPI_Send(&send, 1, mpi_data, random, TAG, MPI_COMM_WORLD);
	    printf("[%d][%d]        Zapytanie o dolaczenie do grupy dla RANK: %d\n",rank, lamportClock,  random);
            while(status == NO_GROUP || status == PARTICIPATOR || status == FOUNDER){
                //waiting for status update
            }
            //pthread_mutex_lock(&statusMutex);
            if(status == ACCEPT_INVITE){
                status = FOUNDER;
                if(groupMoney >= M){
			printf("[%d][%d]        Mamy wystarczajaca ilosc pieniedzy(mamy: %d, wymagane: %d)! Przechodze do wyboru klubu. \n", rank, lamportClock,  groupMoney, M);
		  	break;
		}

            }
            if(status == REJECT_INVITE){
                status = FOUNDER;
            }
            if(status == GROUP_BREAK){
		groupMoney = money;
                status = NO_GROUP;
            }
            if(status == EXIT_CLUB){
		printf("[%d][%d]        Wychodze jako czlonek grupy z klubu o nr: %d\n", rank, lamportClock,  clubNumber);
                restart = true;
                break;
            }
            //pthread_mutex_unlock(&statusMutex);
        }
        //Wychodzimy z klubu (dla emerytów nie będących założycielami)
        if(!restart){

        //Jeżeli za mało pieniędzy oznacza że zapytał wszystkich i nie da rady więc rozwiązuje grupę
        if(groupMoney < M && status == FOUNDER){
            for(int i=0;i<N;i++){
                if(*(tab+i) == MY_GROUP && i != rank){
                    lamportClock++;
                    send.lamportClock = lamportClock;
                    send.message = GROUP_BREAK_MSG;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    send.money = money;
                    MPI_Send(&send, 1, mpi_data, i, TAG, MPI_COMM_WORLD); //Wyślij do wszystkich którzy są w mojej grupie (oprócz mnie)
		    printf("[%d][%d]        Rozwiazanie grupy dla RANK: %d\n",rank, lamportClock, i);
                }
            }
        }

        //Jeżeli mamy siano i możemy ubiegać się o wejście
        if(groupMoney >= M && status == FOUNDER){
	    printf("[%d][%d]        Wybieramy klub!\n", rank, lamportClock);
            //pthread_mutex_lock(&statusMutex);
            status = ENOUGH_MONEY;
            //pthread_mutex_unlock(&statusMutex);
            clubNumber = rand() % K;
	    printf("[%d][%d]        Wybralismy klub o nr: %d\n", rank, lamportClock,  clubNumber);
            for(int i=0;i<N;i++){
              	if(i != rank){
		lamportClock++;
                send.lamportClock = lamportClock;
                send.message = ENTER_CLUB_QUERY;
                send.rank = rank;
                send.clubNumber = clubNumber;
                send.money = money;
                MPI_Send(&send, 1, mpi_data, i, TAG, MPI_COMM_WORLD); //Wyślij do wszystkich zapytanie o wejście do klubu
		printf("[%d][%d]        Zapytanie o wejscie do klubu o nr: %d dla RANK: %d\n",rank, lamportClock, clubNumber, i);
            	}
	    }
	    printf("[%d][%d]        Czekamy na pozwolenia na wejscie do klubu o nr: %d\n", rank, lamportClock,  clubNumber);
            while(status != ENTER_CLUB){
                //waiting for perrmisions to go to club
            }

	    printf("[%d][%d]       Mamy pozwolenie na wejscie do klubu o nr: %d\n", rank, lamportClock,  clubNumber);
            if(status == ENTER_CLUB){
                //sleep(2);
                for(int i=0;i<N;i++){
                    if(*(tab+i) == MY_GROUP && i != rank){
                        lamportClock++;
                        send.lamportClock = lamportClock;
                        send.message = EXIT_CLUB_MSG;
                        send.rank = rank;
                        send.clubNumber = clubNumber;
                        send.money = money;
                        MPI_Send(&send, 1, mpi_data, i, TAG, MPI_COMM_WORLD); //Wyślij do wszystkich którzy są w mojej grupie info o wyjściu z klubu
			printf("[%d][%d]        Informacja --> Koniec imprezy dla RANK: %d\n",rank, lamportClock,i);
                    }
                }
                for(int i=0;i<N;i++){
		if(i != rank && *(tab+i) != MY_GROUP) {
                    lamportClock++;
                    send.lamportClock = lamportClock;
                    send.message = ENTER_PERMISSION;
                    send.rank = rank;
                    send.clubNumber = clubNumber;
                    send.money = money;
                    MPI_Send(&send, 1, mpi_data, i, TAG, MPI_COMM_WORLD); //Wyślij do wszystkich info o możliwości wejścia do klubu w którym byliśmy
		    printf("[%d][%d]        Pozwolenie na wejscie do naszego klubu (nr: %d) dla RANK: %d\n",rank, lamportClock, clubNumber, i);
		}
		}
            }

	printf("[%d][%d]        Kapitan wychodzi z klubu o nr: %d\n", rank, lamportClock, clubNumber);
        }
	}
    }

	MPI_Type_free(&mpi_data);
    MPI_Finalize();

    return 0;

}
