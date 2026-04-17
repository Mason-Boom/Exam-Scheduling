/*
 Greedy Exam Scheduler (C)
 - Reads the same kinds of JSON inputs as BF.py:
   1) students file: {"students":[{"studentID":"...","enrolledCourses":["C1","C2", ...]}, ...]}
   2) exams file: {"departments":[{"dept":"...","courses":[{"courseID":"C1"}, ...]}, ...]}
   3) locations file: {"locations":["Room A","Room B", ...]}
 - Greedy strategy:
   * Generate all (day,time,location) slots
   * Order exams by number of enrolled students (descending)
   * For each exam, pick the slot that introduces the FEWEST new student conflicts.
     - A "conflict" is when a student already has an exam in the same (day,time) slot.
     - If a zero-conflict slot exists, it is always preferred.
     - If no zero-conflict slot exists, the slot with the minimum conflict count is chosen.
     - This guarantees every exam is always placed in a slot.
 - Output:
   * Writes one schedule (mapping courseID -> {day,time,location}) as a JSON object to the save file.
   * Reports to the console whether the schedule is conflict-free or prints the minimum number
     of conflicts that could not be avoided.

 No external libraries required; JSON is parsed minimally for the expected shapes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

/* ---------- Utility: file reading ---------- */
static char* read_entire_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* ---------- Minimal dynamic arrays ---------- */
typedef struct {
    char** data;
    int size;
    int capacity;
} StringArray;

static void sa_init(StringArray* a) {
    a->data = NULL; a->size = 0; a->capacity = 0;
}
static void sa_push(StringArray* a, const char* s) {
    if (a->size == a->capacity) {
        int nc = a->capacity == 0 ? 8 : a->capacity * 2;
        char** nd = (char**)realloc(a->data, (size_t)nc * sizeof(char*));
        if (!nd) exit(1);
        a->data = nd; a->capacity = nc;
    }
    char* cpy = (char*)malloc(strlen(s) + 1);
    if (!cpy) exit(1);
    strcpy(cpy, s);
    a->data[a->size++] = cpy;
}
static void sa_free(StringArray* a) {
    for (int i = 0; i < a->size; i++) free(a->data[i]);
    free(a->data);
    a->data = NULL; a->size = 0; a->capacity = 0;
}

typedef struct {
    int* data;
    int size;
    int capacity;
} IntArray;

static void ia_init(IntArray* a) {
    a->data = NULL; a->size = 0; a->capacity = 0;
}
static void ia_push(IntArray* a, int v) {
    if (a->size == a->capacity) {
        int nc = a->capacity == 0 ? 8 : a->capacity * 2;
        int* nd = (int*)realloc(a->data, (size_t)nc * sizeof(int));
        if (!nd) exit(1);
        a->data = nd; a->capacity = nc;
    }
    a->data[a->size++] = v;
}
static void ia_free(IntArray* a) {
    free(a->data);
    a->data = NULL; a->size = 0; a->capacity = 0;
}

/* ---------- Trim helpers ---------- */
static void trim_newline(char* s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) { s[n - 1] = '\0'; n--; }
}

/* ---------- Minimal JSON scanning helpers ---------- */
static const char* find_after_key_colon(const char* start, const char* key_unquoted) {
    size_t buf_len = strlen(key_unquoted) + 3;
    char* buf = (char*)malloc(buf_len);
    if (!buf) exit(1);
    snprintf(buf, buf_len, "\"%s\"", key_unquoted);
    const char* quoted_key = strstr(start, buf);
    free(buf);
    if (!quoted_key) return NULL;
    const char* p = quoted_key + strlen(key_unquoted) + 2;
    while (*p && *p != ':') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static void extract_all_string_values_in_array(const char* arr_start, StringArray* out_values) {
    const char* p = arr_start;
    if (*p != '[') return;
    int depth = 0;
    while (*p) {
        if (*p == '[') { depth++; p++; continue; }
        if (*p == ']') {
            depth--; p++;
            if (depth == 0) break;
            continue;
        }
        if (*p == '\"') {
            p++;
            const char* s = p;
            while (*p && *p != '\"') p++;
            size_t len = (size_t)(p - s);
            char* val = (char*)malloc(len + 1);
            if (!val) exit(1);
            memcpy(val, s, len);
            val[len] = '\0';
            sa_push(out_values, val);
            free(val);
            if (*p == '\"') p++;
            continue;
        }
        p++;
    }
}

/* ---------- Domain structures ---------- */
typedef struct {
    StringArray courses;
} Student;

typedef struct {
    int studentCount;
    IntArray students;
} CourseRoster;

typedef struct {
    const char* day;
    const char* time;
    char* location;
} Slot;

typedef struct {
    int courseIndex;
    int numStudents;
} CourseOrder;

static int cmp_course_order_desc(const void* a, const void* b) {
    const CourseOrder* ca = (const CourseOrder*)a;
    const CourseOrder* cb = (const CourseOrder*)b;
    if (cb->numStudents != ca->numStudents) return cb->numStudents - ca->numStudents;
    return ca->courseIndex - cb->courseIndex;
}

/* ---------- Parse exams: collect all "courseID" values ---------- */
static void parse_exams_all_courses(const char* json, StringArray* courses_out) {
    sa_init(courses_out);
    const char* p = json;
    while ((p = strstr(p, "\"courseID\"")) != NULL) {
        p += strlen("\"courseID\"");
        while (*p && *p != ':') p++;
        if (*p != ':') break;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '\"') continue;
        p++;
        const char* s = p;
        while (*p && *p != '\"') p++;
        if (*p != '\"') break;
        size_t len = (size_t)(p - s);
        char* id = (char*)malloc(len + 1);
        if (!id) exit(1);
        memcpy(id, s, len);
        id[len] = '\0';
        sa_push(courses_out, id);
        free(id);
        if (*p == '\"') p++;
    }
}

/* ---------- Parse students ---------- */
static void parse_students_enrollments(const char* json, Student** students_out, int* num_students_out) {
    *students_out = NULL; *num_students_out = 0;
    int cap = 0;
    const char* p = json;
    while ((p = strstr(p, "\"enrolledCourses\"")) != NULL) {
        p += strlen("\"enrolledCourses\"");
        while (*p && *p != ':') p++;
        if (*p != ':') break;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '[') continue;
        const char* arr = p;
        if (*num_students_out == cap) {
            int nc = cap == 0 ? 16 : cap * 2;
            Student* ns = (Student*)realloc(*students_out, (size_t)nc * sizeof(Student));
            if (!ns) exit(1);
            *students_out = ns;
            for (int i = cap; i < nc; i++) {
                (*students_out)[i].courses.data = NULL;
                (*students_out)[i].courses.size = 0;
                (*students_out)[i].courses.capacity = 0;
            }
            cap = nc;
        }
        Student* st = &((*students_out)[*num_students_out]);
        sa_init(&st->courses);
        StringArray vals; sa_init(&vals);
        extract_all_string_values_in_array(arr, &vals);
        for (int i = 0; i < vals.size; i++) sa_push(&st->courses, vals.data[i]);
        sa_free(&vals);
        (*num_students_out)++;
        int depth = 0;
        while (*p) {
            if (*p == '[') { depth++; p++; continue; }
            if (*p == ']') { depth--; p++; if (depth == 0) break; continue; }
            p++;
        }
    }
}

/* ---------- Parse locations ---------- */
static void parse_locations_array(const char* json, StringArray* locations_out) {
    sa_init(locations_out);
    const char* p = find_after_key_colon(json, "locations");
    if (!p) return;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '[') return;
    extract_all_string_values_in_array(p, locations_out);
}

/* ---------- Helpers ---------- */
static int find_course_index(const StringArray* courses, const char* id) {
    for (int i = 0; i < courses->size; i++)
        if (strcmp(courses->data[i], id) == 0) return i;
    return -1;
}

static CourseRoster* build_course_rosters(const StringArray* courses, const Student* students, int num_students) {
    int n = courses->size;
    CourseRoster* rosters = (CourseRoster*)malloc((size_t)n * sizeof(CourseRoster));
    if (!rosters) exit(1);
    for (int i = 0; i < n; i++) { rosters[i].studentCount = 0; ia_init(&rosters[i].students); }
    for (int s = 0; s < num_students; s++) {
        for (int c = 0; c < students[s].courses.size; c++) {
            int idx = find_course_index(courses, students[s].courses.data[c]);
            if (idx >= 0) { rosters[idx].studentCount++; ia_push(&rosters[idx].students, s); }
        }
    }
    return rosters;
}

/* ---------- Slots ---------- */
static const char* DAYS[] = {"M", "T", "W", "R", "F"};
static const char* TIMES[] = {
    "8:00am-10:00am", "10:30am-12:30pm", "1:00pm-3:00pm", "3:30pm-5:30pm"
};
#define NUM_DAYS  5
#define NUM_TIMES 4

static Slot* build_all_slots(const StringArray* locations, int* num_slots_out) {
    int nl = locations->size;
    int total = NUM_DAYS * NUM_TIMES * nl;
    *num_slots_out = total;
    Slot* slots = (Slot*)malloc((size_t)total * sizeof(Slot));
    if (!slots) exit(1);
    int k = 0;
    for (int d = 0; d < NUM_DAYS; d++)
        for (int t = 0; t < NUM_TIMES; t++)
            for (int l = 0; l < nl; l++) {
                slots[k].day  = DAYS[d];
                slots[k].time = TIMES[t];
                slots[k].location = (char*)malloc(strlen(locations->data[l]) + 1);
                if (!slots[k].location) exit(1);
                strcpy(slots[k].location, locations->data[l]);
                k++;
            }
    return slots;
}

static int day_index(const char* d) {
    for (int i = 0; i < NUM_DAYS; i++) if (strcmp(DAYS[i], d) == 0) return i;
    return -1;
}
static int time_index(const char* t) {
    for (int i = 0; i < NUM_TIMES; i++) if (strcmp(TIMES[i], t) == 0) return i;
    return -1;
}

/* ---------- Student time-usage tracking ---------- */
/*
 * For each student we track which (day,time) pairs are occupied.
 * We store counts: usage_count[student][day*NUM_TIMES + time] = number of exams assigned there.
 * A value >= 1 means occupied; if we assign another exam to the same slot, that's a conflict.
 */
typedef struct {
    int* count; /* size: NUM_DAYS * NUM_TIMES */
} StudentUsage;

static StudentUsage* alloc_student_usage(int num_students) {
    StudentUsage* u = (StudentUsage*)malloc((size_t)num_students * sizeof(StudentUsage));
    if (!u) exit(1);
    for (int s = 0; s < num_students; s++) {
        u[s].count = (int*)calloc((size_t)(NUM_DAYS * NUM_TIMES), sizeof(int));
        if (!u[s].count) exit(1);
    }
    return u;
}

/*
 * Count how many NEW conflicts placing course `ci` in slot `si` would introduce.
 * A new conflict = a student in this course who already has an exam at the same day+time.
 */
static int count_new_conflicts(int ci, int si,
                               const CourseRoster* rosters,
                               const Slot* slots,
                               const StudentUsage* s_usage) {
    int di = day_index(slots[si].day);
    int ti = time_index(slots[si].time);
    if (di < 0 || ti < 0) return INT_MAX;
    int dti = di * NUM_TIMES + ti;
    int conflicts = 0;
    for (int k = 0; k < rosters[ci].students.size; k++) {
        int stud = rosters[ci].students.data[k];
        if (s_usage[stud].count[dti] > 0) conflicts++;
    }
    return conflicts;
}

/* Record the placement of course ci into slot si in the usage table. */
static void record_placement(int ci, int si,
                              const CourseRoster* rosters,
                              const Slot* slots,
                              StudentUsage* s_usage) {
    int di = day_index(slots[si].day);
    int ti = time_index(slots[si].time);
    if (di < 0 || ti < 0) return;
    int dti = di * NUM_TIMES + ti;
    for (int k = 0; k < rosters[ci].students.size; k++) {
        int stud = rosters[ci].students.data[k];
        s_usage[stud].count[dti]++;
    }
}

/* ---------- Main ---------- */
int main(void) {
    char students_file[512], exams_file[512], locations_file[512], save_file[512];
    char students_path[1024], exams_path[1024], locations_path[1024], save_path[1024];
    clock_t generation_start, generation_end;

    printf("Input students json file (just filename, will look in Data/Students): ");
    if (!fgets(students_file, sizeof(students_file), stdin)) return 1;
    trim_newline(students_file);
    snprintf(students_path, sizeof(students_path), "./codePortion/Data/Students/%s", students_file);

    printf("Input exam json file (just filename, will look in Data/Courses): ");
    if (!fgets(exams_file, sizeof(exams_file), stdin)) return 1;
    trim_newline(exams_file);
    snprintf(exams_path, sizeof(exams_path), "./codePortion/Data/Courses/%s", exams_file);

    printf("Input file for locations (just filename, will look in Data/Locations): ");
    if (!fgets(locations_file, sizeof(locations_file), stdin)) return 1;
    trim_newline(locations_file);
    snprintf(locations_path, sizeof(locations_path), "./codePortion/Data/Locations/%s", locations_file);

    printf("Input file to save results (just filename, will be saved in Data/Schedules): ");
    if (!fgets(save_file, sizeof(save_file), stdin)) return 1;
    trim_newline(save_file);
    snprintf(save_path, sizeof(save_path), "./codePortion/Data/Schedules/%s", save_file);

    char* students_json  = read_entire_file(students_path);
    char* exams_json     = read_entire_file(exams_path);
    char* locations_json = read_entire_file(locations_path);
    if (!students_json || !exams_json || !locations_json) {
        fprintf(stderr, "Error: failed to read one or more input files.\n");
        free(students_json); free(exams_json); free(locations_json);
        return 1;
    }

    /* Parse inputs */
    Student* students = NULL; int num_students = 0;
    parse_students_enrollments(students_json, &students, &num_students);

    StringArray courses; parse_exams_all_courses(exams_json, &courses);
    StringArray locations; parse_locations_array(locations_json, &locations);

    if (courses.size == 0) {
        fprintf(stderr, "Error: no courses found in exams file.\n");
        goto cleanup_early;
    }
    if (locations.size == 0) {
        fprintf(stderr, "Error: no locations found in locations file.\n");
        goto cleanup_early;
    }

    {
        /* Build rosters and slots */
        CourseRoster* rosters = build_course_rosters(&courses, students, num_students);
        int num_slots = 0;
        Slot* slots = build_all_slots(&locations, &num_slots);

        if (num_slots < courses.size) {
            fprintf(stderr, "Error: not enough slots (%d) for %d courses.\n", num_slots, courses.size);
            for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
            free(rosters);
            for (int i = 0; i < num_slots; i++) free(slots[i].location);
            free(slots);
            goto cleanup_early;
        }

        /* Sort courses by enrollment descending */
        CourseOrder* order = (CourseOrder*)malloc((size_t)courses.size * sizeof(CourseOrder));
        if (!order) exit(1);
        for (int i = 0; i < courses.size; i++) {
            order[i].courseIndex = i;
            order[i].numStudents = rosters[i].studentCount;
        }
        qsort(order, (size_t)courses.size, sizeof(CourseOrder), cmp_course_order_desc);

        /* Assignment tracking */
        int* course_to_slot = (int*)malloc((size_t)courses.size * sizeof(int));
        if (!course_to_slot) exit(1);
        for (int i = 0; i < courses.size; i++) course_to_slot[i] = -1;

        char* slot_used = (char*)calloc((size_t)num_slots, 1);
        if (!slot_used) exit(1);

        StudentUsage* s_usage = alloc_student_usage(num_students);

        /* ----------------------------------------------------------------
         * GREEDY PLACEMENT — best-fit with guaranteed assignment
         *
         * For each exam (in descending enrollment order):
         *   1. Scan all unused slots.
         *   2. Track the slot with the minimum conflict count.
         *   3. If a zero-conflict slot is found, stop scanning early and use it.
         *   4. Otherwise use whichever unused slot had the fewest conflicts.
         *
         * Because we always pick *some* unused slot, every exam is placed.
         * ---------------------------------------------------------------- */
        generation_start = clock();
        int total_conflicts = 0; /* accumulated new conflicts introduced */

        for (int oi = 0; oi < courses.size; oi++) {
            int ci = order[oi].courseIndex;
            int best_slot      = -1;
            int best_conflicts = INT_MAX;

            for (int si = 0; si < num_slots; si++) {
                if (slot_used[si]) continue;
                int c = count_new_conflicts(ci, si, rosters, slots, s_usage);
                if (c < best_conflicts) {
                    best_conflicts = c;
                    best_slot      = si;
                    if (c == 0) break; /* can't do better; stop early */
                }
            }

            /* best_slot is always valid here: num_slots >= courses.size and we
               only mark one slot per course, so at least one unused slot always exists. */
            course_to_slot[ci] = best_slot;
            slot_used[best_slot] = 1;
            record_placement(ci, best_slot, rosters, slots, s_usage);
            total_conflicts += best_conflicts;
        }

        generation_end = clock();

        /* ----------------------------------------------------------------
         * Write output
         * ---------------------------------------------------------------- */
        FILE* out = fopen(save_path, "wb");
        if (!out) {
            fprintf(stderr, "Error: cannot open output file: %s\n", save_path);
        } else {
            fprintf(out, "{\n");
            for (int i = 0; i < courses.size; i++) {
                int si = course_to_slot[i];
                fprintf(out, "  \"%s\": ", courses.data[i]);
                if (si >= 0) {
                    Slot* sl = &slots[si];
                    fprintf(out, "{\"day\": \"%s\", \"time\": \"%s\", \"location\": \"%s\"}",
                            sl->day, sl->time, sl->location);
                } else {
                    fprintf(out, "null"); /* should never happen */
                }
                if (i + 1 < courses.size) fprintf(out, ",");
                fprintf(out, "\n");
            }
            fprintf(out, "}\n");
            fclose(out);
        }

        /* ----------------------------------------------------------------
         * Console summary
         * ---------------------------------------------------------------- */
        printf("Greedy schedule written to %s\n", save_path);

        if (total_conflicts == 0) {
            printf("Result: CONFLICT-FREE - every student has at most one exam per time slot.\n");
        } else {
            printf("Result: CONFLICTS PRESENT - a fully conflict-free schedule could not be found.\n");
            printf("        Minimum unavoidable student-slot conflicts introduced: %d\n", total_conflicts);
            printf("        (Each unit = one student placed in a time slot they already occupy.)\n");
        }

        double gen_secs   = (double)(generation_end - generation_start) / CLOCKS_PER_SEC;
        printf("\nGeneration Time: %.4f seconds\n", gen_secs);
        printf("Total Time:      %.4f seconds\n",   gen_secs);
        printf("Total schedules generated: 1\n");

        /* Cleanup inner scope */
        for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
        free(rosters);
        for (int i = 0; i < num_slots; i++) free(slots[i].location);
        free(slots);
        free(order);
        free(course_to_slot);
        free(slot_used);
        for (int s = 0; s < num_students; s++) free(s_usage[s].count);
        free(s_usage);
    }

    free(students_json); free(exams_json); free(locations_json);
    sa_free(&courses); sa_free(&locations);
    if (students) {
        for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
        free(students);
    }
    return 0;

cleanup_early:
    free(students_json); free(exams_json); free(locations_json);
    sa_free(&courses); sa_free(&locations);
    if (students) {
        for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
        free(students);
    }
    return 1;
}