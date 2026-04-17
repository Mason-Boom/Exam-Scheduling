/*
 * exam_scheduler_ga.c
 *
 * Genetic Algorithm exam scheduler.
 *
 * Chromosome  = one complete schedule (all exams assigned to slots)
 * Gene        = one exam's assigned slot index
 * Fitness     = 10000  minus  penalties for student / room conflicts
 *
 * Same JSON input format as the backtracking version:
 *   students file  → { "students": [ { "studentID": ..., "enrolledCourses": [...] } ] }
 *   exams file     → { "departments": [ { "courses": [ { "courseID": ... } ] } ] }
 *   locations file → { "locations": [ "RoomA", "RoomB", ... ] }
 *
 * Compile:
 *   gcc -O2 -o exam_scheduler_ga exam_scheduler_ga.c cJSON.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "cJSON.h"

/* ─── tuneable string limits ───────────────────────────────────────────── */
#define MAX_DAY_LEN   8
#define MAX_TIME_LEN  32
#define MAX_LOC_LEN   128
#define MAX_ID_LEN    64

/* ─── GA hyper-parameters ──────────────────────────────────────────────── */
/* Adjust these to trade off quality vs. runtime.
 *
 *  More courses  → raise POPULATION_SIZE and / or NUM_GENERATIONS.
 *  MUTATION_RATE is per-gene, so ~4 % of all genes flip each generation.
 *  ELITISM_COUNT top chromosomes are copied unchanged to preserve the best.
 *  TOURNAMENT_SIZE controls selection pressure (higher = greedier).
 */
#define POPULATION_SIZE  1000
#define NUM_GENERATIONS  1000
#define MUTATION_RATE    0.04   /* per-gene probability of re-randomisation */
#define ELITISM_COUNT    10     /* top N survive each generation unaltered   */
#define TOURNAMENT_SIZE  5      /* candidates evaluated per tournament pick  */

/* Base fitness — penalties are subtracted from this */
#define BASE_FITNESS             10000
#define STUDENT_CONFLICT_PENALTY   100   /* two exams same (day,time) for a student */
#define ROOM_CONFLICT_PENALTY       50   /* two exams in the exact same (day,time,room) */

/* ─── data structures ──────────────────────────────────────────────────── */

typedef struct {
    char day     [MAX_DAY_LEN ];
    char time    [MAX_TIME_LEN];
    char location[MAX_LOC_LEN ];
} Slot;

/*
 * Student stores indices into the global exams[] array rather than
 * raw course-ID strings, so the fitness function never calls strcmp().
 */
typedef struct {
    char studentID[MAX_ID_LEN];
    int *course_idx;   /* exam array indices for this student's courses */
    int  num_courses;
} Student;

typedef struct {
    int *genes;    /* genes[i] = slot_list index assigned to exam i */
    int  fitness;
} Chromosome;

/* ─── globals ──────────────────────────────────────────────────────────── */

static Slot    *slot_list    = NULL;
static int      num_slots    = 0;

static char   **exams        = NULL;
static int      num_exams    = 0;

static Student *students     = NULL;
static int      num_students = 0;

/* ─── tiny RNG helpers ─────────────────────────────────────────────────── */

/* Uniform integer in [lo, hi) */
static inline int rand_int(int lo, int hi)
{
    return lo + rand() % (hi - lo);
}

/* Uniform double in [0, 1) */
static inline double rand_double(void)
{
    return rand() / (RAND_MAX + 1.0);
}

/* ─── fitness ──────────────────────────────────────────────────────────── */

/*
 * compute_fitness()
 *
 * Iterates over every student's course-pair to detect time clashes, then
 * over every exam-pair to detect duplicate-slot (room) clashes.
 *
 * Complexity per evaluation: O(S·C² + E²)
 * where S = students, C = max courses per student, E = total exams.
 */
static int compute_fitness(const int *genes)
{
    int score = BASE_FITNESS;

    /* ── student conflicts ────────────────────────────────────────────── */
    for (int si = 0; si < num_students; si++) {
        const Student *st = &students[si];
        for (int a = 0; a < st->num_courses; a++) {
            int ia = st->course_idx[a];
            if (ia < 0) continue;          /* course not in exam list — skip */
            for (int b = a + 1; b < st->num_courses; b++) {
                int ib = st->course_idx[b];
                if (ib < 0) continue;
                const Slot *sa = &slot_list[genes[ia]];
                const Slot *sb = &slot_list[genes[ib]];
                /* Conflict: same day AND same time (different rooms are OK) */
                if (strcmp(sa->day,  sb->day ) == 0 &&
                    strcmp(sa->time, sb->time) == 0)
                    score -= STUDENT_CONFLICT_PENALTY;
            }
        }
    }

    /* ── room conflicts ───────────────────────────────────────────────── */
    /*
     * Because each (day, time, location) triple is a unique index in
     * slot_list, two equal gene values means the same room at the same
     * time — a hard room-capacity conflict.
     */
    for (int a = 0; a < num_exams; a++)
        for (int b = a + 1; b < num_exams; b++)
            if (genes[a] == genes[b])
                score -= ROOM_CONFLICT_PENALTY;

    return score;
}

/* ─── chromosome memory management ────────────────────────────────────── */

static Chromosome *alloc_chromosome(void)
{
    Chromosome *c = malloc(sizeof(Chromosome));
    if (!c) { perror("malloc"); exit(1); }
    c->genes = malloc((size_t)num_exams * sizeof(int));
    if (!c->genes) { perror("malloc"); exit(1); }
    c->fitness = 0;
    return c;
}

static void free_chromosome(Chromosome *c)
{
    if (c) { free(c->genes); free(c); }
}

/* Deep-copy src → dst (both must already own a genes buffer) */
static void copy_chromosome(Chromosome *dst, const Chromosome *src)
{
    memcpy(dst->genes, src->genes, (size_t)num_exams * sizeof(int));
    dst->fitness = src->fitness;
}

/* Assign every gene a uniformly random slot index and score it */
static void randomize(Chromosome *c)
{
    for (int i = 0; i < num_exams; i++)
        c->genes[i] = rand_int(0, num_slots);
    c->fitness = compute_fitness(c->genes);
}

/* ─── genetic operators ────────────────────────────────────────────────── */

/*
 * Tournament selection — samples TOURNAMENT_SIZE random individuals and
 * returns the one with the highest fitness.
 */
static const Chromosome *tournament(Chromosome **pop, int pop_size)
{
    const Chromosome *best = pop[rand_int(0, pop_size)];
    for (int i = 1; i < TOURNAMENT_SIZE; i++) {
        const Chromosome *c = pop[rand_int(0, pop_size)];
        if (c->fitness > best->fitness)
            best = c;
    }
    return best;
}

/*
 * Single-point crossover — genes [0, point) from p1, [point, n) from p2.
 * Fitness is NOT recomputed here; the caller does that after mutation.
 */
static void crossover(const Chromosome *p1, const Chromosome *p2,
                      Chromosome *child)
{
    int point = rand_int(1, num_exams);   /* at least one gene from each parent */
    for (int i = 0;     i < point;     i++) child->genes[i] = p1->genes[i];
    for (int i = point; i < num_exams; i++) child->genes[i] = p2->genes[i];
}

/*
 * Per-gene mutation — each gene is replaced with a fresh random slot index
 * with probability MUTATION_RATE.
 */
static void mutate(Chromosome *c)
{
    for (int i = 0; i < num_exams; i++)
        if (rand_double() < MUTATION_RATE)
            c->genes[i] = rand_int(0, num_slots);
}

/* ─── qsort comparator (descending fitness) ────────────────────────────── */

static int cmp_chrom_desc(const void *a, const void *b)
{
    const Chromosome *ca = *(const Chromosome *const *)a;
    const Chromosome *cb = *(const Chromosome *const *)b;
    /* Subtract in reverse to sort highest-fitness first */
    return cb->fitness - ca->fitness;
}

/* ─── JSON I/O helpers ─────────────────────────────────────────────────── */

/* Read an entire file into a heap-allocated string (caller must free) */
static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open: %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { perror("malloc"); exit(1); }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* Write the best chromosome to disk in the same format as the backtracker */
static void save_best(const char *path, const Chromosome *best)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot write: %s\n", path); return; }

    cJSON *obj = cJSON_CreateObject();
    for (int i = 0; i < num_exams; i++) {
        const Slot *s   = &slot_list[best->genes[i]];
        cJSON      *slot_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(slot_obj, "day",      s->day);
        cJSON_AddStringToObject(slot_obj, "time",     s->time);
        cJSON_AddStringToObject(slot_obj, "location", s->location);
        cJSON_AddItemToObject(obj, exams[i], slot_obj);
    }

    char *json_str = cJSON_Print(obj);
    fprintf(f, "%s\n", json_str);
    free(json_str);
    cJSON_Delete(obj);
    fclose(f);

    printf("Saved best schedule (fitness = %d / %d) to %s\n",
           best->fitness, BASE_FITNESS, path);
}

/* ─── main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    srand((unsigned)time(NULL));

    /* ── prompt for file paths ────────────────────────────────────────── */

    char students_file[256], exam_file[256], space_file[256], save_file[256];
    char students_path[512], exam_path[512], space_path[512], save_path[512];

    printf("Input students json file (e.g., GAStudents.json): ");
    if (!fgets(students_file, sizeof students_file, stdin)) return 1;
    students_file[strcspn(students_file, "\n")] = '\0';
    snprintf(students_path, sizeof students_path, "../Data/Students/%s", students_file);

    printf("Input exam json file (e.g., GACourses.json): ");
    if (!fgets(exam_file, sizeof exam_file, stdin)) return 1;
    exam_file[strcspn(exam_file, "\n")] = '\0';
    snprintf(exam_path, sizeof exam_path, "../Data/Courses/%s", exam_file);

    printf("Input file for locations (e.g., BFLocations.json): ");
    if (!fgets(space_file, sizeof space_file, stdin)) return 1;
    space_file[strcspn(space_file, "\n")] = '\0';
    snprintf(space_path, sizeof space_path, "../Data/Locations/%s", space_file);

    printf("Input file to save results (e.g., GAOutput.json): ");
    if (!fgets(save_file, sizeof save_file, stdin)) return 1;
    save_file[strcspn(save_file, "\n")] = '\0';
    snprintf(save_path, sizeof save_path, "../Data/Schedules/%s", save_file);

    /* Hard-coded days and timeslots — identical to both Python and C backtracker */
    const char *days[]      = {"M", "T", "W", "R", "F"};
    int         num_days    = 5;
    const char *timeslots[] = {"8:00am-10:00am", "10:30am-12:30pm",
                               "1:00pm-3:00pm",  "3:30pm-5:30pm"};
    int         num_timeslots = 4;

    /* ── load exams ───────────────────────────────────────────────────── */
    {
        int    cap  = 64;
        exams = malloc((size_t)cap * sizeof(char *));
        if (!exams) { perror("malloc"); return 1; }

        char  *raw  = read_file(exam_path);
        cJSON *root = cJSON_Parse(raw);
        free(raw);
        if (!root) { fprintf(stderr, "Bad exam JSON\n"); return 1; }

        cJSON *depts = cJSON_GetObjectItem(root, "departments"), *dept;
        cJSON_ArrayForEach(dept, depts) {
            cJSON *courses = cJSON_GetObjectItem(dept, "courses"), *course;
            cJSON_ArrayForEach(course, courses) {
                if (num_exams >= cap) {
                    cap *= 2;
                    exams = realloc(exams, (size_t)cap * sizeof(char *));
                    if (!exams) { perror("realloc"); return 1; }
                }
                exams[num_exams++] =
                    strdup(cJSON_GetObjectItem(course, "courseID")->valuestring);
            }
        }
        cJSON_Delete(root);
        printf("Loaded %d exams.\n", num_exams);
    }

    /* ── load locations ───────────────────────────────────────────────── */
    char **locations     = NULL;
    int    num_locations = 0;
    {
        char  *raw  = read_file(space_path);
        cJSON *root = cJSON_Parse(raw);
        free(raw);
        if (!root) { fprintf(stderr, "Bad locations JSON\n"); return 1; }

        cJSON *arr    = cJSON_GetObjectItem(root, "locations");
        num_locations = cJSON_GetArraySize(arr);
        locations     = malloc((size_t)num_locations * sizeof(char *));
        if (!locations) { perror("malloc"); return 1; }
        for (int i = 0; i < num_locations; i++)
            locations[i] = strdup(cJSON_GetArrayItem(arr, i)->valuestring);
        cJSON_Delete(root);
        printf("Loaded %d locations.\n", num_locations);
    }

    /* ── build slot list (Cartesian product: days × timeslots × rooms) ── */
    num_slots = num_days * num_timeslots * num_locations;
    slot_list = malloc((size_t)num_slots * sizeof(Slot));
    if (!slot_list) { perror("malloc"); return 1; }
    {
        int idx = 0;
        for (int d = 0; d < num_days; d++)
            for (int t = 0; t < num_timeslots; t++)
                for (int l = 0; l < num_locations; l++) {
                    strncpy(slot_list[idx].day,      days[d],      MAX_DAY_LEN  - 1);
                    strncpy(slot_list[idx].time,     timeslots[t], MAX_TIME_LEN - 1);
                    strncpy(slot_list[idx].location, locations[l], MAX_LOC_LEN  - 1);
                    slot_list[idx].day     [MAX_DAY_LEN  - 1] = '\0';
                    slot_list[idx].time    [MAX_TIME_LEN - 1] = '\0';
                    slot_list[idx].location[MAX_LOC_LEN  - 1] = '\0';
                    idx++;
                }
    }

    /* ── load students and map course IDs → exam indices ─────────────── */
    {
        char  *raw  = read_file(students_path);
        cJSON *root = cJSON_Parse(raw);
        free(raw);
        if (!root) { fprintf(stderr, "Bad students JSON\n"); return 1; }

        cJSON *arr   = cJSON_GetObjectItem(root, "students");
        num_students = cJSON_GetArraySize(arr);
        students     = malloc((size_t)num_students * sizeof(Student));
        if (!students) { perror("malloc"); return 1; }

        for (int i = 0; i < num_students; i++) {
            cJSON *s = cJSON_GetArrayItem(arr, i);

            strncpy(students[i].studentID,
                    cJSON_GetObjectItem(s, "studentID")->valuestring,
                    MAX_ID_LEN - 1);
            students[i].studentID[MAX_ID_LEN - 1] = '\0';

            cJSON *courses    = cJSON_GetObjectItem(s, "enrolledCourses");
            int    nc         = cJSON_GetArraySize(courses);
            students[i].num_courses = nc;
            students[i].course_idx  = malloc((size_t)nc * sizeof(int));
            if (!students[i].course_idx) { perror("malloc"); return 1; }

            for (int j = 0; j < nc; j++) {
                const char *cid = cJSON_GetArrayItem(courses, j)->valuestring;
                students[i].course_idx[j] = -1;   /* -1 = not found */
                for (int k = 0; k < num_exams; k++) {
                    if (strcmp(exams[k], cid) == 0) {
                        students[i].course_idx[j] = k;
                        break;
                    }
                }
            }
        }
        cJSON_Delete(root);
        printf("Loaded %d students.\n\n", num_students);
    }

    /* ── print run configuration ──────────────────────────────────────── */
    printf("=== Genetic Algorithm Configuration ===\n");
    printf("  Exams          : %d\n", num_exams);
    printf("  Total slots    : %d  (%d days x %d times x %d rooms)\n",
           num_slots, num_days, num_timeslots, num_locations);
    printf("  Students       : %d\n", num_students);
    printf("  Population     : %d\n", POPULATION_SIZE);
    printf("  Generations    : %d\n", NUM_GENERATIONS);
    printf("  Mutation rate  : %.2f per gene\n", MUTATION_RATE);
    printf("  Elitism count  : %d\n", ELITISM_COUNT);
    printf("  Tournament size: %d\n", TOURNAMENT_SIZE);
    printf("  Perfect fitness: %d\n\n", BASE_FITNESS);

    /* ── allocate two populations (ping-pong buffers) ─────────────────── */
    Chromosome **pop  = malloc((size_t)POPULATION_SIZE * sizeof(Chromosome *));
    Chromosome **next = malloc((size_t)POPULATION_SIZE * sizeof(Chromosome *));
    if (!pop || !next) { perror("malloc"); return 1; }

    for (int i = 0; i < POPULATION_SIZE; i++) {
        pop [i] = alloc_chromosome();
        next[i] = alloc_chromosome();
        randomize(pop[i]);
    }

    /* Separate buffer to safely store the all-time best across swaps */
    Chromosome *best_ever = alloc_chromosome();
    int         best_fitness = INT_MIN;

    /* ── evolutionary loop ────────────────────────────────────────────── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    for (int gen = 0; gen < NUM_GENERATIONS; gen++) {

        /* Sort population: highest fitness first */
        qsort(pop, (size_t)POPULATION_SIZE, sizeof(Chromosome *), cmp_chrom_desc);

        /* Update all-time best (copy so the pointer stays valid after swap) */
        if (pop[0]->fitness > best_fitness) {
            best_fitness = pop[0]->fitness;
            copy_chromosome(best_ever, pop[0]);
        }

        printf("Gen %4d | Best this gen: %6d | All-time best: %6d\n",
               gen, pop[0]->fitness, best_fitness);

        /* Early exit: perfect schedule found — no conflicts at all */
        if (best_fitness >= BASE_FITNESS) {
            printf("\nPerfect conflict-free schedule found at generation %d!\n\n", gen);
            break;
        }

        /* ── elitism: copy the top ELITISM_COUNT chromosomes unchanged ── */
        for (int i = 0; i < ELITISM_COUNT && i < POPULATION_SIZE; i++)
            copy_chromosome(next[i], pop[i]);

        /* ── fill remaining slots: tournament → crossover → mutate ─────── */
        for (int i = ELITISM_COUNT; i < POPULATION_SIZE; i++) {
            const Chromosome *p1 = tournament(pop, POPULATION_SIZE);
            const Chromosome *p2 = tournament(pop, POPULATION_SIZE);
            crossover(p1, p2, next[i]);
            mutate(next[i]);
            next[i]->fitness = compute_fitness(next[i]->genes);
        }

        /* Swap active / staging buffers for the next generation */
        Chromosome **tmp = pop;
        pop  = next;
        next = tmp;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec  - t_start.tv_sec)
                   + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    /* ── results summary ──────────────────────────────────────────────── */
    printf("\n=== Results ===\n");
    printf("  Total time       : %.2f seconds\n", elapsed);
    printf("  Best fitness     : %d  (perfect = %d)\n", best_fitness, BASE_FITNESS);

    int student_conflicts = (BASE_FITNESS - best_fitness > 0)
                          ? (BASE_FITNESS - best_fitness) / STUDENT_CONFLICT_PENALTY
                          : 0;
    printf("  Approx. unresolved student conflicts : %d\n\n", student_conflicts);

    /* ── save ─────────────────────────────────────────────────────────── */
    save_best(save_path, best_ever);

    /* ── cleanup ──────────────────────────────────────────────────────── */
    free_chromosome(best_ever);

    for (int i = 0; i < POPULATION_SIZE; i++) {
        free_chromosome(pop [i]);
        free_chromosome(next[i]);
    }
    free(pop);
    free(next);

    for (int i = 0; i < num_students; i++)
        free(students[i].course_idx);
    free(students);

    free(slot_list);

    for (int i = 0; i < num_exams;     i++) free(exams[i]);
    free(exams);
    for (int i = 0; i < num_locations; i++) free(locations[i]);
    free(locations);

    return 0;
}