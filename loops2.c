#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define N 1729
#define reps 1000
#define thread_nums 12

#include <omp.h> 

double a[N][N], b[N][N], c[N];
int jmax[N];  

//chunk for affinity scheduling
//a node with a chunk in queue
typedef struct chunk{
  //chunk info
  int lo;
  int hi;
  int workload;
  struct chunk *next;//easy to find
} ck;

//work queue with nodes
typedef struct work_queue{
    ck *front;//the front of the queue
    ck *rear;//the rear of the queue
    int total_chunk_size;
    int total_workload;
} wq;

/*
Init a queue
*/
void init_queue(wq *p_queue){
  if(p_queue != NULL){
    p_queue->front = NULL;
    p_queue->rear = NULL;
    p_queue->total_chunk_size = 0;
    p_queue->total_workload = 0;
  }
  return;
}

/*
Destroy a queue
*/
void destroy_queue(wq *p_queue){
  if(p_queue != NULL){
    while(p_queue->front != NULL){
      ck *p_temp = p_queue->front;
      p_queue->front = p_queue->front->next;
      free(p_temp);
      p_temp = NULL;
    }
    p_queue->rear = NULL;
    p_queue->total_chunk_size = 0;
    p_queue->total_workload = 0;
  }
  return;
}

/*
enQueue for a new chunk
*/
void enQueue(wq *p_queue, ck chunk){
  if(p_queue != NULL){
    //init for a new work node
    ck *ck_node = (ck *)malloc(sizeof(ck));

    ck_node->lo = chunk.lo;
    ck_node->hi = chunk.hi;
    ck_node->workload = chunk.workload;
    ck_node->next = NULL;

    if(p_queue->front != NULL){
      //already have nodes
      p_queue->rear->next = ck_node;
      p_queue->rear = p_queue->rear->next;
    }
    else{
      //have no nodes
      p_queue->front = ck_node;
      p_queue->rear = ck_node;
    }

    p_queue->total_workload += ck_node->workload;
    (p_queue->total_chunk_size)++;
  }
  return;
}

/*
deQueue and return a chunk
*/
ck deQueue(wq *p_queue){
  ck res_chunk;
  if(p_queue != NULL && p_queue->front != NULL){
    ck *ck_temp = p_queue->front;

    res_chunk.lo = ck_temp->lo;
    res_chunk.hi = ck_temp->hi;
    res_chunk.workload = ck_temp->workload;
    res_chunk.next = NULL;

    p_queue->front = p_queue->front->next;
    p_queue->total_workload = p_queue->total_workload - ck_temp->workload;
    (p_queue->total_chunk_size)--;

    free(ck_temp);
    ck_temp = NULL;  
  }

  return res_chunk;
}

/*
Judge if the queue is empty
*/
int isEmpty(wq *p_queue){
  if(p_queue != NULL){
    if(p_queue->total_chunk_size > 0 )
      return 0;
    else
      return 1;
  }
}

/*
Print a queue
*/
void print_queue(wq *p_queue){
  printf("Total work load: %d\n", p_queue->total_workload);
  printf("Total chunk size: %d\n", p_queue->total_chunk_size);
  ck *ck_node = p_queue->front;
  while(ck_node != NULL){
    //print chunk info of every node
    printf("--------------\n");
    printf("lo:%d\n", ck_node->lo);
    printf("hi:%d\n", ck_node->hi);
    printf("workload:%d\n", ck_node->workload);
    printf("--------------\n");
    ck_node = ck_node->next;
  }
  return;
}

void init1(void);
void init2(void);
void runloop(int);
void run_loopchunk(int, int, int);
void loop1chunk(int, int);
void loop2chunk(int, int);
void valid1(void);
void valid2(void);

int main(int argc, char *argv[]) { 

  double start1,start2,end1,end2;
  int r;

  init1(); 

  start1 = omp_get_wtime(); 

  for (r=0; r<reps; r++){ 
    // printf("%d\n", r);
    runloop(1);
  } 

  end1  = omp_get_wtime();  

  valid1(); 

  printf("Total time for %d reps of loop 1 = %f\n",reps, (float)(end1-start1)); 


  init2(); 

  start2 = omp_get_wtime(); 

  for (r=0; r<reps; r++){ 
    runloop(2);
  } 

  end2  = omp_get_wtime(); 

  valid2(); 

  printf("Total time for %d reps of loop 2 = %f\n",reps, (float)(end2-start2)); 

} 

void init1(void){
  int i,j; 

  for (i=0; i<N; i++){ 
    for (j=0; j<N; j++){ 
      a[i][j] = 0.0; 
      b[i][j] = 1.618*(i+j); 
    }
  }

}

void init2(void){ 
  int i,j, expr; 

  for (i=0; i<N; i++){ 
    expr =  i%( 4*(i/60) + 1); 
    if ( expr == 0) { 
      jmax[i] = N/2;
    }
    else {
      jmax[i] = 1; 
    }
    c[i] = 0.0;
  }

  for (i=0; i<N; i++){ 
    for (j=0; j<N; j++){ 
      b[i][j] = (double) (i*j+1) / (double) (N*N); 
    }
  }
 
} 
// void runloop(int loopid)  {

// #pragma omp parallel default(none) shared(loopid) 
//   {
//     int myid  = omp_get_thread_num();
//     // printf("myid-%d\n", myid);
//     int nthreads = omp_get_num_threads();
//     // printf("nthreads-%d\n", nthreads); 
//     int ipt = (int) ceil((double)N/(double)nthreads); 
//     int lo = myid*ipt;
//     int hi = (myid+1)*ipt;
//     if (hi > N) hi = N; 
  
//     run_loopchunk(loopid, lo, hi);
//   }
// }

void runloop(int loopid){
  wq *work_queues;  //work queues for every thread
  omp_lock_t *locks; //locks for every thread
#pragma omp parallel default(none) shared(loopid, work_queues, locks) num_threads(thread_nums)
  {
    int myid = omp_get_thread_num();
    int nthreads = omp_get_num_threads();

    printf("Current thread id is %d\n", myid);
#pragma omp single
    {
      //Init work queues for all threads
      work_queues = (wq *)malloc(nthreads * sizeof(wq));
      //Init locks for all threads
      locks = (omp_lock_t *)malloc(nthreads * sizeof(omp_lock_t));
      
      for(int i = 0; i< nthreads ; i++){
        init_queue(&work_queues[i]);
        omp_init_lock(&locks[i]);
      }

      //debug
      // for(int i = 0; i < nthreads ; i++){
      //   print_queue(&work_queues[i]);
      // }     
    }   
    
    // printf("+++++++++++++++++++%d\n", myid);
    //Local work set
    int ipt = (int) ceil((double)N / (double)nthreads); 
    int local_set_start = myid * ipt;
    int local_set_end = (myid + 1)*ipt;
    
    if (local_set_end > N){
      local_set_end = N;
    }

    // printf("----------%d\n", myid);

    while(local_set_start < local_set_end){
      //workload: 1/p of remaining iterations
      int workload = (int)ceil((double)(local_set_end - local_set_start) / (double)nthreads);
      // printf("workload-----------%d\n", workload);
      ck temp_ck;
      temp_ck.lo = local_set_start;
      temp_ck.hi = local_set_start + workload;
      temp_ck.workload = workload;
      temp_ck.next = NULL;

      //Put in thread's work queue
      enQueue(&work_queues[myid], temp_ck);

      local_set_start = local_set_start + workload;
    }
    // printf("over-thread: %d\n", myid);
#pragma omp barrier
    // printf("All threads are over.\n");

// debug
// #pragma omp barrier 
// #pragma omp single
//     {
//       for(int i = 0; i < nthreads ; i++){
//         printf("Thread:%d\n", i);
//         printf("Chunk size:%d\n", work_queues[i].total_chunk_size);
//         printf("Work load:%d\n", work_queues[i].total_workload);
//       }   
//     }

    //local work execution
    while(!isEmpty(&work_queues[myid])){
      
      ck current_chunk;
      //deQueue for executing
      omp_set_lock(&locks[myid]);
      current_chunk = deQueue(&work_queues[myid]);
      omp_unset_lock(&locks[myid]);

      //execute current chunk
      run_loopchunk(loopid, current_chunk.lo, current_chunk.hi);
    }
    // printf("-----tag%d-----\n", myid);
    //execute wordload from other threads - work stealing
    int m_id;
    int m_workload;
    while(1){
      //get the most loaded thread
      m_id = -1;
      m_workload = 0;
      for(int i = 0 ; i < nthreads ; i++){
        if(work_queues[i].total_workload > m_workload){
          m_workload = work_queues[i].total_workload;
          m_id = i;
        }
      }
      //All workload is over
      if(m_id == -1){
        break;
      }
      omp_set_lock(&locks[m_id]);

      ck o_chunk;

      if(work_queues[m_id].total_workload != m_workload){
        omp_unset_lock(&locks[m_id]);
        continue;
      }
      o_chunk = deQueue(&work_queues[m_id]);
      omp_unset_lock(&locks[m_id]);

      run_loopchunk(loopid, o_chunk.lo, o_chunk.hi);

    }
    // printf("-----tag%d-----\n", myid);

#pragma omp barrier
    // printf("All threads are over.\n");

    destroy_queue(&work_queues[myid]);
    omp_destroy_lock(&locks[myid]);
  }
  if(work_queues != NULL)
    free(work_queues);

  if(locks != NULL)
    free(locks);
}

//run loop chunk for loop1 and loop2
void run_loopchunk(int loopid, int lo, int hi){
  switch(loopid){
    case 1:
      loop1chunk(lo, hi);
      break;
    case 2:
      loop2chunk(lo, hi);
      break;
  }
}

void loop1chunk(int lo, int hi) { 
  int i,j;   
  for (i=lo; i<hi; i++){ 
    for (j=N-1; j>i; j--){
      a[i][j] += cos(b[i][j]);
    } 
  }

} 



void loop2chunk(int lo, int hi) {
  int i,j,k; 
  double rN2; 

  rN2 = 1.0 / (double) (N*N);  

  for (i=lo; i<hi; i++){ 
    for (j=0; j < jmax[i]; j++){
      for (k=0; k<j; k++){ 
	      c[i] += (k+1) * log (b[i][j]) * rN2;
      } 
    }
  }

}


void valid1(void) { 
  int i,j; 
  double suma; 
  
  suma= 0.0; 
  for (i=0; i<N; i++){ 
    for (j=0; j<N; j++){ 
      suma += a[i][j];
    }
  }
  printf("Loop 1 check: Sum of a is %lf\n", suma);

} 


void valid2(void) { 
  int i; 
  double sumc; 
  
  sumc= 0.0; 
  for (i=0; i<N; i++){ 
    sumc += c[i];
  }
  printf("Loop 2 check: Sum of c is %f\n", sumc);
} 
 

