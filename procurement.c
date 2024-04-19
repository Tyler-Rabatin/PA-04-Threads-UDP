//---------------------------------------------------------------------
// Assignment : PA-03 UDP Single-Threaded Server
// Date       :
// Author     : Justin Bryan and Tyler Rabatin
// File Name  : procurement.c
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

#define MAXFACTORIES    20

typedef struct sockaddr SA ;

/*-------------------------------------------------------*/
int main( int argc , char *argv[] )
{
    int     numFactories ,      // Total Number of Factory Threads
            activeFactories ,   // How many are still alive and manufacturing parts
            iters[ MAXFACTORIES+1 ] = {0} ,  // num Iterations completed by each Factory
            partsMade[ MAXFACTORIES+1 ] = {0} , totalItems = 0;

    char  *myName = "Justin Bryan and Tyler Rabatin" ; 
    printf("\nPROCUREMENT: Started. Developed by %s\n\n" , myName );    
    fflush( stdout ) ;
    
    if ( argc < 4 )
    {
        printf("PROCUREMENT Usage: %s  <order_size> <FactoryServerIP>  <port>\n" , argv[0] );
        exit( -1 ) ;  
    }

    unsigned        orderSize  = atoi( argv[1] ) ;
    char	       *serverIP   = argv[2] ;
    unsigned short  port       = (unsigned short) atoi( argv[3] ) ;
 

    /* Set up local and remote sockets */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if( sd < 0 ) {
        printf("PROCUREMENT: error creating socket, exiting\n");
        perror("Failure: ");
        exit(EXIT_FAILURE);
    }

    // Prepare the server's socket address structure
    struct sockaddr_in srvrSkt;
    memset((void *) &srvrSkt, 0, sizeof(srvrSkt));
    srvrSkt.sin_family   = AF_INET;
    srvrSkt.sin_port     = htons( port ) ;
    if( inet_pton( AF_INET, serverIP , (void *) & srvrSkt.sin_addr.s_addr ) != 1 ) {
        printf( "PROCUREMENT: Invalid server IP address\n" );
        perror("Failure: ");
        close(sd);
        exit(EXIT_FAILURE);
    }
    printf("Attempting Factory server at '%s' : %d\n", serverIP, port);

    // Send the initial request to the Factory Server
    msgBuf  msg1;
    msg1.purpose = htonl(REQUEST_MSG);
    msg1.orderSize = htonl(orderSize);
    if( sendto(sd, &msg1, sizeof(msg1), 0, (SA *) &srvrSkt, sizeof(srvrSkt)) < 0 ) {
        printf("Error sending on procurement side\n");
        perror("Failure: ");
        close(sd);
        exit(EXIT_FAILURE);
    }

    printf("\nPROCUREMENT Sent this message to the FACTORY server: "  );
    printMsg( & msg1 );  puts("");

    /* Now, wait for oreder confirmation from the Factory server */
    msgBuf  msg2;
    printf ("\nPROCUREMENT is now waiting for order confirmation ...\n" );

    unsigned alen = sizeof( srvrSkt ) ;
    if( recvfrom(sd, &msg2, sizeof(msg2), 0, (SA *) &srvrSkt, &alen) < 0 ) {
        printf("Error receiving on procurement side\n");
        perror("Failure: ");
        close(sd);
        exit(EXIT_FAILURE);
    }

    printf("PROCUREMENT received this from the FACTORY server: "  );
    printMsg( & msg2 );  puts("\n");

    numFactories = ntohl(msg2.numFac);
    activeFactories = numFactories;

    int purpose;
    int factoryID;
    int made;
    // Monitor all Active Factory Lines & Collect Production Reports
    while ( activeFactories > 0 ) // wait for messages from sub-factories
    {
        msgBuf msg3;
        // First recive a message
        if( recvfrom(sd, &msg3, sizeof(msg3), 0, (SA *) &srvrSkt, &alen) < 0 ) {
            printf("Error receiving on procurement side\n");
            perror("Failure: ");
            close(sd);
            exit(EXIT_FAILURE);
        }
        purpose = ntohl(msg3.purpose);
        factoryID = ntohl(msg3.facID);

        // Inspect the incoming message
        switch(purpose){
            case PRODUCTION_MSG:
                made = ntohl(msg3.partsMade);
                // display message and update totals
                printf("PROCUREMENT: Factory #%-3d produced %-4d parts in %-5d milliSecs\n", factoryID, made, ntohl(msg3.duration));
                iters[factoryID - 1]++;
                partsMade[factoryID - 1] = partsMade[factoryID - 1] + made;
                break;
            case COMPLETION_MSG:
                // decrement activeFactories
                activeFactories--;
                printf("PROCUREMENT: Factory #%-3d\tCOMPLETED its task\n", factoryID);
                break;
            default:
                // if you recieve something else you can assume the program failed and you should terminate
                printf("PROCUREMENT recieved invalid message, something went wrong. Terminating.\n");
                close(sd);
                exit(EXIT_FAILURE);
                break;
        }
    } 

    // Print the summary report
    totalItems  = 0 ;
    printf("\n\n****** PROCUREMENT Summary Report ******\n");

    for(int i = 0; i < numFactories; i++) {
        printf("Factory # %2d made a total of %4d parts in %5d iterations\n" , i+1, partsMade[i], iters[i]);
        totalItems+= partsMade[i];
    }

    printf("==============================\n") ;

    printf("Grand total parts made = %5d   vs  order size of %5d\n", totalItems, orderSize);

    printf( "\n>>> Supervisor Terminated\n");

    close(sd);

    return 0 ;
}
