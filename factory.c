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

#include <sys/stat.h>
#include <sys/wait.h>

#include "wrappers.h"
#include "message.h"

#define MAXSTR     200
#define IPSTRLEN    50

typedef struct sockaddr SA ;

int minimum( int a , int b)
{
    return ( a <= b ? a : b ) ; 
}

void subFactory( int factoryID , int myCapacity , int myDuration ) ;

void factLog( char *str )
{
    printf( "%s" , str );
    fflush( stdout ) ;
}

/*-------------------------------------------------------*/

// Global Variable for Future Thread to Shared
int   remainsToMake , // Must be protected by a Mutex
      actuallyMade ;  // Actually manufactured items

int   sd ;      // Server socket descriptor
struct sockaddr_in  
             srvrSkt,       /* the address of this server   */
             clntSkt;       /* remote client's socket       */

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
    //kill(getpid(), sig);
    close(sd);
    exit(1);
}


/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    char  *myName = "Justin Bryan and Tyler Rabatin" ; 
    unsigned short port = 50015 ;      /* service port number  */
    int    N = 1 ;                     /* Num threads serving the client */

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

      default:
        printf( "FACTORY Usage: %s [numThreads] [port]\n" , argv[0] );
        exit( 1 ) ;
    }

    // handling signals
    sigactionWrapper(SIGINT, goodbye);
    sigactionWrapper(SIGTERM, goodbye);

    // Set up server

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sd < 0) {
        printf("Error creating socket, shutting down\n");
        perror("Failure: ");
        exit(EXIT_FAILURE);
    }
    // Prepare server's socket address structure
    memset( (void *) &srvrSkt, 0, sizeof(srvrSkt));
    memset( (void *) &clntSkt, 0, sizeof(clntSkt));
    srvrSkt.sin_family = AF_INET;
    srvrSkt.sin_addr.s_addr = htonl( INADDR_ANY);
    srvrSkt.sin_port = htons(port);

    // bind server to socket
    if( bind(sd, (SA *) &srvrSkt, sizeof(srvrSkt) ) < 0) {
        printf("Error binding server to socket\n");
        perror("Failure: ");
        close(sd);
        exit(EXIT_FAILURE);
    }
    char ipStr[IPSTRLEN];
    if( inet_ntop( AF_INET, (void *) & srvrSkt.sin_addr.s_addr , ipStr , IPSTRLEN ) == NULL ) {
        printf("Error obtaining text of server IP\n");
        perror("Failure: ");
        close(sd);
        exit(EXIT_FAILURE);
    }
    printf("Bound socket %d to IP %s Port %d\n", sd, ipStr, ntohs(srvrSkt.sin_port));
    unsigned int alen = sizeof(clntSkt);

    msgBuf receiving;
    msgBuf sending;

    int forever = 1;
    int purpose;
    while ( forever )
    {
        printf( "\nFACTORY server waiting for Order Requests\n" ) ; 

        if(recvfrom(sd, &receiving, sizeof(receiving), 0, (SA *) &clntSkt, &alen) < 0) {
            printf("Error receiving on server side\n");
            perror("Failure: ");
            close(sd);
            exit(EXIT_FAILURE);
        }



        printf("\n\nFACTORY server received: " ) ;
        printMsg( & receiving );  puts("");
        if( inet_ntop( AF_INET, (void *) & clntSkt.sin_addr.s_addr , ipStr , IPSTRLEN ) == NULL ) {
            printf("Error obtaining text of client IP\n");
            perror("Failure: ");
            close(sd);
            exit(EXIT_FAILURE);
        }
        printf("\tFrom IP %s Port %d\n", ipStr, clntSkt.sin_port);
        // We are only checking to see if the send message is a REQUEST_MSG and getting
        // the size of the order
        purpose = ntohl(receiving.purpose);

        if(purpose = REQUEST_MSG) {
            remainsToMake = ntohl(receiving.orderSize);
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
            exit(EXIT_FAILURE);
        }
        subFactory( 1 , 50 , 350 ) ;  // Single factory, ID=1 , capacity=50, duration=350 msg
    }


    return 0 ;
}

void subFactory( int factoryID , int myCapacity , int myDuration )
{
    char    strBuff[ MAXSTR ] ;   // print buffer
    int     partsImade = 0 , myIterations = 0 , partsMadeThisIteration;
    msgBuf  msg;

    msg.facID = htonl(factoryID);
    msg.capacity = htonl(myCapacity);
    msg.duration = htonl(myDuration);

    while ( 1 )
    {
        // See if there are still any parts to manufacture
        if ( remainsToMake <= 0 )
            break ;   // Not anymore, exit the loop

        // decides how many to make as the lesser or capacity and remainsToMake
        if (remainsToMake < myCapacity) partsMadeThisIteration = remainsToMake;
        else partsMadeThisIteration = myCapacity;

        // decrements remainsToMake by the number of items this sub-factory will make
        remainsToMake -= partsMadeThisIteration;

        // sleep for duration milliseconds
        Usleep(myDuration / 1000);

        // Send a Production Message to Supervisor
        printf("Factory #%3d: Going to make %5d parts in %4d mSec\n", factoryID, partsMadeThisIteration, myDuration);
        msg.partsMade = htonl(partsMadeThisIteration);
        msg.purpose = htonl(PRODUCTION_MSG);
        if( sendto(sd, &msg, sizeof(msg), 0, (SA *) &clntSkt, sizeof(clntSkt)) < 0) {
            printf("Error sending on server side\n");
            perror("Failure: ");
            close(sd);
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
        exit(EXIT_FAILURE);
    }
    
    snprintf( strBuff , MAXSTR , ">>> Factory # %-3d: Terminating after making total of %-5d parts in %-4d iterations\n" 
          , factoryID, partsImade, myIterations);
    factLog( strBuff ) ;
    
}