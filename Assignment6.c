/*
 * Assignment6: Majority Voting with Pipes
 *
 * One judge (parent process) and N voters (child processes).
 * Each round, every voter randomly votes YES/NO, prints its vote, and writes
 * it to its own pipe. The judge reads one vote from each pipe, counts them,
 * and prints the majority decision.
 *
 * Compile: gcc -Wall -o Assignment6 Assignment6.c
 * Run:     ./Assignment6
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

int main(void)
{
    int n, rounds;

    /* ---------- Step 1: get user input ---------- */
    printf("Enter the number of voters: ");
    if (scanf("%d", &n) != 1 || n < 3) {
        fprintf(stderr, "Number of voters must be an integer >= 3.\n");
        return 1;
    }
    printf("Enter the number of rounds: ");
    if (scanf("%d", &rounds) != 1 || rounds < 1) {
        fprintf(stderr, "Number of rounds must be an integer >= 1.\n");
        return 1;
    }

    /* ---------- Step 2 (1st for loop): create N pipes ---------- */
    /* pipes[i][0] = read end, pipes[i][1] = write end.
     * Voter i writes to pipes[i][1]; the judge reads from pipes[i][0]. */
    int (*pipes)[2] = malloc(n * sizeof(int[2]));
    if (pipes == NULL) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }

    /* ---------- Step 3 (2nd for loop): create N children ---------- */
    /* voter_id == -1 identifies the judge; children get their loop index.
     * Only the judge calls fork(); children do NOT exit here, they fall
     * out of the loop (via break) and continue to Step 4. */
    int voter_id = -1;
    fflush(stdout);                 /* avoid duplicated buffered output */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid == 0) {             /* child: remember id and stop forking */
            voter_id = i;
            break;
        }
    }

    /* Seed the RNG differently in every process, otherwise all children
     * would produce identical vote sequences. */
    if (voter_id >= 0)
        srand((unsigned)time(NULL) ^ ((unsigned)getpid() << 8));

    /* Close pipe ends this process does not need. */
    for (int i = 0; i < n; i++) {
        if (voter_id == -1) {
            close(pipes[i][1]);     /* judge only reads */
        } else {
            close(pipes[i][0]);     /* voters never read */
            if (i != voter_id)
                close(pipes[i][1]); /* voter only writes to its own pipe */
        }
    }

    /* ---------- Step 4 (3rd for loop): voting rounds ---------- */
    for (int r = 1; r <= rounds; r++) {
        if (voter_id == -1) {
            /* ===== Judge ===== */
            printf("\n------ Round %d -------\n", r);
            fflush(stdout);

            int yes = 0, no = 0;
            /* (4th for loop) collect one vote from each voter */
            for (int i = 0; i < n; i++) {
                int vote;
                if (read(pipes[i][0], &vote, sizeof(vote)) != sizeof(vote)) {
                    perror("read");
                    exit(1);
                }
                if (vote) yes++; else no++;
            }

            if (yes > no)
                printf("--- Round %d: The voting result is a YES.\n", r);
            else if (no > yes)
                printf("--- Round %d: The voting result is a NO.\n", r);
            else
                printf("--- Round %d: The voting result is a TIE.\n", r);
            fflush(stdout);
        } else {
            /* ===== Voter ===== */
            int vote = rand() % 2;  /* 1 = YES, 0 = NO */
            printf("Round %d: Voter %d votes: %s\n", r, voter_id,
                   vote ? "YES" : "NO");
            fflush(stdout);
            /* No loop: voter i writes only to its own pipe i */
            if (write(pipes[voter_id][1], &vote, sizeof(vote)) != sizeof(vote)) {
                perror("write");
                exit(1);
            }
        }
    }

    /* ---------- Cleanup: terminate all processes properly ---------- */
    if (voter_id == -1) {
        for (int i = 0; i < n; i++)
            close(pipes[i][0]);
        for (int i = 0; i < n; i++)
            wait(NULL);             /* reap every child */
        free(pipes);
    } else {
        close(pipes[voter_id][1]);
        free(pipes);
        exit(0);                    /* voters exit only after all rounds */
    }

    return 0;
}
