//---------------------------------------------------------------------
// Assignment : PA-04
// Date       :
// Author     : Justin Bryan and Tyler Rabatin
// File Name  : factory.c
//---------------------------------------------------------------------

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "wrappers.h"
#include "message.h"

#define MAXSTR     200
#define IPSTRLEN    50

typedef struct sockaddr SA ;

typedef struct 
{
    int id, capacity, duration;
} params_t;


int minimum( int a , int b)
{
    return ( a <= b ? a : b ) ; 
}

void *subFactory( void * ptr) ;

void factLog( char *str )
{
    printf( "%s" , str );
    fflush( stdout ) ;
}

/*-------------------------------------------------------*/

// Global Variable for Future Thread to Shared
int   remainsToMake , // Must be protected by a Mutex
      actuallyMade ,  // Actually manufactured items
      * eachPartsMade ,      // Hold the # of iterations for each sub-factory
      * iters ;         // Hold the # of iterations for each sub-factory

int   sd ;      // Server socket descriptor
struct sockaddr_in  
             srvrSkt,       /* the address of this server   */
             clntSkt;       /* remote client's socket       */
sem_t threadMutex;

//------------------------------------------------------------
//  Handle Ctrl-C or KILL 
//------------------------------------------------------------
void goodbye(int sig) 
{
    /* Mission Accomplished */
    printf( "\n### I (%d) have been nicely asked to TERMINATE. "
           "goodbye\n\n" , getpid() );  

    // missing code goes here
    fflush( stdout ) ;
    Sem_destroy(&threadMutex);
    close(sd);
    exit(1);
}


/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    char  *myName = "Justin Bryan and Tyler Rabatin" ,
          *serverIP ;                  /* the ipv4 address of this server */
    unsigned short port = 50015 ;      /* service port number  */
    int    N = 1 ;                     /* Num threads serving the client */
    char *ipArg = "0.0.0.0";           /* placeholder address*/
    struct timeval startTime, endTime;

    printf("\nThis is the FACTORY server developed by %s\n\n" , myName ) ;
	switch (argc) 
	{
      case 1:
        break ;     // use default port with a single factory thread
      
      case 2:
        N = atoi( argv[1] ); // get from command line
        port = 50015;            // use this port by default
        break;

      case 3:
        N    = atoi( argv[1] ) ; // get from command line
        port = atoi( argv[2] ) ; // use port from command line
        break;
      case 4:
        N    = atoi( argv[1] ) ; // get from command line
        serverIP = argv[2];      // use IP address from command line
        port = atoi( argv[3] ) ; // use port from command line
        break;

      default:
        printf( "FACTORY Usage: %s [numThreads] [port]\n" , argv[0] );
        exit( 1 ) ;
    }

    // handling signals
    sigactionWrapper(SIGINT, goodbye);
    sigactionWrapper(SIGTERM, goodbye);

    // mutex should start available
    Sem_init(&threadMutex, 0, 1);

    // Set up server
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        printf("Error creating socket, shutting down\n");
        perror("Failure: ");
        Sem_destroy(&threadMutex);
        exit(EXIT_FAILURE);
    }
    // Prepare server's socket address structure
    memset( (void *) &srvrSkt, 0, sizeof(srvrSkt));
    memset( (void *) &clntSkt, 0, sizeof(clntSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_port = htons(port);

    // use the host IP address if one was provided through args
    if (argc == 4) {
        // convert string in ipv4 format into usable binary
        if( inet_pton( AF_INET, serverIP , (void *) & srvrSkt.sin_addr.s_addr ) != 1 ) {
            printf( "PROCUREMENT: Invalid IP address\n" );
            perror("Failure: ");
            close(sd);
            exit(EXIT_FAILURE);
        }
    } else {
        srvrSkt.sin_addr.s_addr = htonl( INADDR_ANY);
    }

    // bind server to socket
    if( bind(sd, (SA *) &srvrSkt, sizeof(srvrSkt) ) < 0) {
        printf("Error binding server to socket\n");
        perror("Failure: ");
        close(sd);
        Sem_destroy(&threadMutex);
        exit(EXIT_FAILURE);
    }

    char ipStr[IPSTRLEN];
    if( inet_ntop( AF_INET, (void *) & srvrSkt.sin_addr.s_addr , ipStr , IPSTRLEN ) == NULL ) {
        printf("Error obtaining text of server IP\n");
        perror("Failure: ");
        close(sd);
        Sem_destroy(&threadMutex);
        exit(EXIT_FAILURE);
    }

    printf("I will attempt to accept orders at IP %s (%X): port %d and use %d sub-factories.\n\n", ipStr, ntohl(srvrSkt.sin_addr.s_addr), ntohs(srvrSkt.sin_port), N) ;


    printf("Bound socket %d to IP %s Port %d\n", sd, ipStr, ntohs(srvrSkt.sin_port));
    unsigned int alen = sizeof(clntSkt);

    msgBuf receiving;
    msgBuf sending;

    int forever = 1;
    int purpose;

    int capacity;
    int duration;
    int orderSize;
    srandom(time(NULL));
    pthread_t thrd[N + 1];
    params_t *argsPtr;
    while ( forever )
    {
        // initialize arrays holding parts made and iterations
        eachPartsMade    = malloc(N * sizeof(int));
        iters = malloc(N * sizeof(int));

        printf( "\nFACTORY server waiting for Order Requests\n" ) ; 

        if(recvfrom(sd, &receiving, sizeof(receiving), 0, (SA *) &clntSkt, &alen) < 0) {
            printf("Error receiving on server side\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }



        printf("\n\nFACTORY server received: " ) ;
        printMsg( & receiving );  puts("");
        if( inet_ntop( AF_INET, (void *) & clntSkt.sin_addr.s_addr , ipStr , IPSTRLEN ) == NULL ) {
            printf("Error obtaining text of client IP\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }
        printf("\tFrom IP %s Port %d\n", ipStr, clntSkt.sin_port);
        // We are only checking to see if the send message is a REQUEST_MSG and getting
        // the size of the order
        purpose = ntohl(receiving.purpose);

        if(purpose = REQUEST_MSG) {
            remainsToMake = ntohl(receiving.orderSize);
            orderSize = remainsToMake;
            sending.purpose = htonl(ORDR_CONFIRM);
            sending.numFac = htonl(N);
            
        } else {
            // If the first message is not a request then something went wrong, the printMsg function should voice that
            // purpose set to 0 since that is not within the purpose enum and will trigger invalid msg
            sending.purpose = htonl(0);
        }

        printf("\n\nFACTORY sent this Order Confirmation to the client " );
        printMsg(  & sending );  puts("");
        // now convert the info for network
        if( sendto(sd, &sending, sizeof(sending), 0, (SA *) &clntSkt, alen) < 0 ) {
            printf("Error sending on server side\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }
        if(gettimeofday(&startTime, NULL) < 0) {
            printf("Error getting start time of after confirming order\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }
        actuallyMade = 0;

        //subFactory( 1 , 50 , 350 ) ;  // Single factory, ID=1 , capacity=50, duration=350 msg
        // now spawn N sub-factory threads, capacity and duration should be randomized like in PA 2
        for(int i = 1; i <= N; i++) {
            capacity = (random() % 40) + 10;
            duration = (random() % 700) + 500;
            argsPtr = ( params_t *) malloc ( sizeof(params_t) ) ;
            if (!argsPtr) {
                printf("Out of memory\n");
                close(sd);
                Sem_destroy(&threadMutex);
                exit(EXIT_FAILURE);
            }
            argsPtr->id = i;
            argsPtr->capacity = capacity;
            argsPtr->duration = duration;
            Pthread_create( &thrd[i], NULL, subFactory, (void *) argsPtr);
        }

        // wait for subFactories threads to finish
        for(int i = 1; i <= N; i++) {
            Pthread_join(thrd[i], NULL);
        }
        if( gettimeofday(&endTime, NULL) < 0 ) {
            printf("Error getting start time of after completing order\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }

        // collect status
        int totalIterations = 0;
        for(int i = 0; i < N; i++) {
            totalIterations += iters[i];
        }

        // print summary report
        printf("\n****** FACTORY Server Summary Report ******\n");
        printf("    Sub-Factory\tParts Made\tIterations\n");
        for(int i = 1; i <= N; i++) {
            printf("\t%7d\t %d\t %9d\n", i, eachPartsMade[i - 1], iters[i - 1]);
        }
        printf("=======================================================\n");
        printf("Grand total parts made\t=\t%d\tvs\torder size of\t%d\n", actuallyMade, ntohl(receiving.orderSize));
        // convert start and end time to milliseconds, tv_sec is in seconds, and tv_usec is in microseconds
        double startTimeMilli = ((double)startTime.tv_sec * 1000) + ((double)startTime.tv_usec / 1000);
        double endTimeMilli = ((double)endTime.tv_sec * 1000) + ((double)endTime.tv_usec / 1000);
        // subtract the end time by the start time to find the elapsed time
        printf("Order-to-Completion time =\t%.1lf milliSeconds\n\n", endTimeMilli - startTimeMilli);
        // printf("WE STILL NEED TO FIND TIME IN MILLISECONDS\n((endTime - startTime) / CLOCKS_PER_SEC (from time.h))\n");

        free(eachPartsMade);
        free(iters);
    }




    return 0 ;
}

void *subFactory( void * ptr)
{
    char    strBuff[ MAXSTR ] ;   // print buffer
    int     partsImade = 0 , myIterations = 0 , partsMadeThisIteration;
    msgBuf  msg;

    params_t *args = (params_t *) ptr;
    int factoryID = args->id;
    int myCapacity = args->capacity;
    int myDuration = args->duration;
    // free args as I no  longer need it
    free(args);

    msg.facID = htonl(factoryID);
    msg.capacity = htonl(myCapacity);
    msg.duration = htonl(myDuration);


    while ( 1 )
    {
        // See if there are still any parts to manufacture
        if ( remainsToMake <= 0 )
            break ;   // Not anymore, exit the loop

        // remainsToMake is being referenced and caluclated here and is global.
        // Therefore a mutex is used here.
        Sem_wait(&threadMutex);

        // decides how many to make as the lesser or capacity and remainsToMake
        if (remainsToMake < myCapacity) partsMadeThisIteration = remainsToMake;
        else partsMadeThisIteration = myCapacity;

        // decrements remainsToMake by the number of items this sub-factory will make
        remainsToMake -= partsMadeThisIteration;
        // increments actuallyMade by the number of items this sub-factory will make
        actuallyMade += partsMadeThisIteration;
        Sem_post(&threadMutex);

        // sleep for duration milliseconds
        Usleep(myDuration * 1000);

        // Send a Production Message to Supervisor
        printf("Factory #%3d: Going to make %5d parts in %4d mSec\n", factoryID, partsMadeThisIteration, myDuration);
        msg.partsMade = htonl(partsMadeThisIteration);
        msg.purpose = htonl(PRODUCTION_MSG);
        if( sendto(sd, &msg, sizeof(msg), 0, (SA *) &clntSkt, sizeof(clntSkt)) < 0) {
            printf("Error sending on server side\n");
            perror("Failure: ");
            close(sd);
            Sem_destroy(&threadMutex);
            exit(EXIT_FAILURE);
        }

        // keep track of how many iterations it has performed
        myIterations++;
        // keep track of the total number of items it has has actually made
        partsImade += partsMadeThisIteration;
    }

    // Send a Completion Message to Supervisor
    msg.purpose = htonl(COMPLETION_MSG);
    if( sendto(sd, &msg, sizeof(msg), 0, (SA *) &clntSkt, sizeof(clntSkt)) < 0 ) {
        printf("Error sending on server side\n");
        perror("Failure: ");
        close(sd);
        Sem_destroy(&threadMutex);
        exit(EXIT_FAILURE);
    }
    
    snprintf( strBuff , MAXSTR , ">>> Factory # %-3d: Terminating after making total of %-5d parts in %-4d iterations\n" 
          , factoryID, partsImade, myIterations);
    factLog( strBuff ) ;
    // update status
    eachPartsMade[factoryID - 1] = partsImade;
    iters[factoryID - 1] = myIterations;
    pthread_exit(NULL);
    
}
