Nume: Alexandru Licuriceanu
Grupă: 325CD

# Tema 2 - Planificator de thread-uri

Organizare
-
1. Planificatorul este continut intr-o structura proprie, care include pe langa alte campuri, doua cozi, una pentru a retine thread-urile pornite, alta pentru a ordona crescator thread-urile in functie de prioritate, astfel thread-ul cu cea mai mare prioritate se afla la sfarsitul cozii.
2. Structura unui thread are pe langa campurile aferente, un semafor care semnalizeaza daca thread-ul poate sa ruleze.

```c
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
```
```c
typedef struct thread {
    tid_t tid;                  /* Id-ul thread-ului */
    unsigned int time_quantum;  /* Cuanta de timp */
    unsigned int priority;      /* Prioritatea */
    unsigned int status;        /* Status-ul */
    unsigned int io;            /* Numarul evenimentului IO */

    so_handler *handler;        /* Functia handler */
    sem_t semaphore;            /* Semaforul pentru a semnala daca un thread poate rula */
} thread_t;
```

* Programul se foloseste de modelul Round-robin pentru a planifica thread-urile in functie si de prioritatea acestora. Detalii legate de implementarile fiecarei functii se afla in comentariile din fisierul sursa.

Implementare
-

* Este implementat tot enuntul.
* Am intampinat ceva dificultati cu intelegerea enuntului si de asemenea cu lucrul cu memoria, ceva normal cand scrii in C de altfel.

Cum se compilează și cum se rulează?
-
Compilare: make\
Rulare: ./libscheduler.so\
Rulare cu checker: make -f Makefile.checker

Bibliografie
-

* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-08
