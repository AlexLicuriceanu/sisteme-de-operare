/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>
#include <fcntl.h>

#include "exec_parser.h"

static so_exec_t *exec;
static struct sigaction default_sa;     // handler default pentru semnale
static struct stat st;
static int pageSize;                    // dimensiunea unei pagini
static int fd;                          // file descriptor
static char* execPtr;

typedef struct segmentInfo {
    int *isPageMapped;
} segmentInfo;

static void segv_handler(int signum, siginfo_t *info, void *context) {

    // default action pentru orice semnal in afara de segfault
    if (signum != SIGSEGV) {
        default_sa.sa_sigaction(signum, info, context);
        return;
    }

    int rc;
    unsigned long faultAddr = (unsigned long)info->si_addr;

    for (int i=0; i<exec->segments_no; i++) {
        so_seg_t *segment = &exec->segments[i];

        // daca adresa segfault-ului s-a produs undeva intre adresa de inceput si adresa de final a segmentului
        if (segment->vaddr <= faultAddr && faultAddr <= segment->vaddr + segment->mem_size) {
            segmentInfo *currentSegmentInfo = (segmentInfo *)segment->data;

            unsigned long pageNumber = (faultAddr - segment->vaddr) / pageSize;    // numarul paginii din cadrul segmentului la care s-a produs segfault
            unsigned long nrPagesFile = ceil(segment->file_size / pageSize);       // numarul de pagini necesare pentru a mapa file_size
            if (segment->file_size % pageSize) {
                nrPagesFile++;                  // daca file_size nu e multiplu de pageSize, mai trebuie inca o pagina in plus ca sa incapa datele
            }

            // daca pagina nu este mapata in memorie
            if (!currentSegmentInfo->isPageMapped[pageNumber]) {
                unsigned long offset = pageNumber * pageSize;

                // mapam pagina, initial cu drepturi de scriere
                char *mappedAddr = mmap((void *)(segment->vaddr + offset), pageSize,
                                        PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
                if (mappedAddr == MAP_FAILED) {
                    perror("mmap");
                    exit(EXIT_FAILURE);
                }

                unsigned long nrBytes = pageSize;       // initial, avem de copiat pageSize bytes

                /* daca numarul paginii este mai mare decat numarul de pagini necesare pentru a
                 * mapa file_size bytes, nu copiem nimic
                 */
                if (pageNumber >= nrPagesFile) {
                    nrBytes = 0;
                }
                else if (segment->file_size % pageSize != 0 && pageNumber == nrPagesFile - 1) {
                    /* daca file_size nu e multiplu de pageSize, ultima pagina a segmentului
                     * nu este plina si nu va contine pageSize bytes de date, ci
                     * file_size % pageSize bytes
                     */
                    nrBytes = segment->file_size % pageSize;
                }

                // copiem in pagina mapata anterior, nrBytes bytes de la adresa care a provocat segfault
                memcpy(mappedAddr, execPtr + segment->offset + offset, nrBytes);

                // schimbam drepturile maparii conform permisiunilor segmentului
                rc = mprotect(mappedAddr, pageSize, segment->perm);
                if (rc < 0) {
                    perror("mprotect");
                    exit(EXIT_FAILURE);
                }

                currentSegmentInfo->isPageMapped[pageNumber] = 1;       // marcam pagina ca fiind mapata
                return;
            } else {
                default_sa.sa_sigaction(signum, info, context);         // pagina este deja mapata in memorie, rulam handler-ul default
                return;
            }
        }
    }
    default_sa.sa_sigaction(signum, info, context);
}

int so_init_loader(void) {

    int rc;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO;
    rc = sigaction(SIGSEGV, &sa, NULL);
    if (rc < 0) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int so_execute(char *path, char *argv[]) {
    exec = so_parse_exec(path);
    if (!exec) {
        return -1;
    }

    pageSize = getpagesize();
    for (int i = 0; i < exec->segments_no; i++) {
        so_seg_t* segment = &exec->segments[i];

        segmentInfo* currentSegmentInfo = malloc(sizeof(segmentInfo));      // alocam memorie structurii
        unsigned long nrPages = ceil(segment->mem_size / pageSize);         // calculam numarul de pagini necesare segmentului

        currentSegmentInfo->isPageMapped = calloc(nrPages, sizeof(int));    // alocam memorie vectorului de aparitii
        segment->data = currentSegmentInfo;                                 // asignam segmentului aferent structura
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    fstat(fd, &st);
    unsigned long fileSize = st.st_size;   // dimensiunea fisierului

    execPtr = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (execPtr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    so_start_exec(exec, argv);

    return -1;
}
