
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP(msecs) sleep(msecs)
#elif __unix 
    #include <unistd.h>
#else
    error "Unknown OS"
#endif

/* 
 * Time conventions used :
 *
 * 3 Seconds -> 1 Day
 * 1 Seconds  -> 8 Hours / 28800 Seconds
 * 1 / 28800 Seconds -> 1 Seconds
 * 34.72 Microseconds -> 1 Seconds
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
 *
 * Algumas suposições : Se um cliente chegar 
 * e pedir um tempo acima do tempo faltando para
 * fechar o cyber_cafe, ele é rejeitado, e o
 * tempo de recursos dele é o tempo faltando 
 * para fechar o café.
 *
*/

#define DAY 3
#define _8HOURS 1
#define SECOND 0.00003472222

typedef enum {
    Gamer,
    Freelancer,
    Student
} client_type;

typedef struct {
    sem_t sem;
}pc;

typedef struct {
    sem_t sem;
}headset;

typedef struct {
    sem_t sem;
}seat;

typedef struct{
    pthread_t p; 
    client_type t;
    float time;
    int UID;
} client;

typedef struct{
    pc *pcs;
    size_t p_used;
    size_t n_pcs;

    headset *headsets;
    size_t h_used;
    size_t n_headsets;

    seat *seats;
    size_t s_used;
    size_t n_seats;

    size_t n_gamers;
    size_t n_freelancers;
    size_t n_students;

    size_t timed_out;

    float total_time;
    float time_left;
    size_t c_size;
}cyber_flux;

void open_cafe(cyber_flux *f)
{
  assert(f!=NULL);

  f->n_pcs = 10;
  f->n_headsets = 6;
  f->n_seats = 8;

  f->pcs = (pc *) malloc(sizeof(pc));     
  f->headsets = (headset *) malloc(sizeof(headset));     
  f->seats = (seat *) malloc(sizeof(seat));     

  f->s_used = 0;
  f->h_used = 0;
  f->p_used = 0;

  f->n_gamers = 0;
  f->n_freelancers = 0;
  f->n_students = 0;

  f->timed_out = 0;

  sem_init(&(f->pcs->sem), 0, f->n_pcs);
  sem_init(&(f->headsets->sem), 0, f->n_headsets);
  sem_init(&(f->seats->sem), 0, f->n_seats);

  f->total_time = 1;
  f->c_size = 0;
  f->time_left = f->total_time;
}

/*
 * Random float in the range [0, 1] inclusive.
*/
float randomFloat()
{
   float r = (float)rand() / (float)RAND_MAX;
   assert(r>=0&&r<=1);
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
    double real_hours = s_time * 8;
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

cyber_flux cafe = {0};

pthread_mutex_t inc_mutex      = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_gamers     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_students   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inc_freelancers= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t time_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rand_mutex     = PTHREAD_MUTEX_INITIALIZER;

void increment_pcs()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.p_used++;
  pthread_mutex_unlock(&inc_mutex); 
}

void increment_headsets()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.h_used++;
  pthread_mutex_unlock(&inc_mutex); 
}

void increment_seats()
{
  pthread_mutex_lock(&inc_mutex);
  cafe.s_used++;
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

void *new_client(void *d)
{
  client *c = (client *) d;
  assert(c!=NULL);

  // Time of arrival at the cafe
  // this simulates random client arrival
  // we have to lock since main is reducing
  // cafe.time_left.
  pthread_mutex_lock(&time_mutex);
  float arrival = cafe.total_time - cafe.time_left;
  pthread_mutex_unlock(&time_mutex); 

  real_time art = cfsttr(arrival);
  char *art_s = real_time_string(&art);

  pthread_mutex_lock(&rand_mutex);
  c->t = (client_type) (rand() % 3);
  c->time = randomFloat();
  assert(c->t>=0&&c->t<=2);
  pthread_mutex_unlock(&rand_mutex);

  (void) sleep(arrival);     
  (void) fprintf(stdout, "Client {%d} : {%d} arrived at the cafe at time %s\n",
          c->UID, c->t, art_s);

  (void) free(art_s);

  // Tempo entre 0 e tempo máximo
  // de funcionamento do café
  // ( Tempo de uso dos recursos pelo cliente )
  (void) pthread_mutex_lock(&time_mutex);
  if (c->time > cafe.time_left)
  {
      cafe.timed_out++;
      real_time art = cfsttr(c->time);
      real_time art_t = cfsttr(cafe.time_left);
      char *art_s = real_time_string(&art);
      char *art_ss = real_time_string(&art_t);
      (void) fprintf(stdout, "Client {%d} : {%d} wanted to use resources for %s\n"
              "but the cafe would close, instead stayed for %s\n",
              c->UID, c->t, art_s, art_ss);
      (void) free(art_s); (void) free(art_ss);
      c->time = cafe.time_left;
  }
  (void) pthread_mutex_unlock(&time_mutex); 

  switch(c->t)
  {
      case Gamer:{
          increment_gamers();

          // TODO: add wait time measurements ?
          sem_wait(&cafe.pcs->sem);         
          increment_pcs();

          sem_wait(&cafe.headsets->sem);
          increment_headsets();
          sem_wait(&cafe.seats->sem); 
          increment_seats();

          real_time art = cfsttr(c->time);
          char *art_s = real_time_string(&art);

          sleep(c->time);
          (void) fprintf(stdout, "Client {%d} : {%d} used the resources for %s\n",
                  c->UID, c->t, art_s);
          free(art_s);
          

          sem_post(&cafe.seats->sem); 
          sem_post(&cafe.headsets->sem);
          sem_post(&cafe.pcs->sem);
          break;
      }
      case Freelancer:{
          increment_freelancers();
          sem_wait(&cafe.pcs->sem);         
          increment_pcs();

          sem_wait(&cafe.seats->sem);         
          increment_seats();
          sem_wait(&cafe.headsets->sem);         
          increment_headsets();

          real_time art = cfsttr(c->time);
          char *art_s = real_time_string(&art);
          sleep(c->time);
          (void) fprintf(stdout, "Client {%d} : {%d} used the resources for %s\n",
                  c->UID, c->t, art_s);
          free(art_s);

          sem_post(&cafe.headsets->sem);         
          sem_post(&cafe.seats->sem);
          sem_post(&cafe.pcs->sem);
          break;
      }
      case Student:{
          increment_students();
          sem_wait(&cafe.pcs->sem);         
          increment_pcs();

          real_time art = cfsttr(c->time);
          char *art_s = real_time_string(&art);
          sleep(c->time);
          (void) fprintf(stdout, "Client {%d} : {%d} used the resources for %s\n",
                  c->UID, c->t, art_s);
          free(art_s);

          sem_post(&cafe.pcs->sem);
          break;
      }
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
       (void) pthread_join(cs[i].p, NULL);
    }

    sem_destroy(&f->pcs->sem);
    sem_destroy(&f->seats->sem);
    sem_destroy(&f->headsets->sem);

    (void) free(f->pcs);
    (void) free(f->seats);
    (void) free(f->headsets);
    (void) free(cs);
}

void generate_report(cyber_flux *f)
{

   (void) fprintf(stdout, "\nStatistical Report : \n");
   (void) fprintf(stdout, "0 - Gamer, 1 - Freelancer, 2 - Student\n");
   (void) fprintf(stdout, "\nTotal number of clients in 8 hours : %lu\n",
           f->c_size); 
   (void) fprintf(stdout, "Number of times the resources were used :\n\n"
           " PCS : %lu\n HEADSETS : %lu\n SEATS : %lu\n\n", f->p_used,
           f->h_used, f->s_used); 
   (void) fprintf(stdout, "Total number of types of clients :\n\n"
           " GAMERS : %lu\n FREELANCERS : %lu\n STUDENTS : %lu\n\n",
           f->n_gamers, f->n_freelancers, f->n_students); 
   (void) fprintf(stdout, "Clients that timed out : %lu\n\n", f->timed_out);
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

int main(void)
{
    client *cs = NULL;   
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
        //
        
        pthread_mutex_lock(&rand_mutex);
        int r_prob = rand() % 100;
        pthread_mutex_unlock(&rand_mutex);

        if(r_prob==5)
        {
            size_t cc = cafe.c_size;
            // Client wants to go to the cafe, this makes it so that
            // he cannot change his mind mid "commute there" etc.
            // The time that he arrives there is the random value.
            client *n_cs = (client *) realloc(cs, sizeof(client) * (++(cafe.c_size)));
            if(!n_cs)
            {
                perror("realloc failed");
                exit(EXIT_FAILURE);
            }

            cs = n_cs;

            cs[cc].UID = cafe.c_size;
            int err = pthread_create(&(cs[cc].p), NULL, new_client, (void *) &cs[cc]);
            if (err!=0)
            {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
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

        pthread_mutex_lock(&time_mutex);
        cafe.time_left = clamp(cafe.time_left - SECOND);
        cond = cafe.time_left > 0;
        pthread_mutex_unlock(&time_mutex);

     }

    (void) close_cafe(&cafe, cs);
    (void) generate_report(&cafe);
    (void) pthread_exit(NULL);
    return EXIT_SUCCESS;
}

