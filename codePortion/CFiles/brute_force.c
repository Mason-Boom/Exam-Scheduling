/*
 * exam_scheduler.c
 *
 * C port of the Python exam scheduling backtracker.
 *
 * Dependencies:
 *   cJSON  — single-file JSON library
 *   Get it: https://github.com/DaveGamble/cJSON
 *   Place cJSON.h and cJSON.c alongside this file, then compile with:
 *
 *     gcc -O2 -o exam_scheduler exam_scheduler.c cJSON.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

/* ─── tuneable limits ──────────────────────────────────────────── */
#define MAX_DAY_LEN   8
#define MAX_TIME_LEN  32
#define MAX_LOC_LEN   128
#define MAX_ID_LEN    64

/* ─── data structures ──────────────────────────────────────────── */

typedef struct {
    char day[MAX_DAY_LEN];
    char time[MAX_TIME_LEN];
    char location[MAX_LOC_LEN];
} Slot;

typedef struct {
    char examID[MAX_ID_LEN];
    Slot slot;
} ExamAssignment;

typedef struct {
    ExamAssignment *assignments;   /* array, length = num_exams */
    int             num_exams;
} Schedule;

typedef struct {
    char   studentID[MAX_ID_LEN];
    char **courses;                /* array of course ID strings  */
    int    num_courses;
} Student;

/* ─── globals ──────────────────────────────────────────────────── */

static Schedule *master_schedule_list     = NULL;
static int       master_schedule_count    = 0;
static int       master_schedule_capacity = 0;

static Student  *master_student_list = NULL;
static int       num_students        = 0;

static int count = 0;   /* total schedules generated */

/* ─── dynamic schedule list helpers ───────────────────────────── */

static void push_schedule(ExamAssignment *assignments, int num_exams)
{
    if (master_schedule_count >= master_schedule_capacity) {
        master_schedule_capacity =
            master_schedule_capacity == 0 ? 64 : master_schedule_capacity * 2;
        master_schedule_list = realloc(
            master_schedule_list,
            (size_t)master_schedule_capacity * sizeof(Schedule));
        if (!master_schedule_list) { perror("realloc"); exit(1); }
    }

    Schedule *s    = &master_schedule_list[master_schedule_count++];
    s->num_exams   = num_exams;
    s->assignments = malloc((size_t)num_exams * sizeof(ExamAssignment));
    if (!s->assignments) { perror("malloc"); exit(1); }
    memcpy(s->assignments, assignments,
           (size_t)num_exams * sizeof(ExamAssignment));
}

/* ─── conflict detection ───────────────────────────────────────── */

/*
 * Returns 1 if any student has two exams at the same (day, time),
 * 0 otherwise.  Mirrors hasConflict() from the Python original.
 */
static int has_conflict(const Schedule *schedule)
{
    for (int si = 0; si < num_students; si++) {
        const Student *student = &master_student_list[si];

        for (int a = 0; a < student->num_courses; a++) {
            for (int b = a + 1; b < student->num_courses; b++) {

                const Slot *sa = NULL, *sb = NULL;

                for (int k = 0; k < schedule->num_exams; k++) {
                    if (strcmp(schedule->assignments[k].examID,
                               student->courses[a]) == 0)
                        sa = &schedule->assignments[k].slot;
                    if (strcmp(schedule->assignments[k].examID,
                               student->courses[b]) == 0)
                        sb = &schedule->assignments[k].slot;
                }

                if (sa && sb &&
                    strcmp(sa->day,  sb->day)  == 0 &&
                    strcmp(sa->time, sb->time) == 0)
                    return 1;
            }
        }
    }
    return 0;
}

/* ─── backtracking schedule generator ─────────────────────────── */

static void backtrack(int            index,
                      char         **exam_list,  int num_exams,
                      const Slot    *slot_list,  int num_slots,
                      ExamAssignment *current,
                      int           *used_slots)
{
    if (index == num_exams) {
        push_schedule(current, num_exams);
        count++;
        printf("Added Schedule %d\n", count);
        return;
    }

    for (int i = 0; i < num_slots; i++) {
        if (!used_slots[i]) {
            strncpy(current[index].examID, exam_list[index], MAX_ID_LEN - 1);
            current[index].examID[MAX_ID_LEN - 1] = '\0';
            current[index].slot = slot_list[i];
            used_slots[i] = 1;

            backtrack(index + 1,
                      exam_list, num_exams,
                      slot_list, num_slots,
                      current, used_slots);

            used_slots[i] = 0;
        }
    }
}

static void generate_schedules(char **exam_list, int num_exams,
                                char **days,      int num_days,
                                char **timeslots, int num_timeslots,
                                char **locations, int num_locations)
{
    int num_slots = num_days * num_timeslots * num_locations;
    Slot *slot_list = malloc((size_t)num_slots * sizeof(Slot));
    if (!slot_list) { perror("malloc"); exit(1); }

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

    ExamAssignment *current = malloc((size_t)num_exams * sizeof(ExamAssignment));
    int            *used    = calloc((size_t)num_slots,  sizeof(int));
    if (!current || !used) { perror("malloc"); exit(1); }

    backtrack(0, exam_list, num_exams, slot_list, num_slots, current, used);

    free(slot_list);
    free(current);
    free(used);
}

/* ─── conflict removal ─────────────────────────────────────────── */

static void remove_conflicts(void)
{
    int new_count = 0;
    for (int i = 0; i < master_schedule_count; i++) {
        if (has_conflict(&master_schedule_list[i])) {
            printf("Removed Improper Schedule %d\n", i);
            free(master_schedule_list[i].assignments);
        } else {
            /* Compact valid schedules to the front of the array */
            master_schedule_list[new_count++] = master_schedule_list[i];
        }
    }
    master_schedule_count = new_count;
}

/* ─── JSON I/O helpers ─────────────────────────────────────────── */

/* Read an entire file into a heap-allocated string (caller frees) */
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

static void save_schedules(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot write: %s\n", path); return; }

    fprintf(f, "[\n");

    for (int i = 0; i < master_schedule_count; i++) {
        cJSON    *sched_obj = cJSON_CreateObject();
        Schedule *s         = &master_schedule_list[i];

        for (int j = 0; j < s->num_exams; j++) {
            cJSON *slot_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(slot_obj, "day",      s->assignments[j].slot.day);
            cJSON_AddStringToObject(slot_obj, "time",     s->assignments[j].slot.time);
            cJSON_AddStringToObject(slot_obj, "location", s->assignments[j].slot.location);
            cJSON_AddItemToObject(sched_obj, s->assignments[j].examID, slot_obj);
        }

        // Print one object at a time, never accumulating
        char *json_str = cJSON_PrintUnformatted(sched_obj); // unformatted = smaller output
        fprintf(f, "%s%s\n", json_str, (i < master_schedule_count - 1) ? "," : "");

        free(json_str);
        cJSON_Delete(sched_obj); // free immediately after writing

        if (i % 100000 == 0) printf("Written %d / %d\n", i, master_schedule_count);
    }

    fprintf(f, "]\n");
    fclose(f);
    printf("Wrote %d schedules to %s\n", master_schedule_count, path);
}

/* ─── main ─────────────────────────────────────────────────────── */

int main(void)
{
    char students_file[256], exam_file[256], space_file[256], save_file[256];
    char students_path[512], exam_path[512], space_path[512], save_path[512];

    printf("Input students json file (just filename, will look in ../Data/Students): ");
    if (!fgets(students_file, sizeof(students_file), stdin)) return 1;
    students_file[strcspn(students_file, "\n")] = '\0';
    snprintf(students_path, sizeof(students_path), "../Data/Students/%s", students_file);

    printf("Input exam json file (just filename, will look in ../Data/Courses): ");
    if (!fgets(exam_file, sizeof(exam_file), stdin)) return 1;
    exam_file[strcspn(exam_file, "\n")] = '\0';
    snprintf(exam_path, sizeof(exam_path), "../Data/Courses/%s", exam_file);

    printf("Input file for locations (just filename, will look in ../Data/Locations): ");
    if (!fgets(space_file, sizeof(space_file), stdin)) return 1;
    space_file[strcspn(space_file, "\n")] = '\0';
    snprintf(space_path, sizeof(space_path), "../Data/Locations/%s", space_file);

    printf("Input file to save results (just filename, will be saved in ../Data/Schedules): ");
    if (!fgets(save_file, sizeof(save_file), stdin)) return 1;
    save_file[strcspn(save_file, "\n")] = '\0';
    snprintf(save_path, sizeof(save_path), "../Data/Schedules/%s", save_file);

    /* Hard-coded days / timeslots — identical to the Python version */
    char *days[]      = {"M", "T", "W", "R", "F"};
    int   num_days    = 5;
    char *timeslots[] = {"8:00am-10:00am", "10:30am-12:30pm",
                         "1:00pm-3:00pm",  "3:30pm-5:30pm"};
    int   num_timeslots = 4;

    /* ── load students ─────────────────────────────────────────── */
    {
        char  *raw  = read_file(students_path);
        cJSON *root = cJSON_Parse(raw);
        free(raw);
        if (!root) { fprintf(stderr, "Bad students JSON\n"); return 1; }

        cJSON *arr   = cJSON_GetObjectItem(root, "students");
        num_students = cJSON_GetArraySize(arr);
        master_student_list = malloc((size_t)num_students * sizeof(Student));
        if (!master_student_list) { perror("malloc"); return 1; }

        for (int i = 0; i < num_students; i++) {
            cJSON *s = cJSON_GetArrayItem(arr, i);
            strncpy(master_student_list[i].studentID,
                    cJSON_GetObjectItem(s, "studentID")->valuestring,
                    MAX_ID_LEN - 1);
            master_student_list[i].studentID[MAX_ID_LEN - 1] = '\0';

            cJSON *courses = cJSON_GetObjectItem(s, "enrolledCourses");
            int    nc      = cJSON_GetArraySize(courses);
            master_student_list[i].num_courses = nc;
            master_student_list[i].courses =
                malloc((size_t)nc * sizeof(char *));
            for (int j = 0; j < nc; j++)
                master_student_list[i].courses[j] =
                    strdup(cJSON_GetArrayItem(courses, j)->valuestring);
        }
        cJSON_Delete(root);
    }

    /* ── load exams ────────────────────────────────────────────── */
    int    num_exams = 0, exam_cap = 64;
    char **exams     = malloc((size_t)exam_cap * sizeof(char *));
    if (!exams) { perror("malloc"); return 1; }

    {
        char  *raw  = read_file(exam_path);
        cJSON *root = cJSON_Parse(raw);
        free(raw);
        if (!root) { fprintf(stderr, "Bad exam JSON\n"); return 1; }

        cJSON *depts = cJSON_GetObjectItem(root, "departments");
        cJSON *dept;
        cJSON_ArrayForEach(dept, depts) {
            cJSON *courses = cJSON_GetObjectItem(dept, "courses");
            cJSON *course;
            cJSON_ArrayForEach(course, courses) {
                if (num_exams >= exam_cap) {
                    exam_cap *= 2;
                    exams = realloc(exams, (size_t)exam_cap * sizeof(char *));
                    if (!exams) { perror("realloc"); return 1; }
                }
                exams[num_exams++] =
                    strdup(cJSON_GetObjectItem(course, "courseID")->valuestring);
            }
        }
        cJSON_Delete(root);
    }

    /* ── load locations ────────────────────────────────────────── */
    int    num_locations = 0;
    char **locations     = NULL;

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
    }

    /* ── generate ──────────────────────────────────────────────── */
    count = 0;

    struct timespec gen_start, gen_end, conflict_end;
    clock_gettime(CLOCK_MONOTONIC, &gen_start);

    generate_schedules(exams, num_exams,
                       days,      num_days,
                       timeslots, num_timeslots,
                       locations, num_locations);

    clock_gettime(CLOCK_MONOTONIC, &gen_end);

    /* ── remove conflicts ──────────────────────────────────────── */
    remove_conflicts();
    clock_gettime(CLOCK_MONOTONIC, &conflict_end);

    /* ── save results ──────────────────────────────────────────── */
    save_schedules(save_path);

    /* ── timing report ─────────────────────────────────────────── */
    double gen_time = (gen_end.tv_sec  - gen_start.tv_sec)
                    + (gen_end.tv_nsec - gen_start.tv_nsec) / 1e9;
    double rem_time = (conflict_end.tv_sec  - gen_end.tv_sec)
                    + (conflict_end.tv_nsec - gen_end.tv_nsec) / 1e9;
    double tot_time = (conflict_end.tv_sec  - gen_start.tv_sec)
                    + (conflict_end.tv_nsec - gen_start.tv_nsec) / 1e9;

    printf("\nGeneration Time: %.2f seconds\n", gen_time);
    printf("Removal Time:    %.2f seconds\n",   rem_time);
    printf("Total Time:      %.2f seconds\n",   tot_time);
    printf("Total schedules generated: %d\n",   count);
    printf("Valid schedules remaining: %d\n",   master_schedule_count);

    /* ── cleanup ───────────────────────────────────────────────── */
    for (int i = 0; i < master_schedule_count; i++)
        free(master_schedule_list[i].assignments);
    free(master_schedule_list);

    for (int i = 0; i < num_students; i++) {
        for (int j = 0; j < master_student_list[i].num_courses; j++)
            free(master_student_list[i].courses[j]);
        free(master_student_list[i].courses);
    }
    free(master_student_list);

    for (int i = 0; i < num_exams;    i++) free(exams[i]);
    free(exams);
    for (int i = 0; i < num_locations; i++) free(locations[i]);
    free(locations);

    return 0;
}
