
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP(secs) sleep(secs)
#elif __unix 
    #include <unistd.h>
#else
    error "Unknown OS"
#endif

/* 
 * Time conventions used :
 *
 * 1 Simulated Second == 0.001 Real Seconds
 * Aka
 * 1 Simulated Second == 1 Real Millisecond
 *
 * Also all time printed from 0 -> 8 
 * assumes that the cafe opened at "0" 
 * for example it could have opened at 12:00 
 * and that would be the "0", which then
 * respectively it would have ended at 20:00
 * at noon and 8 pm for non metric users.
 *
*/

/*
 * Algumas suposições : Se um cliente chegar 
 * e pedir um tempo acima do tempo faltando para
 * fechar o cyber_cafe, ele é rejeitado, e o
 * tempo de recursos dele é o tempo faltando 
 * para fechar o café.
*/

#define SECOND 0.001
#define MILLISECOND (1000*SECOND)
#define MINUTE (60*SECOND)
#define HOUR (60*MINUTE)
#define DAY (24*HOUR) 

typedef enum {
    Gamer,
    Freelancer,
    Student
} client_type;

typedef struct {
    sem_t sem;
    size_t num;
    size_t used;
    float  time_spent_waiting;
}pc;

typedef struct {
    sem_t sem;
    size_t num;
    size_t used;
    float  time_spent_waiting;
}headset;

typedef struct {
    sem_t sem;
    size_t num;
    size_t used;
    float  time_spent_waiting;
}seat;

typedef struct{
    pthread_t p; 
    client_type t;

    float arrival_time;
    float time_spent_waiting;

    float res_time;
    int UID;

    bool serviced;
    bool waiting_for_resources;
    bool waiting_for_pc;
    bool waiting_for_headset;
    bool waiting_for_seat;

} client;

typedef struct{
    pc *pcs;
    headset *headsets;
    seat *seats;

    size_t n_gamers;
    size_t n_freelancers;
    size_t n_students;

    size_t timed_out;
    size_t serviced;

    float total_time;
    float time_left;
    size_t c_size;
}cyber_flux;

void open_cafe(cyber_flux *f)
{
  assert(f!=NULL);

  f->pcs = (pc *) malloc(sizeof(pc));     
  f->headsets = (headset *) malloc(sizeof(headset));     
  f->seats = (seat *) malloc(sizeof(seat));     

  f->pcs->num = 10;
  f->pcs->used = 0;

  f->headsets->num = 6; 
  f->headsets->used = 0;
  
  f->seats->num = 8;
  f->seats->used = 0;

  f->n_gamers = 0;
  f->n_freelancers = 0;
  f->n_students = 0;

  f->serviced  = 0;
  f->timed_out = 0;

  sem_init(&(f->pcs->sem), 0, f->pcs->num);
  sem_init(&(f->headsets->sem), 0, f->headsets->num);
  sem_init(&(f->seats->sem), 0, f->seats->num);

  f->total_time = (8 * HOUR) + 5 * MINUTE;
  f->c_size = 0;
  f->time_left = f->total_time;
}

/*
 * Random float in the range [0, 8 * HOUR] inclusive
 * in units of hours.
 * Right now : 
 * [0, 28.8]
*/
float randomFloat()
{
   float r =
       ((float)rand() * (8 * HOUR)) / (float)RAND_MAX;
   assert(r>=0&&r<=(8*HOUR));
   return r;
}

typedef struct{
    int hrs;
    int mins;
    int secs;
   // float misecs;
}real_time;

// In hours return the real time 
// from the simulated time.
// As defined in the real_time struct.
real_time cfsttr(float s_time)
{
    assert(s_time>=0);
    real_time rt = {0, 0, 0};
    double real_hours = s_time / HOUR;
    rt.hrs  = (int) real_hours;
    double rem_hours = (real_hours - rt.hrs);
    rt.mins = (int) (rem_hours * 60);
    double rem_mins = (rem_hours * 60);
    rt.secs = (int) ((rem_mins - rt.mins) * 60);
    //double rem_secs = (rem_mins * 60);
    //rt.misecs = (int) ((rem_secs - rt.secs)* 1000.0);
    return rt;
}

char *real_time_string(real_time *rt)
{
    assert(rt!=NULL);
    char rts[] = "{ %d h :: %d m :: %d s }";
    int s_len = snprintf(NULL, 0, rts, 
            rt->hrs, rt->mins, rt->secs ); //rt->misecs);
    char *rts_r = (char *) malloc(s_len + 1);
    assert(rts_r!=NULL);
    (void) sprintf(rts_r, rts, rt->hrs, rt->mins,
            rt->secs); //rt->misecs);
    return rts_r;
}

cyber_flux cafe = {};

pthread_mutex_t inc_mutex      = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_gamers     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_students   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_freelancers= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t time_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rand_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t update_average_resource = PTHREAD_MUTEX_INITIALIZER;

void increment_pcs()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.pcs->used++;
  pthread_mutex_unlock(&inc_mutex); 
}

void increment_headsets()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.headsets->used++;
  pthread_mutex_unlock(&inc_mutex); 
}

void increment_seats()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.seats->used++;
  pthread_mutex_unlock(&inc_mutex); 
}

void increment_gamers()
{
  pthread_mutex_lock(&inc_gamers);
  cafe.n_gamers++;
  pthread_mutex_unlock(&inc_gamers); 
}

void increment_students()
{
  pthread_mutex_lock(&inc_students);
  cafe.n_students++;
  pthread_mutex_unlock(&inc_students); 
}

void increment_freelancers()
{
  pthread_mutex_lock(&inc_freelancers);
  cafe.n_freelancers++;
  pthread_mutex_unlock(&inc_freelancers); 
}

pthread_mutex_t closing_mutex = PTHREAD_MUTEX_INITIALIZER;
bool cafe_closing = false;

char *type_string(client_type t)
{
    char *r;
    switch(t)
    {
        case Gamer:
        {
            r = (char *) malloc(strlen("GAMER") * sizeof(char) + 1);
            strcpy(r, "GAMER");
            return r;
        }
        case Freelancer:
        {
            r = (char *) malloc(strlen("FREELANCER") * sizeof(char) + 1);
            strcpy(r, "FREELANCER");
            return r;
        }
        case Student:
        {
            r = (char *) malloc(strlen("STUDENT") * sizeof(char) + 1);
            strcpy(r, "STUDENT");
            return r;
        }
    }
    return NULL;
}

double wt_pcs = 0;
double wt_headsets = 0;
double wt_seats = 0;

void *new_client(void *d)
{
  client *c = (client *) d;
  assert(&c!=NULL);

  // Time of arrival at the cafe
  // this simulates random client arrival
  // we have to lock since main is reducing
  // cafe.time_left.
  pthread_mutex_lock(&time_mutex);
  float arrival = cafe.total_time - cafe.time_left;
  c->arrival_time = arrival;
  pthread_mutex_unlock(&time_mutex); 

  c->serviced = false;

  real_time art = cfsttr(arrival);
  char *art_s = real_time_string(&art);

  pthread_mutex_lock(&rand_mutex);
  c->t = (client_type) (rand() % 3);
  c->res_time = randomFloat();
  assert(c->t>=0&&c->t<=2);
  pthread_mutex_unlock(&rand_mutex);

  char *type = type_string(c->t);
  if(type != NULL)
  {
      (void) fprintf(stdout,
              "Client {%d} : {%s} arrived at the cafe at time ( sim %.3f :: real %s )\n",
              c->UID, type, c->arrival_time,art_s);
      free(type);
  }

  (void) free(art_s);

  pthread_mutex_lock(&closing_mutex);
  if (cafe_closing) {
      pthread_mutex_unlock(&closing_mutex);
      goto exit;
  }
  pthread_mutex_unlock(&closing_mutex);

  // Tempo entre 0 e tempo máximo
  // de funcionamento do café
  // ( Tempo de uso dos recursos pelo cliente )
  (void) pthread_mutex_lock(&time_mutex);
  if (c->res_time > cafe.time_left)
  {
      cafe.timed_out++;
      c->res_time = cafe.time_left;
  }
  (void) pthread_mutex_unlock(&time_mutex); 

  time_t start, end;

  switch(c->t)
  {
      case Gamer:{
          increment_gamers();
          c->waiting_for_resources = true;
          c->waiting_for_pc = true;

          // TODO: add wait time measurements ?
          time(&start);
          int cantLock = sem_trywait(&cafe.pcs->sem);
          if(cantLock)
          {
             sem_wait(&cafe.pcs->sem);         
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_pcs += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_pcs();
          c->waiting_for_pc = false;
          c->waiting_for_headset = true;

          time(&start);
          cantLock = sem_trywait(&cafe.headsets->sem);
          if(cantLock)
          {
             sem_wait(&cafe.headsets->sem);
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_headsets += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_headsets();
          c->waiting_for_headset = false;
          c->waiting_for_seat = true;

          time(&start);
          cantLock = sem_trywait(&cafe.seats->sem);
          if(cantLock)
          {
             sem_wait(&cafe.seats->sem); 
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_seats += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_seats();
          c->waiting_for_seat = false;

          pthread_mutex_lock(&time_mutex);
          c->time_spent_waiting =
              (cafe.total_time - cafe.time_left) - c->arrival_time; 
          pthread_mutex_unlock(&time_mutex);

          c->waiting_for_resources = false;
          real_time art = cfsttr(c->res_time);
          char *art_s = real_time_string(&art);

          usleep(c->res_time * pow(10, 6));
          c->serviced = true;
          char *type = type_string(c->t);
    
          if(type != NULL)
          {
              (void) fprintf(stdout, "Client {%d} : {%s} used the resources for %s\n",
                      c->UID, type, art_s);
              free(type);
          }

          free(art_s);

          sem_post(&cafe.seats->sem); 
          sem_post(&cafe.headsets->sem);
          sem_post(&cafe.pcs->sem);
          break;
      }
      case Freelancer:{
          increment_freelancers();
          c->waiting_for_pc = true;
          c->waiting_for_resources = true;

          time(&start);
          int cantLock = sem_trywait(&cafe.pcs->sem);
          if(cantLock)
          {
             sem_wait(&cafe.pcs->sem);         
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_pcs += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_pcs();
          c->waiting_for_pc = false;
          c->waiting_for_seat  = true;

          time(&start);
          cantLock = sem_trywait(&cafe.seats->sem);
          if(cantLock)
          {
             sem_wait(&cafe.seats->sem); 
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_seats += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_seats();
          c->waiting_for_seat = false;
          c->waiting_for_headset = true;

          time(&start);
          cantLock = sem_trywait(&cafe.headsets->sem);
          if(cantLock)
          {
              sem_wait(&cafe.headsets->sem);
              time(&end);
              pthread_mutex_lock(&update_average_resource);
              wt_headsets += difftime(end, start);   
              pthread_mutex_unlock(&update_average_resource);
          }

          increment_headsets();
          c->waiting_for_headset = false;

          pthread_mutex_lock(&time_mutex);
          c->time_spent_waiting =
              (cafe.total_time - cafe.time_left) - c->arrival_time; 
          pthread_mutex_unlock(&time_mutex);

          c->waiting_for_resources = false;
          real_time art = cfsttr(c->res_time);
          char *art_s = real_time_string(&art);
          usleep(c->res_time * pow(10, 6));
          c->serviced = true;
          char *type = type_string(c->t);
    
          if(type != NULL)
          {
              (void) fprintf(stdout, "Client {%d} : {%s} used the resources for %s\n",
                      c->UID, type, art_s);
              free(type);
          }
          free(art_s);


          sem_post(&cafe.headsets->sem);         
          sem_post(&cafe.seats->sem);
          sem_post(&cafe.pcs->sem);
          break;
      }
      case Student:{
          increment_students();
          c->waiting_for_resources = true;
          c->waiting_for_pc = true;

          time(&start);
          int cantLock = sem_trywait(&cafe.pcs->sem);
          if(cantLock)
          {
             sem_wait(&cafe.pcs->sem);         
             time(&end);
             pthread_mutex_lock(&update_average_resource);
             wt_pcs += difftime(end, start);   
             pthread_mutex_unlock(&update_average_resource);
          }

          increment_pcs();
          c->waiting_for_pc = false;

          pthread_mutex_lock(&time_mutex);
          c->time_spent_waiting =
              (cafe.total_time - cafe.time_left) - c->arrival_time; 
          pthread_mutex_unlock(&time_mutex);
          c->waiting_for_resources = false;

          real_time art = cfsttr(c->res_time);
          char *art_s = real_time_string(&art);
          usleep(c->res_time * pow(10, 6));
          c->serviced = true;
          char *type = type_string(c->t);
    
          if(type != NULL)
          {
              (void) fprintf(stdout, "Client {%d} : {%s} used the resources for %s\n",
                      c->UID, type, art_s);
              free(type);
          }
          free(art_s);

          sem_post(&cafe.pcs->sem);
          break;
      }
  }

exit:
  if(c->serviced)
  {
      pthread_mutex_lock(&s_mutex);
      cafe.serviced++;
      pthread_mutex_unlock(&s_mutex);
  }

  (void) pthread_exit(NULL);
}

/*
 * Close the cafe and wait for the remaining clients 
 * to finish their time.
*/
void close_cafe(cyber_flux *f, client *cs)
{
    assert(f!=NULL&&cs!=NULL); 
    for( size_t i=0; i < f->c_size; i++)
    {
        // Cancela os clientes esperando por recursos
        // por exemplo antes de todos os semáforos 
       (void) pthread_cancel(cs[i].p); 

       (void) pthread_join(cs[i].p, NULL);
    }

    (void) sem_destroy(&f->pcs->sem);
    (void) sem_destroy(&f->seats->sem);
    (void) sem_destroy(&f->headsets->sem);

    //(void) free(cs);
}

float c_avg(client cs[])
{
    float avg = 0;
    for(size_t i=0; i < cafe.c_size; i++)
    {
       avg += cs[i].time_spent_waiting; 
    }
    return avg / cafe.c_size;
}

void generate_report(cyber_flux *f, client cs[])
{
    assert(f!=NULL);
   (void) fprintf(stdout, "\nStatistical Report : \n");
   (void) fprintf(stdout, "0 - Gamer, 1 - Freelancer, 2 - Student\n");
   (void) fprintf(stdout, "\nTotal number of clients in 8 hours : %lu\n",
           f->c_size); 
   (void) fprintf(stdout, "Number of times the resources were used :\n\n"
           " PCS : %lu\n HEADSETS : %lu\n SEATS : %lu\n\n", f->pcs->used,
           f->headsets->used, f->seats->used); 
   (void) fprintf(stdout, "Total number of types of clients :\n\n"
           " GAMERS : %lu\n FREELANCERS : %lu\n STUDENTS : %lu\n\n",
           f->n_gamers, f->n_freelancers, f->n_students); 
   (void) fprintf(stdout, "Clients that timed out : %lu\n", f->timed_out);
   (void) fprintf(stdout, "Clients that were serviced : %lu\n", f->serviced);
   (void) fprintf(stdout, "Clients that were not serviced : %lu\n",
           f->c_size - f->serviced);

   float avg = c_avg(cs);
   real_time t = cfsttr(avg);
   char *s = real_time_string(&t);

   (void) fprintf(stdout, "Average wait time for all client types ( sim %.2f "
           ":: real %s )\n\n",
           avg, s);
   free(s);

   real_time time = cfsttr(wt_pcs / cafe.pcs->used );
   s = real_time_string(&time);

   (void) fprintf(stdout, "Average wait time for PCS : ( sim %.2f :: REAL %s )\n", wt_pcs / cafe.pcs->used, s);
   free(s);
   
   time = cfsttr(wt_headsets/ cafe.headsets->used );
   s = real_time_string(&time);

   (void) fprintf(stdout, "Average wait time for HEADSETS : ( sim %.2f :: REAL %s ) \n", wt_headsets / cafe.headsets->used, s);
   free(s);

   time = cfsttr(wt_seats/ cafe.seats->used );
   s = real_time_string(&time);
   (void) fprintf(stdout, "Average wait time for SEATS : ( sim %.2f :: REAL %s ) \n", wt_seats / cafe.seats->used, s);
   
   free(s);

   (void) free(f->pcs);
   (void) free(f->seats);
   (void) free(f->headsets);
   return;
}

float clamp(float f)
{
    if(f <= 0)
    {
       return 0;
    }else{
       return f;
    }
}

#define YEL   "\x1B[33m"
#define GRN   "\x1B[32m"
#define RESET "\x1B[0m"

#define MAXIMUM_CLIENTS 28800
#define LEEWAY 100

int main(int argc, char **argv)
{

    // Calculo da probabilidade de chegada
    // de um cliente a cada segundo
    // valor default é 0.1 % 
    // se não passado como argumento.
    int modulo;

    if(argc > 1)
    {
        char *strProb = argv[1];
        double prob = strtod(strProb, NULL);
        if(prob > 3)
        {
            fprintf(stdout, YEL "WARNING:" RESET " Higher probability than 3%% will"
                    " create too many threads using 3%% instead\n");
            modulo = 100;
        }else
        {
            modulo = (int) (1 / (prob / 100));
        }
    }else
    {
        fprintf(stdout, GRN "INFO:" RESET " Using default probability of 0.1%%\n");
        modulo = 1000;
    }

    //client cs[500];
    client *cs = (client *) malloc( sizeof(client) * ( (int)(MAXIMUM_CLIENTS * (float) (1.0/ modulo)) + LEEWAY) ); 

    // client *cs = null;
    // Limit amount of 500 Clients
    // almost impossible given a 1% probability of arrival
    // changed because there were some invalid reads when
    // looking at static memory analyzers.
    // before i was trying to use dynamic memory on the heap.
    
    (void) srand(time(NULL));
    (void) open_cafe(&cafe);
    assert(&cafe!=NULL);

    int cond = cafe.time_left > 0;
     while(cond)
     {
        //
        // The probability of landing on a number in the range [0, 99]
        // is 1/100 = 1 %
        // this is the chosen probability that a new client wants
        // to come to the cafe.
        // 5 can be chosen as any number.
        //
        // 1% of 28800 ( Number of seconds in 8 hours )
        // is 288, the average number of customers given that probability.
        // 28.8 with 1000, etc.
        //
        
        pthread_mutex_lock(&rand_mutex);
        int r_prob = rand() % modulo;
        pthread_mutex_unlock(&rand_mutex);

        if(r_prob==1)
        {
            size_t cc = cafe.c_size;
            // Client wants to go to the cafe, this makes it so that
            // he cannot change his mind mid "commute there" etc.
            // The time that he arrives there is the random value.

            //client *n_cs = (client *) realloc(cs, sizeof(client) * (++(cafe.c_size)));

            //if(!n_cs)
            //{
            //    (void) perror("realloc failed");
            //    (void) exit(EXIT_FAILURE);
            //}

            //cs = n_cs;

            cs[cc].UID = ++cafe.c_size;
            int err = pthread_create(&cs[cc].p, NULL, new_client, (void *) &(cs[cc]));
            if (err!=0)
            {
                (void) perror("pthread_create failed");
                (void) exit(EXIT_FAILURE);
            }

        }

        // Alocar dinamicamente clientes desse jeito 
        // deixa fragmentação quando clientes terminam 
        // não é memory leak por que tudo ainda pode 
        // ser desalocado, mas gasta mais memoria.


        // This function should be ran the amount
        // of seconds that exists in time_left
        // each time decrement time_left by the simulated
        // second definition

        bool closingCond;
        (void) pthread_mutex_lock(&time_mutex);
        cafe.time_left = clamp(cafe.time_left - SECOND);
        closingCond = cafe.time_left <= (5 * MINUTE);
        cond = cafe.time_left > 0;
        (void) pthread_mutex_unlock(&time_mutex);
#ifdef _WIN32
        SLEEP(0);
#elif __unix
        usleep(1000);
#endif

        if(closingCond)
        {
            pthread_mutex_lock(&closing_mutex);
            cafe_closing = true;
            pthread_mutex_unlock(&closing_mutex);
        }
     }


    (void) close_cafe(&cafe, cs);
    (void) generate_report(&cafe, cs);
    free(cs);
    (void) pthread_exit(NULL);
    return EXIT_SUCCESS;
}

