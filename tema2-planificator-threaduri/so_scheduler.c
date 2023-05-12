#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <pthread.h>
#include "./so_scheduler.h"

#define true 1
#define false 0
#define NEW 0
#define READY 1
#define RUNNING 2
#define WAITING 3
#define TERMINATED 4

/**
 * Structura unui thread.
 */
typedef struct thread {
    tid_t tid;                  /* Id-ul thread-ului */
    unsigned int time_quantum;  /* Cuanta de timp */
    unsigned int priority;      /* Prioritatea */
    unsigned int status;        /* Status-ul */
    unsigned int io;            /* Numarul evenimentului IO */

    so_handler *handler;        /* Functia handler */
    sem_t semaphore;            /* Semaforul pentru a semnala daca un thread poate rula */
} thread_t;

/**
 * Structura planificatorului.
 */
typedef struct scheduler {
    int initialized;            /* 0 daca este neinitializat, 1 altfel. */
    unsigned int io;            /* Numarul maxim de evenimente IO */
    unsigned int time_quantum;  /* Cuanta maxima de timp */
    thread_t *running_thread;   /* Pointer catre thread-ul care ruleaza */

    unsigned int nr_threads;
    unsigned int queue_size;    /* Dimensiunea cozii de prioritati */
    sem_t stop;                 /* Semafor care semnaleaza ca poate rula planificatorul */
    thread_t **threads;         /* Coada de thread-uri pornite */
    thread_t **queue;           /* Coada de prioritati */
} scheduler_t;

scheduler_t s;

/**
 * Adauga in coada de prioritati thread-ul primit ca parametru.
 * Coada va avea elementele ordonate in ordine crescatoare a prioritatii.
 * @param thread Thread-ul de adaugat in coada.
 */
void priority_enqueue(thread_t *thread) {
    /* Daca este goala coada, aloca memorie si insereaza elementul */
    if (s.queue == NULL || s.queue_size == 0) {
        s.queue = malloc(1 * sizeof(thread_t *));
        s.queue[0] = thread;
        s.queue[0]->status = READY;
        s.queue_size++;
        return;
    }

    /* Altfel, mareste zona de memorie */
    s.queue = realloc(s.queue, (s.queue_size + 1) * sizeof(thread_t *));

    /* Cauta index-ul la care trebuie inserat elementul */
    unsigned int index = 0;
    for (unsigned int i = 0; i < s.queue_size; i++) {
        if (thread->priority > s.queue[i]->priority) {
            index++;
        } else {
            break;
        }
    }

    /* Shifteaza toate elementele de la index-ul gasit la dreapta cu 1
     * pentru a-i face loc threadului
     */
    for (unsigned int i = s.queue_size; i > index; i--) {
        s.queue[i] = s.queue[i-1];
    }

    /* Insereaza elementul */
    s.queue[index] = thread;
    s.queue[index]->status = READY;
    s.queue_size++;
}

/**
 * Scoate primul element cu cea mai mare prioritate din coada.
 * @return Thread-ul scos din coada.
 */
thread_t *priority_dequeue() {
    /* Iau elementul cu prioritatea cea mai mare */
    thread_t *removed_thread = s.queue[s.queue_size-1];
    /* Realocarea memoriei */
    s.queue = realloc(s.queue, (s.queue_size - 1) * sizeof(thread_t *));
    /* Decrementez dimensiunea cozii */
    s.queue_size--;
    /* Returnez thread-ul */
    return removed_thread;
}

/**
 * Adauga in coada de thread-uri pornite thread-ul primit ca parametru.
 * @param thread Thread-ul de adaugat in coada.
 */
void enqueue(thread_t *thread) {
    /* Daca este prima operatie de enqueue, aloca memorie pentru
     * coada si adauga elementul
     */
    if ((s.nr_threads == 0) != 0) {
        s.threads = malloc(1 * sizeof(thread_t *));
        s.threads[0] = thread;
        s.nr_threads++;
        return;
    }

    /* Realocarea memoriei */
    s.threads = realloc(s.threads, (s.nr_threads + 1) * sizeof(thread_t *));
    /* Inserez elementul */
    s.threads[s.nr_threads] = thread;
    /* Incrementez dimensiunea */
    s.nr_threads++;
}

/**
 * Verifica daca coada de prioritati este goala.
 * @return 1 daca coada este goala, 0 altfel.
 */
int is_priority_empty() {
    return s.queue_size == 0;
}

void *start_thread(thread_t *thread);
void so_scheduler();

/**
 * Functie care initializeaza planificatorul.
 * @param time_quantum Cuanta de timp dupa care un thread trebuie preemptat.
 * @param io Numarul maxim de evenimente IO.
 * @return 0 daca a fost initializat cu success, -1 in caz de eroare.
 */
int so_init(unsigned int time_quantum, unsigned int io) {

    /* Verificarea parametrilor. */
    if (s.initialized == true || time_quantum <= 0 || io > SO_MAX_NUM_EVENTS) {
        return -1;
    }

    /* Initializez campurile planificatorului. */
    s.initialized = true;
    s.io = io;
    s.time_quantum = time_quantum;
    s.running_thread = NULL;
    s.queue = NULL;
    s.queue_size = 0;
    s.threads = NULL;
    s.nr_threads = 0;
    sem_destroy(&s.stop);
    sem_init(&s.stop, 0, 1);

    return 0;
}

/**
 * Aloca memorie pentru un thread nou, il porneste si ii returneaza pointer-ul.
 * @param func Functia handler.
 * @param priority Prioritatea thread-ului.
 * @return Pointer catre noul thread.
 */
thread_t* so_create_thread(so_handler *func, unsigned int priority) {
    thread_t* new_thread = malloc(sizeof(thread_t));

    /* Initializarea campurilor thread-ului */
    new_thread->tid = -1;
    new_thread->time_quantum = s.time_quantum;
    new_thread->priority = priority;
    new_thread->status = NEW;
    new_thread->io = SO_MAX_NUM_EVENTS;
    new_thread->handler = func;
    sem_destroy(&new_thread->semaphore);
    sem_init(&new_thread->semaphore, 0, 0);

    /* Pornesc thread-ul si il returnez */
    pthread_create(&new_thread->tid, NULL, (void *(*)(void *))start_thread, (void *)new_thread);
    return new_thread;
}

/**
 * Functie care creaza un nou thread si o planifica.
 * @param func Functia handler.
 * @param priority Prioritatea thread-ului.
 * @return ID-ul thread-ului.
 */
tid_t so_fork(so_handler *func, unsigned int priority) {

    /* Verificarea parametrilor. */
    if (func == NULL || priority > SO_MAX_PRIO) {
        return INVALID_TID;
    }

    /* Daca nu a fost pornit niciun thread, planificatorul asteapta la semafor */
    if (s.nr_threads == 0) {
        sem_wait(&s.stop);
    }

    /* Creez un thread nou */
    thread_t *new_thread = so_create_thread(func, priority);

    /* Il pun in lista de thread-uri pornite */
    enqueue(new_thread);
    /* Il adaug la coada de prioritati */
    priority_enqueue(new_thread);

    /* Apeleaza so_scheduler() pentru a planifica thread-ul nou */
    if (s.running_thread == NULL) {
        so_scheduler();
    } else {
        so_exec();
    }

    return new_thread->tid;
}

/**
 * Simuleaza executia unei instructiuni.
 */
void so_exec(void) {
    thread_t *running_thread = s.running_thread;

    /* Daca ruleaza un thread */
    if (running_thread != NULL) {
        /* Scade din timp */
        running_thread->time_quantum--;

        /* Apeleaza so_scheduler() */
        so_scheduler();

        /* Asteapta la semafor daca thread-ul este preemptat */
        sem_wait(&running_thread->semaphore);
    } else {
        return;
    }
}

/**
 * Blocheaza thread-ul care ruleaza pana cand se primeste un semnal pentru deblocare.
 * @param io Id-ul evenimentului IO care trebuie sa deblocheze thread-ul.
 * @return 0 daca se executa cu succes operatiunea, -1 in caz de eroare.
 */
int so_wait(unsigned int io) {
    /* Verificarea parametrilor */
    if (io >= s.io) {
        return -1;
    }

    /* Seteaza status-ul thread-ului care ruleaza la WAITING */
    thread_t *running_thread = s.running_thread;
    running_thread->status = WAITING;
    running_thread->io = io;

    so_exec();
    return 0;
}

/**
 * Deblocheaza thread-urile care asteptau evenimentul cu id-ul primit ca parametru.
 * @param io Id-ul evenimentului.
 * @return Numarul de thread-uri deblocate.
 */
int so_signal(unsigned int io) {
    /* Verificarea parametrilor */
    if (io >= s.io) {
        return -1;
    }

    /* Daca exista thread-uri care sunt blocate */
    if (s.nr_threads != 0) {
        /* Contorul thread-urilor care vor fi deblocate */
        int count = 0;

        /* Parcurge toate thread-urile care sunt in starea WAITING si asteapta
         * evenimentul cu id-ul primit ca parametru.
         */
        for (int i = 0; i < s.nr_threads; i++) {
            if (s.threads[i]->status == WAITING && s.threads[i]->io == io) {
                /* Seteaza starea thread-ului la READY si pune-l inapoi in coada
                 * de prioritati.
                 */
                s.threads[i]->status = READY;
                priority_enqueue(s.threads[i]);
                count++;
            }
        }
        so_exec();
        return count;
    }

    so_exec();
    return 0;
}

/**
 * Asteapta terminarea thread-urilor si apoi elibereaza memoria folosita.
 */
void so_end(void) {
    if (s.initialized == false) {
        return;
    }

    /* Semnaleaza ca planificatorul nu poate rula */
    sem_wait(&s.stop);

    /* Asteapta ca fiecare thread sa se termine */
    for (int i = 0; i < s.nr_threads; i++) {
        pthread_join(s.threads[i]->tid, NULL);
    }

    /* Elibereaza memoria alocata pentru fiecare thread */
    for (int i = 0; i < s.nr_threads; i++) {
        sem_destroy(&s.threads[i]->semaphore);
        free(s.threads[i]);
    }

    sem_destroy(&s.stop);
    s.initialized = false;
}

/**
 * Simuleaza executia unui thread.
 * @param thread Thread-ul de simulat.
 */
void run_thread(thread_t *thread) {
    thread_t *removed = priority_dequeue();
    // free(removed);
    /* Seteaza starea thread-ului */
    thread->status = RUNNING;

    /* Seteaza cuanta de timp */
    thread->time_quantum = s.time_quantum;

    /* Thread-ul poate rula */
    sem_post(&thread->semaphore);
}

/**
 * Planifica sau replanifica thread-urile dupa modelul Round-robin.
 */
void so_scheduler() {
    thread_t *running_thread = s.running_thread, *next_thread;

    /* Daca nu este goala coada, */
    if (!is_priority_empty()) {
        /* Ia primul thread din coada */
        next_thread = s.queue[s.queue_size-1];
    } else {
        /* Thread-ul care ruleaza este ultimul, se poate opri
         * planificatorul daca thread-ul si-a terminat executia
         */
        if (running_thread->status == TERMINATED) {
            sem_post(&s.stop);
        }

        sem_post(&running_thread->semaphore);
        return;
    }

    /* Daca nu ruleaza niciun thread, sau daca thread-ul care ruleaza este
     * in starea de WAITING sau a terminat de executat, porneste urmatorul thread
     */
    if (!running_thread ||
        running_thread->status == WAITING ||
        running_thread->status == TERMINATED) {

        s.running_thread = next_thread;
        run_thread(next_thread);
        return;
    }

    /* Daca exista un thread cu prioritate mai mare, preempteaza thread-ul care
     * ruleaza si ruleaza-l pe cel cu prioritate mai mare
     */
    if (running_thread != NULL && running_thread->priority < next_thread->priority) {
        priority_enqueue(running_thread);
        s.running_thread = next_thread;
        run_thread(next_thread);
        return;
    }

    /* Verifica daca i-a expirat cuanta de timp a thread-ului care ruleaza */
    if (running_thread != NULL && running_thread->time_quantum <= 0) {

        /* Ruleaza urmatorul thread daca are prioritate egala cu cea
         * a thread-ului caruia i-a expirat cuanta de timp
         */
        if (running_thread->priority == next_thread->priority) {
            priority_enqueue(running_thread);
            s.running_thread = next_thread;
            run_thread(next_thread);
            return;
        } else {
            /* Daca nu exista in coada un thread cu prioritatea mai mare sau
             * egala cu cel a carui cuanta a expirat, este replanificat
             * acelasi thread si ii este resetata cuanta la valoarea maxima.
             */
            running_thread->time_quantum = s.time_quantum;
            /* Thread-ul poate rula */
            sem_post(&running_thread->semaphore);
            return;
        }
    }
    sem_post(&running_thread->semaphore);
}

/**
 * Functie care va rula handler-ul pentru fiecare thread.
 * @param thread Thread-ul al carui handler trebuie rulat.
 * @return NULL
 */
void *start_thread(thread_t *thread) {
    sem_wait(&thread->semaphore);

    /* Ruleaza handler-ul */
    thread->handler(thread->priority);

    /* Seteaza starea thread-ului la TERMINATED */
    thread->status = TERMINATED;
    so_scheduler();
    return 0;
}
