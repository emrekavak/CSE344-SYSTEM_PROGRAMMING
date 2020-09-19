// EMRE KAVAK 151044085 SYSTEM PROGAMMING FINAL PROJECT
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <sys/file.h>
#include <string.h>
#include <sys/time.h>
#include <time.h> 
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <signal.h>

#define IP "127.0.0.1"

struct threadTask{				// keep thread task. main thread load fd int his struct
	int id;
	int status;
	int socketFd; 
	int full;
	pthread_mutex_t mutex;	
	pthread_cond_t cond;
};

struct data{					// keep database datas
	long int start;
	long int destination;
	char *result;
};

struct dataBase{				// data base keep calculated paths
	long int addedCount;
	long int capacity;
	struct data* paths;
};

struct Queue {					// use in BFS search
	int reSizeCount;
    long int size;
    long int front;
    long int rear;
    long int* array;
    long int capacity; 
};


struct node { 					// adj list elements load 
    long id; 
    struct node* next; 
}; 
 
struct adjList { 				// keep graphs in linked list array
    struct node *head;  
}; 
  
struct Graph { 					// graph loaded this struct
    long int size;
    long int addedNodeCount; 
    struct adjList* array; 
};

/// Queue functions
struct Queue* createQueue(long int capacity);		// creates queue
int isEmpty(struct Queue* queue);				
int isFull(struct Queue* queue);
void enqueue(struct Queue* queue, long int item);	// add element into queue
long int dequeue(struct Queue* queue);
long int rear(struct Queue* queue);
long int front(struct Queue* queue);

/// graph functions
struct node* newNode(long int dest);				
void  createGraph(long int V);						// create graph
void addEdge(struct Graph* graph,long int src, long int dest);	// add element into graph

///////////////// adjacency list

void checkOneInstanceRunning();						// check program runned before or not with create locked pid file
void usage_print(char *logFileName, const char *msg);
void usage();

char* bfs(struct Graph *g, long int startNode, long int endNode);	// do bfs search into adj list
char *bfsRes=NULL;			// keeps bfs result
long int **paths=NULL;		// keeps calculated paths into bfs search
long int pathsSize;
long int *visited=NULL;

/* 	DATA BASE FUNCTIONS	*/

struct dataBase* db;									// db keep in this variable
void createDataBase(int capacity);
int addDataBase(long int s, long int d,char res[]);
long int getDbEntryIndex(long int n1, long int n2);		// if 0, entry does not exist, otherwise return entry index
char* getPath(long int index);							// if check not equal zero, get path from db according to index
long int getHashCode(long int value);					// return hash code according to given nodes sum
void expandDb(long cap);								// expand db if db full
/* 	DATA BASE END 	*/

void createTasksArr(int size);							// created thread task objects with ids
void writeLogFile(char* msg);

struct Graph* graph;				// keep graph
struct Queue* que;					// keep queu
struct threadTask* tasksArr;		// keep thread tasks

pthread_t *threadPool;				// keep server threads
pthread_t resizingThread;			// helper thread
void* threadFunc(void* id);
void* resizingThreadFunc(void* id);	// helper thread function

// reader/writer sync variables

int AR=0, WR=0,AW=0,WW=0;	// AR=ACTIVE READER - WR=WAIT READER - AW=ACTIVE WRITER - WW=WAIT WRITER 
pthread_mutex_t db_mutex=PTHREAD_MUTEX_INITIALIZER;		// db mutex
pthread_cond_t okToRead=PTHREAD_COND_INITIALIZER;		// reader mutex
pthread_cond_t okToWrite=PTHREAD_COND_INITIALIZER;		// writer mutex
pthread_mutex_t resize_mutex=PTHREAD_MUTEX_INITIALIZER;	// helper thread mutex
pthread_cond_t cond_resize=PTHREAD_COND_INITIALIZER;	// helper thread cond

long int reader(int t_id,long int start,long int end);
void writer(int t_id,long int start,long int end);

pthread_mutex_t main_mutex=PTHREAD_MUTEX_INITIALIZER;	// main thread mutex
pthread_mutex_t full_mutex=PTHREAD_MUTEX_INITIALIZER;	// main thread if all thread work, use this mutex
pthread_cond_t cond_full=PTHREAD_COND_INITIALIZER;		// cond for main thread full check
// counts variables
long int edgesCount=0;			// keep number of edges count
long int uniqSCount=0;			// keep number of start Node count
char* pathToLogFile;			// log file path
int logFileFd;					// keep log file's fd
int instantWorkerCount=0;		// keep instant thread count 
long int maxNodeId=0;			// keep max node id in graph	
long int minNodeId=2147483647;	// keep min node id in graph
int instantThreadCount=0;		// keep instant thread count 
int maxThreadCount=0;			// keep entered x count
long int *uniqSArr;				// keep start nodes
double loadPer=0;				// laoded thread percentage
long int **noteToNodeArr;		// when file lood, keeps edges for add graph 

void sighandler(int sig);		// handle SIGINT sıgnal
static volatile sig_atomic_t gotSIGINTSig = 0;				// for check SIGINT sıgnal sent

int main(int argc, char* argv[]){

	checkOneInstanceRunning(); 	//  Check for duplicate	
	pid_t pid = 0,snw = 0;
	
	pid = fork();				// Create child process
	if (pid < 0){				// Check Fail
		printf("Fork failed!\n");
		exit(EXIT_FAILURE);
	}
	if (pid > 0){
		exit(EXIT_SUCCESS);		// parent process exit
	}	
	umask(0);					// unmask file mode
	snw = setsid();				// set new session
	if(snw < 0){
		fprintf(stderr, "%s\n","setsid() error!");
		exit(EXIT_FAILURE);
	}

//////////////// child process area
	if(getpid() == syscall(SYS_gettid)){	// MAIN THREAD START

		int opt,s,x,indx=0;
		char *pathToFile;
		char* portNum;

		if(argc!=11){						// argument count check
			fprintf(stderr, "%s. Entered count: %d\n","Argument count should be 11",argc );
			usage();
			exit(EXIT_FAILURE);
		}
		while((opt = getopt (argc, argv,"i:p:o:s:x:")) != -1){		// argument chech
			switch(opt){
				case 'i':
					pathToFile=optarg;
					break;
				case 'p':
					portNum=optarg;
	                indx=0;
	                for (; indx<strlen(portNum); indx++){
	                    if (!isdigit(portNum[indx])){
	                        printf("\n -p %s argument not integer. It should be integer.\n\n",portNum);
	                        usage();
	                        exit(EXIT_FAILURE);
	                    }
	                }
					break;
				case 'o':
					pathToLogFile=optarg;
					break;
				case 's':
	                indx=0;
	                for(; indx<strlen(optarg); indx++){
	                    if (!isdigit(optarg[indx])){
	                        printf("\n -s %s argument not integer. It should be integer.\n\n",optarg);
	                        usage();
	                        exit(EXIT_FAILURE);
	                    }
	                }
					s=atoi(optarg);
					if(s<2){
					  	fprintf (stderr, "-s must be equal or grader than 2\n");
					  	usage();
					  	exit(EXIT_FAILURE);					
					}
					break;
				case 'x':
	                indx=0;
	                for(; indx<strlen(optarg); indx++){
	                    if (!isdigit(optarg[indx])){
	                        printf("\n -x %s argument not integer. It should be integer.\n\n",optarg);
	                        usage();
	                        exit(EXIT_FAILURE);
	                    }
	                }
					x=atoi(optarg);

					if(x<=s){
					  	fprintf (stderr, "-x must be grader than s parameter.\n");
					  	usage();
					  	exit(EXIT_FAILURE);
					}
					break;
				case '?':
						if (optopt == 'i'){
						  	fprintf (stderr, "Option -%c requires an input file path.\n", optopt);
						  	usage();
						  	exit(EXIT_FAILURE);
						}
						else if (optopt == 'p'){
						  	fprintf (stderr, "Option -%c equires a port number.\n", optopt);
						  	usage();
						  	exit(EXIT_FAILURE);
						}
						else if (optopt == 'o'){
						  	fprintf (stderr, "Option -%c requires a pathToLogFile argument.\n", optopt);
						  	usage();
						  	exit(EXIT_FAILURE);
						}
						else if (optopt == 's'){
						  	fprintf (stderr, "Option -%c requires number of threads in the pool at startup (at least 2).\n", optopt);
						  	usage();
						  	exit(EXIT_FAILURE);
						}
						else if (optopt == 'x'){
						  	fprintf (stderr, "Option -%c requires number maximum allowed number of threads.\n", optopt);
						  	usage();
						  	exit(EXIT_FAILURE);
						}
	  					else {
				  			fprintf (stderr,"Unknown option character %c\n", optopt);
							usage();
							exit(EXIT_FAILURE);
				  		}
				  		break;
	            default:
	            	usage();
	            	exit(EXIT_FAILURE);
	                break; 
			}
		}
		
		close(STDIN_FILENO);			// close inherited files
		close(STDOUT_FILENO);			
		close(STDERR_FILENO);
		int close_fd=0;
		close(close_fd);
		struct timeval  tm1, tm2;

		mode_t mode= S_IRUSR | S_IWUSR;
		logFileFd=open(pathToLogFile,O_CREAT|O_WRONLY|O_TRUNC,mode);	// path file open

		char *msg=" Executing with parameters:\n";						// writing argument informations
		writeLogFile(msg);

		char p1[512];
		sprintf(p1," -i %s\n",pathToFile);
		writeLogFile(p1);


		char p2[512];
		sprintf(p2," -p %s\n",portNum);
		writeLogFile(p2);

		char p3[512];
		sprintf(p3," -o %s\n",pathToLogFile);
		writeLogFile(p3);

		char p4[512];
		sprintf(p4," -s %d\n",s);
		writeLogFile(p4);

		char p5[512];
		sprintf(p5," -x %d\n",x);
		writeLogFile(p5);


		FILE* fp;
		int bufLength = 512;
		char buf[512];

		char* loMsg=" Loading graph...\n";
		writeLogFile(loMsg);

		fp = fopen(pathToFile, "r");
		if(fp==NULL){
			const char* err="pathToFile can not open!. Program finished.\n";
			write(logFileFd,err,strlen(err));
			exit(EXIT_FAILURE);
		}
		gettimeofday(&tm1, NULL);
		while(fgets(buf, bufLength, fp)) {			// first read graph file for calculate edges count
			if(buf[0]!='#'){
				if(strcmp(buf, "\n") != 0 && strcmp(buf,"\r\n") != 0)edgesCount++;	// check if line is not empty
			}
		}

		fclose(fp);
		long int i=0;

		uniqSArr=(long int*)malloc(sizeof(long int)*edgesCount);				// create unique start nodes array

	    noteToNodeArr= (long int **)malloc(edgesCount * sizeof(long int *)); 	// add nodes edges 
	    for (i=0; i<edgesCount; i++) 
	         noteToNodeArr[i] = malloc(2 * sizeof(long int));

		createDataBase(edgesCount*2);											// create database
		pathsSize=edgesCount*2;
		que = createQueue(pathsSize);

		paths=(long int **)malloc((pathsSize)*sizeof(long int)); 				// keep bfs paths
		for(i=0; i<pathsSize; i++) 
	     	paths[i] = (long int *)malloc(2*sizeof(long int));

		bfsRes=(char*)malloc(sizeof(char)*edgesCount);							// bfs result

		int fd =open(pathToFile, O_RDONLY);										// open file for get edges and calculate nodes count
		if(fd==-1){
			const char* err="pathToFile can not open!. Program finished.\n";
			write(logFileFd,err,strlen(err));
			exit(EXIT_FAILURE);
		}
		int sharpFlag=0, firstNode=0,secondNode=0, firstIndex=0,secondIndex=0, flag1=0, nArrIndex=0;
		char byteBuf[1],startNode[11],endNode[11];
		while(read(fd, byteBuf, 1)) {						// read file byte by byte and get start node to end node
			if(byteBuf[0]=='#') sharpFlag=1;

		    if(sharpFlag!=1){

		    	if(byteBuf[0]=='\t' || byteBuf[0]==' ') {
		    		secondNode=1;
		    		firstNode=0;
		    	}else
		    	if(secondNode!=1 && byteBuf[0]!=' ' && byteBuf[0]!='\n' && firstNode==1){
		    		startNode[firstIndex]=byteBuf[0];
		    		firstIndex++;
		    	}
		    	else
		    	if(secondNode==1 && byteBuf[0]!='\t'){
		    		endNode[secondIndex]=byteBuf[0];
		    		secondIndex++;
		    	}
		    	if(byteBuf[0]=='\n'){
		    		flag1=0;
		    		long int n1=atoi(startNode);	// get start node
		    		long int n2=atoi(endNode);		// get end node	
		    		if(uniqSCount==0){	
		    			uniqSArr[0]=n1;
		    			uniqSCount++;
		    		}else{
		    			for(i=0;i<uniqSCount;i++){
		    				if(uniqSArr[i]==n1) {
		    					flag1=1;
		    					i=uniqSCount;
		    				}

		    			}
		    			if(flag1==0) {
		    				uniqSArr[uniqSCount]=n1;	// and start nodes into array
		    				uniqSCount++;
		    			}
		    		}
		    	    if(minNodeId>n2) 	minNodeId=n2;
		    	    if(minNodeId>n1)	minNodeId=n1;

		    	    if(n2 > maxNodeId) 	maxNodeId=n2;
		    	    if(n1 > maxNodeId)	maxNodeId=n1;
		    	    noteToNodeArr[nArrIndex][0]=n1;		// add edges
		    	    noteToNodeArr[nArrIndex][1]=n2;
		    	    nArrIndex++;

		    		secondNode=0;
		    		firstIndex=0;
		    		secondIndex=0;
		    		firstNode=1;
		    	}
			}
			if(sharpFlag==1 && byteBuf[0]=='\n') {
				sharpFlag=0;
				firstNode=1;
			}

		}
		close(fd);
		createGraph(uniqSCount);
	 	visited=(long int*)malloc(edgesCount*sizeof(long int));	// initialize visited with vertes size

		gettimeofday(&tm2, NULL);
		double totalTime=(double) (tm2.tv_usec - tm1.tv_usec) / 1000000 +(double) (tm2.tv_sec - tm1.tv_sec);

		long int n1=0,n2=1;
		long int m=-1, temp;
		for(i=0,m=0;i<edgesCount;i++){			// add edges into graph n1 start node, n2 end node
			if(temp!=noteToNodeArr[i][n1]) m++;

			addEdge(graph,noteToNodeArr[i][n1],noteToNodeArr[i][n2]);
			temp=noteToNodeArr[i][n1];
		}

	////////////////////////////////////////////7 socket area

		gettimeofday(&tm2, NULL);
		totalTime=(double) (tm2.tv_usec - tm1.tv_usec) / 1000000 +(double) (tm2.tv_sec - tm1.tv_sec);

		char loMsg2[128];
		sprintf(loMsg2, " Graph loaded in %.1f seconds with %ld nodes and %ld edges\n",totalTime,maxNodeId+1,edgesCount);
		writeLogFile(loMsg2);
		
		maxThreadCount=x;
		instantThreadCount=s;
		if(pthread_create(&resizingThread, NULL, resizingThreadFunc,(void*)1)!=0){		// initiliaze helper thread check thread count percentage and increase if big or equal %75
				writeLogFile("Error in thread creation\n");
				exit(EXIT_FAILURE);
		}
		threadPool=(pthread_t*)malloc(sizeof(pthread_t)*instantThreadCount); 	/* Theread pool created in there*/
		createTasksArr(maxThreadCount);

		char msg3[40];
		sprintf(msg3," A pool of %d threads has been created\n",instantThreadCount);
		writeLogFile(msg3);

		for(i=0;i<s;i++){
			if(pthread_create(&threadPool[i], NULL, threadFunc,(void*)i)!=0){	// initiliaze threads
				writeLogFile("Error in thread creation\n");
				exit(EXIT_FAILURE);
			}
		}
	    int socketFd = 0;
	    struct sockaddr_in serv_addr; 

	    char resBuff[1025];

	    memset(&serv_addr, '0', sizeof(serv_addr));
	    memset(resBuff, '0', sizeof(resBuff)); 

	    if((socketFd = socket(AF_INET, SOCK_STREAM, 0))<0){		// socket initialize
	    	writeLogFile("socket error!\n");
	        exit(EXIT_FAILURE);
	    }
	    int PORT=atoi(portNum);

	    serv_addr.sin_family = AF_INET;
	    serv_addr.sin_addr.s_addr = inet_addr(IP);
	    serv_addr.sin_port = htons(PORT); 

	    if((bind(socketFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0){	// bind socket
	    	writeLogFile("bind error!\n");	
	        exit(EXIT_FAILURE);
	    } 

	    if((listen(socketFd, 20)) < 0){			// listen socket
	    	writeLogFile("listen error!\n");
	        exit(EXIT_FAILURE);
	    } 
		signal(SIGINT, sighandler);			// listen SIGCTRL
	
	    while(1){								// main thread manage accepts in infinite loop 
	        socklen_t len=sizeof(serv_addr);

	        int accptfd=accept(socketFd,(struct sockaddr*)&serv_addr, &len); 	// accept connects
	    	pthread_mutex_lock(&main_mutex);	// main thread mutex
	    	int foundFlag=0;
	        for(i=0; i<instantThreadCount;i++){	// after connect came, find not bust thread and assign job and send signal
	        	if(tasksArr[i].status==0){
	        		tasksArr[i].status=1;
	        		tasksArr[i].socketFd=accptfd;
	        		tasksArr[i].full=1;
	        		pthread_cond_signal(&tasksArr[i].cond);	// send signal to thread
	        		instantWorkerCount++;					// increase worker size
	        		char msgToFile[75]=" A connection has been delegated to thread id #";
	        		loadPer=((double)instantWorkerCount*100.0)/(double)instantThreadCount;		// calculate load percentage

	        		if(loadPer>=75.0) pthread_cond_signal(&cond_resize);	// if load bif or equatl %75, send signal resizing thread

	        		char tempL[30];
	        		sprintf(tempL,"%ld, system load %.1f%%\n",i,loadPer);
	        		strcat(msgToFile,tempL);
	        		writeLogFile(msgToFile);
	        		i=instantThreadCount;
	        		foundFlag=1;
	        	}
	        }
	    	pthread_mutex_unlock(&main_mutex);
			if(foundFlag==0){				// if not thread avaible, wait until one thread avaible threads send this cond signal
				pthread_mutex_lock(&full_mutex);
				writeLogFile(" No thread is available! Waiting for one.\n");
				while(instantWorkerCount==instantThreadCount-1){
					pthread_cond_wait(&cond_full,&main_mutex);
				}
				pthread_mutex_unlock(&full_mutex);
			}
			if(gotSIGINTSig==1) break;
	    }
	}
	return (0);
}

void* threadFunc(void* id){			// thread pool use this function
	int t_id=(int)id, i=0,bytes=0;	
	char info[40];
	sprintf(info," Thread #%d: waiting for connection\n",t_id);
	while(1){
		writeLogFile(info);

		if(gotSIGINTSig==1) {
			break;
		}
		char *st,*des;
		char buff[1025];
		pthread_mutex_lock(&tasksArr[t_id].mutex);

		if(tasksArr[t_id].full==0){		// if thread not has a job, wait cond signal
			while(tasksArr[t_id].full==0)
				pthread_cond_wait(&tasksArr[t_id].cond, &tasksArr[t_id].mutex);
				tasksArr[t_id].status=1;
		}

		if(gotSIGINTSig==0){
			if((bytes=recv(tasksArr[t_id].socketFd, buff, 1025, 0)) < 0){	// if jop came, read request
	       		writeLogFile("Receive failed!\n");
	    	}
	    	buff[bytes]='\0';

	        char* token = strtok(buff, "-");
	        i=0;
	        while(token != NULL){
	        	 if(i==0) st=token;
	        	 if(i==1) des=token;
	        	 i++;
	        	 token = strtok(NULL, "-"); 
	        }
	        long int start=atoi(st);	// convert nodes into long int
	        long int end=atoi(des);

	        char ans[100];
	        sprintf(ans," Thread #%d: searching database for a path from node %ld to node %ld\n",t_id,start,end);
	        writeLogFile(ans);

	        /// reader start
	        long int res=reader(t_id,start,end);	// first read db and send client if found
	        // writer start
	        if(res==-1){							// -1 means not node found, so thread fill calculate bfs and write it into db
				sprintf(ans," Thread #%d: no path in database, calculating %ld->%ld\n",t_id,start,end);
				writeLogFile(ans);
				writer(t_id,start,end);		
	    	}
	        close(tasksArr[t_id].socketFd);			// close assigned socket fd
	        pthread_mutex_lock(&main_mutex);
	        tasksArr[t_id].full=0;
	        tasksArr[t_id].status=0;
	        instantWorkerCount--;					// dcreace worker count
	        pthread_mutex_unlock(&main_mutex);
	        if(instantWorkerCount==instantThreadCount) pthread_cond_signal(&cond_full);	// send signal if all thread works
	        pthread_mutex_unlock(&tasksArr[t_id].mutex);
	    }

	}
	return 0;
}

void* resizingThreadFunc(void* id){				// wait signal from main thread and increase thread count
	while(instantThreadCount<maxThreadCount){
		pthread_mutex_lock(&resize_mutex);
		pthread_cond_wait(&cond_resize,&resize_mutex);

		pthread_mutex_lock(&main_mutex);
		loadPer=((double)instantWorkerCount*100.0)/(double)instantThreadCount;
		
		if(loadPer>=75.0){
			int passCount=instantThreadCount;
			instantThreadCount+=(instantThreadCount*25)/100;
			if(instantThreadCount==passCount) {
				instantThreadCount++;
				if(instantThreadCount>maxThreadCount) instantThreadCount=maxThreadCount;

			}
			if(instantThreadCount<=maxThreadCount){
				char msg[50];
				sprintf(msg," System load %.1f, pool extended to %d threads\n",loadPer,instantThreadCount);
				writeLogFile(msg);
				if(instantThreadCount>passCount){		// create new threads
					
					pthread_t* temp=(pthread_t *)realloc(threadPool,(instantThreadCount)*sizeof(pthread_t));	// extand thread pool size
					threadPool=temp;		// assign new sized thread pool
					int i=passCount;
					for(;i<instantThreadCount;i++){
						if(pthread_create(&threadPool[i], NULL, threadFunc,(void*)i)!=0){	// initiliaze threads
							writeLogFile("Error in thread creation!\n");
						pthread_cond_signal(&cond_full);	// send signal if all thread works
					}
				}
			}else instantThreadCount=passCount;
		}
		pthread_mutex_unlock(&main_mutex);
		pthread_mutex_unlock(&resize_mutex);

		}
		if(gotSIGINTSig==1) {
			break;
		}
	}
	return 0;
}

void checkOneInstanceRunning(){			// create a locked file and check if program runned again and print
    int pid_file = open("/tmp/server.pid", O_CREAT | O_RDWR, 0666);
    int res = flock(pid_file, LOCK_EX | LOCK_NB);
    
    if(res) {
        if(EAGAIN == errno){ 						// it means server running
            printf("Server already running...\n" );
            exit(EXIT_FAILURE);        
        }
    }
}

struct node* newNode(long int index) { 
    struct node* newNode = (struct node*) malloc(sizeof(struct node)); 
    newNode->id = index; 
    newNode->next = NULL; 
    return newNode; 
} 
  
void createGraph(long int gSize) { 
    graph = (struct Graph*) malloc(sizeof(struct Graph)); 
    graph->size =gSize; 
    graph->array = (struct adjList*) malloc(gSize * sizeof(struct adjList)); 
    long int i; 
    for (i = 0; i < gSize; ++i) 
        graph->array[i].head = NULL; 
    graph->addedNodeCount=0;
} 
  
void addEdge(struct Graph* graph,long int startNode, long int connectedNode) { 	// add new edge into graph    
    long int i,flag=0;
	    for(i=0;i<graph->addedNodeCount;i++){			// check if start node found, if is, add next pointer to new edge
	    	if(graph->array[i].head!=NULL){	
		    	if(graph->array[i].head->id==startNode){
		    		flag=1;
				    struct node* nodeN = newNode(connectedNode);

		    		if(graph->array[i].head->next!=NULL){
					    struct node *temp=graph->array[i].head->next;
					    nodeN->next=temp;
					    graph->array[i].head->next=nodeN;
		    		}else graph->array[i].head->next=nodeN;
			    	i=graph->addedNodeCount;
		    	}
	    	}
		}
	if(flag==0){				// if start node didnt found, add it into new index
	    struct node* n0 = newNode(startNode);
	    struct node* n1 = newNode(connectedNode);
	    graph->array[graph->addedNodeCount].head=n0;
	    graph->array[graph->addedNodeCount].head->next=n1;
	    graph->addedNodeCount++;
	}
} 

char* bfs(struct Graph *g, long int startNode, long int endNode){		// calculate paths with use bfs search
	int foundeIndex=0, noPath=0,founded=0, okStart=0,okEnd=0,okToFirst=0;
	long int item,i, path_index=0, visited_index=0, lookedEdgeCount=0;
	if(endNode<=maxNodeId && startNode>=minNodeId){
		
		for(i=0; i<uniqSCount;i++){				// first check start node is there unique start nodes arr
			if(uniqSArr[i]==startNode){
				okStart=1;
				foundeIndex=i;				
				i=uniqSCount;
			}
		}
		if(okStart==1){							// if stat nodes found, do bfs search

			struct node* temp=g->array[foundeIndex].head;

		 	item=temp->id;
		 	enqueue(que,item);
		 	item=dequeue(que);					// pop start node and add visited

			visited[visited_index]=item;
			visited_index++;

			paths[path_index][0]=item;
			temp=temp->next;
			paths[path_index][1]=temp->id;
			lookedEdgeCount++;
		 	while(temp){						// get all connected graph

		 		if(temp!=NULL){
			 		paths[path_index][0]=item;
			 		paths[path_index][1]=temp->id;
			 		lookedEdgeCount++;
			 		if(temp->id==endNode) {		// if end node found, break
			 			okToFirst=1;
			 			break;
			 		}
			 		enqueue(que,temp->id);
			 		path_index++;
		 		}
		 		temp=temp->next;
		 	}
		 	okEnd=0;
			int flag2=0,j=0;
			if(okToFirst==0){ 
			 	while(isEmpty(que)==0){			// get all adjacent nodes
			 		long int nodeV=dequeue(que);
			 		if(item==endNode){
			 			paths[path_index][0]=item;
			 			okEnd=1;
			 			break;
			 		}
			 		item=nodeV;
			 		founded=0;
			 		flag2=0;
					for(i=0; i<g->addedNodeCount;i++){				// get startnode index
						if(g->array[i].head->id==item){
							founded=1;
							foundeIndex=i;							// founded start node index
							i=g->addedNodeCount;
						}
					}
					j=0;
					if(founded==1){
						for(;j<visited_index;j++){
							if(visited[j]==item){					// addd visited list
								flag2=1;
								j=visited_index;
							}
						}
						if(flag2==0){
							visited[visited_index]=item;
							visited_index++;
							if(visited_index==maxNodeId+1){
								noPath=1;
								break;
							}
						}
				 		temp=g->array[foundeIndex].head;
				 		temp=temp->next;
					 	while(temp){								// get all connected graph
					 		if(path_index==pathsSize-1){
					 			printf("path_index doldu... %ld\n",path_index);
					 			noPath=1;
					 			break;
					 		}
					 		int p_check=0;
					 		for(i=0;i<path_index;i++){
					 			if(paths[i][0]==item && paths[i][1]==temp->id){
					 				p_check=1;
					 				i=path_index;
					 			}
					 		}
					 		if(p_check==0){							// add paths
					 			paths[path_index][0]=item;
					 			paths[path_index][1]=temp->id;
					 			path_index++;
						 		if(path_index==pathsSize-1){		// if end node found break
						 			noPath=1;
						 			break;
						 		}
				 			}	 			
				 			if(temp->id==endNode){
				 				item=temp->id;
				 				okEnd=1;
				 				break;
				 			}
				 			if(que->size==que->capacity){
				 				noPath=1;
				 				break;
				 			}
							enqueue(que,temp->id);					// get next node
				 			temp=temp->next;
				 		}
			 		}
			 		if(okEnd==1 || noPath==1) break;
			 	}
			 }
	 	 }
	 }else okEnd=0;	

 	if(okEnd==1 && okStart==1 && okToFirst==0){	// if paths found, go preper to  path
	 	while(isEmpty(que)==0){
	 		dequeue(que);
	 	}
	 	int okToStart=0;
	 	enqueue(que,item);						// enddnode added queue
	 	long int parent=paths[path_index-1][0];
	 	enqueue(que,parent);					// parent added
	 	long int resultSize=2;
	 	for(i=path_index-1;i>=0;i--){
	 		if(paths[i][1]==parent){			// finded parents parent
	 			resultSize++;	
	 			enqueue(que,paths[i][0]);		// nodes added queue
	 			if(paths[i][0]==startNode || paths[i][1]==startNode){		// check if parent equal start node, if it, finish loop
	 				i=-1;
	 				okToStart=1;
	 			}else parent=paths[i][0];
	 		}
	 	}
	 	if(okToStart==1){
	 		if(que->size>0){
			 	long int nodes,index=0;
			 	while(isEmpty(que)==0){
			 		nodes=dequeue(que);
			 		if(index>0){
				 		if(paths[index-1][0]!=nodes){
					 		paths[index][0]=nodes;
					 		index++;
				 		}
				 	}else{
				 		paths[index][0]=nodes;
				 		index++;
				 	}
			 	}

			 	long int tmp,l=0,r=index-1; 
			    while (l < r){ 
			        tmp = paths[l][0];    
			        paths[l][0] = paths[r][0]; 
			        paths[r][0] = tmp; 
			        l++; 
			        r--; 
			    }
			    char *val;
			    long int b=0;
			    for (i = 0; i < index; i++) {			// add result bfs and return
			    	if(i!=index-1) val="%ld->";
			    	else val="%ld";
			        b+=sprintf(&bfsRes[b],val, paths[i][0]);
			    }

			 }else noPath=1;
		}else noPath=1;
	    

	}else noPath=1;

	if(noPath==1 && okToFirst==0){			// if path not found	
		char temp[100];
		sprintf(temp," path not possible from node %ld to %ld",startNode,endNode);
		strcpy(bfsRes,temp);
   	}
   	if(okToFirst==1){
		char temp[100];
		sprintf(temp,"%ld->%ld",startNode,endNode);
		strcpy(bfsRes,temp);
   	}
 	for(i=0; i<path_index; i++){ 
 		paths[i][0]=-1;
     	paths[i][1]=-1;
 	}
 	for(i=0;i<visited_index;i++)
 		visited[i]=-1;
 	while(isEmpty(que)==0){
 		dequeue(que);
 	}

    return bfsRes;
}

struct Queue* createQueue(long int capacity){ 
    struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->reSizeCount=0; 
    queue->capacity = capacity; 
    queue->front = 0;
    queue->size = 0; 
    queue->rear = capacity - 1; 
    queue->array = (long int*)malloc(queue->capacity * sizeof(long int)); 
    return queue; 
}

int isEmpty(struct Queue* queue){ 
    if(queue->size == 0) return 1; 	// true
    else return 0;					// false	 
}

int isFull(struct Queue* queue){ 
    return (queue->size == queue->capacity); 
} 

void enqueue(struct Queue* queue, long int item){ 
    if (isFull(queue)) 
        return; 
    queue->rear = (queue->rear + 1) % queue->capacity; 
    queue->array[queue->rear] = item; 
    queue->size = queue->size + 1; 
} 

long int dequeue(struct Queue* queue){ 
    if (isEmpty(queue)) return -1; 
    long int item = queue->array[queue->front]; 
    queue->front = (queue->front + 1) % queue->capacity; 
    queue->size = queue->size - 1; 
    return item; 
}

long int rear(struct Queue* queue) { 
    if (isEmpty(queue)) 
        return -1; 
    return queue->array[queue->rear]; 
}

long int front(struct Queue* queue){  // -1 means true
    if (isEmpty(queue)) 
        return -1; 
    return queue->array[queue->front]; 
} 

void createTasksArr(int size){
	tasksArr=(struct threadTask*)malloc(sizeof(struct threadTask)*size);
	int i=0;
	for(;i<size;i++){
		tasksArr[i].id=i;
		tasksArr[i].status=0; // 0: avaible 1: busy
		tasksArr[i].full=0;
		tasksArr[i].socketFd=0;
		if(pthread_mutex_init(&tasksArr[i].mutex,NULL)!=0){								// initialize mutex and condition variables
			printf("mutex initialize failed!!!. Program will finish.");
			exit(EXIT_FAILURE);
		}
		if(pthread_cond_init(&tasksArr[i].cond,NULL)!=0){
			printf("condtion variable initialize failed!!!. Program will finish.");
			exit(EXIT_FAILURE);
		}
	}
}

void writeLogFile(char* msg){
	char info[1024];
	struct tm *currentTime;
    time_t timeCurrent = time(0);
    currentTime = gmtime (&timeCurrent);
    strftime (info, sizeof(info), "%Y-%m-%d %H:%M:%S ", currentTime);
    strcat(info,msg);
	if(write(logFileFd,info,strlen(info))==-1){
		perror("Write log file error.\n");
		exit(EXIT_FAILURE);
	}
}

long int getHashCode(long int value){		// get start+endnode sum and calculate hash code
	long int index=(value)%(db->capacity);
	return index;
}

int addDataBase(long int s, long int d,char res[]){		// add two nodes path into db with start-end nodes with use hash code
	long int index=getHashCode(s+d);
	if(db->paths[index].start==-1 && db->paths[index].destination==-1){			// if index empty
		db->paths[index].start=s;
		db->paths[index].destination=d;
		db->paths[index].result=(char*)malloc(sizeof(char)*(strlen(res)));		// add result
		sprintf(db->paths[index].result,"%s",res);
		db->addedCount++;
	}else{
		index++;

		int check=0;
		while(db->paths[index].start!=-1 && db->paths[index].destination!=-1){	// false, find next empty row
			if(index==db->capacity){
				index=index%db->capacity;
				check++;
				if(check==2) break;
			}
			index++;
		}
		if(check<2){
			db->paths[index].start=s;
			db->paths[index].destination=d;
			db->paths[index].result=(char*)malloc(sizeof(char)*(strlen(res)));
			sprintf(db->paths[index].result,"%s",res);
			db->addedCount++;
		}
	}
	if(db->addedCount==db->capacity){
		expandDb(2*db->capacity);
	}
	return -1;			// this means, db full;
}

long int getDbEntryIndex(long int n1, long int n2){		// -1 there is no entry, otherwise return index
	long int index=getHashCode(n1+n2);
	if(db->paths[index].start==-1 && db->paths[index].destination==-1){
		return -1;
	}else{
		if(db->paths[index].start==n1 && db->paths[index].destination==n2)
			return index;
		else{
			index++;
			int check=0;
			while(db->paths[index].start!=n1 && db->paths[index].destination!=n2){	// false, find next empty row
				index++;
				if(index==db->capacity-1){
					index=index%db->capacity;
					check++;
					if(check==2) return 0;			// this means, db full;
				}
			}
			if(index<db->capacity) return index;
			else return -1;
		}
	}
	return -1;	
}

char* getPath(long int index){						// return path from db
	return db->paths[index].result;
}

void createDataBase(int capacity){
	db=(struct dataBase*)malloc(sizeof(struct dataBase));				// created databse 
	db->paths=(struct data*)malloc(sizeof(struct data)*capacity);

	int i=0;
	for(;i<capacity;i++){			// -1 means empty
		db->paths[i].start=-1;
		db->paths[i].destination=-1;
	}
	db->capacity=capacity;
	db->addedCount=0;
}

long int reader(int t_id,long int start,long int end){
    pthread_mutex_lock(&db_mutex);

	while ((AW + WW) > 0) { // if any writers, wait
		WR++; // waiting reader
		pthread_cond_wait(&okToRead	,&db_mutex);
		WR--;
	}
	AR++;
	pthread_mutex_unlock(&db_mutex);
    long int res=getDbEntryIndex(start,end);	// reader first get index from hash code

    if(res>=0){
		char* path=getPath(res);				// get path if exist
		char* ans=(char*)malloc(sizeof(char)*strlen(path));
	 	sprintf(ans," Thread #%d: path found in database: %s\n",t_id,path);
	 	writeLogFile(ans);
		write(tasksArr[t_id].socketFd, path, strlen(path));
    }

    pthread_mutex_lock(&db_mutex);
	AR--;
	if (AR == 0 && WW > 0)
		pthread_cond_signal(&okToWrite);		// prioriy writers
	pthread_mutex_unlock(&db_mutex);
	return res;
}

void writer(int t_id,long int start,long int end){	// writer thread first get bfs result, send client and add db

	pthread_mutex_lock(&db_mutex);
	while ((AW + AR) > 0) { // if any readers or writers, wait
		WW++; 				// waiting writer
		pthread_cond_wait(&okToWrite, &db_mutex);
		WW--;
	}
	AW++; // active writer
	pthread_mutex_unlock(&db_mutex); 

	// writer start
    char* resultBfs=bfs(graph, start, end);
	char* ans=(char*)malloc(sizeof(char)*strlen(resultBfs));

    sprintf(ans," Thread #%d: path calculated: %s\n",t_id,resultBfs);
    writeLogFile(ans);
    write(tasksArr[t_id].socketFd, resultBfs, strlen(resultBfs));

    sprintf(ans," Thread #%d: responding to client and adding path to database\n",t_id);
    writeLogFile(ans);
    addDataBase(start,end,resultBfs);

	AW--;
	if (WW > 0) 	// give priority to other writers
		pthread_cond_signal(&okToWrite);
	else if (WR > 0)
		pthread_cond_broadcast(&okToRead);
	pthread_mutex_unlock(&db_mutex);

}

void expandDb(long cap){			// expand db if full
	db->paths=(struct data*)realloc(db->paths,sizeof(struct data)*cap);
	int i=db->capacity-1;
	for(;i<cap;i++){			// -1 means empty
		db->paths[i].start=-1;
		db->paths[i].destination=-1;
	}
	db->capacity=cap;
}

void sighandler(int sig){													// threads use this handler for hand SIGINT SIGNAL
	int j=0,i=0;
    switch (sig){
        case SIGINT:
         	gotSIGINTSig = 1;
            writeLogFile(" Termination signal received, waiting for ongoing threads to complete.\n");
	       	for(j=0;j<instantThreadCount;j++){
	       		if(tasksArr[j].full==0) {
	       			tasksArr[j].full=1;	
	       			pthread_cond_signal(&tasksArr[j].cond);
	       		}
				void * ptr = NULL;
				i=pthread_join(threadPool[j],&ptr); 						// after canceled, wait for terminate
				if(i!=0) 
					writeLogFile(" Error p_thread Join");

			}
			free(threadPool);
			pthread_cond_signal(&cond_resize);
			void * ptr = NULL;
			i=pthread_join(resizingThread,&ptr); 						// after canceled, wait for terminate
			if(i!=0) 
				writeLogFile(" Error resizimg thread_join\n");
			free(bfsRes);			// free all variables					
			free(visited);
			for(i = 0; i < pathsSize; i++)
			    free(paths[i]);
			free(paths);
			free(db->paths);
			free(db);
			for(i=0; i<graph->size;i++){
		    	if(graph->array[i].head!=NULL){
				    struct node* tmp;
					   while (graph->array[i].head!=NULL){
					       tmp = graph->array[i].head;
					       graph->array[i].head = graph->array[i].head->next;
					       free(tmp);
					    }
    		    		free(graph->array[i].head);
	    		}
	    		free(graph->array[i].head);
			}
			free(graph->array);
			free(graph);
			for(i=0;i<edgesCount;i++)
				free(noteToNodeArr[i]);

			free(noteToNodeArr);
			free(que->array);
			free(que);
			free(tasksArr);
			free(uniqSArr);
			writeLogFile(" All threads have terminated, server shutting down.");
			close(logFileFd);
            kill(getpid(),SIGKILL);
            break;              
        default:
            writeLogFile(".... UNKNOW SIGNAL IN MAIN THREAD ...\n");    
            break;
    }
}
void usage(){		// print usage
	printf("Program finished\nusage: ./server -i pathToFile -p PORT -o pathToLogFile -s 4 -x 24\n");
	printf("-pathToFile: Containing a directed unweighted graph from the Stanford Large Network Dataset Collection ( https://snap.stanford.edu/data/ )\n");
	printf("-PORT: This is the port number the server will use for incoming connections.\n-pathToLogFile: Relative or absolute path of the log file to which the server daemon will write all of its output (normal output & errors)\n");
	printf("-s : this is the number of threads in the pool at startup (at least 2)\n");
	printf("-x : this is the maximum allowed number of threads, the pool must not grow beyond this number.\n");
	exit(EXIT_FAILURE);
}

