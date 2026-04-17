/*
 Graph Coloring Exam Scheduler (C)
 - Uses DSatur graph coloring algorithm to assign conflict-free time slots
 - Hard constraint:  no two exams share the same location at the same time
                     (each time slot holds at most num_locations exams)
 - Soft constraint:  minimise student conflicts (two exams at the same time
                     sharing at least one student)
 - If the hard constraint cannot be satisfied (total slots * locations < exams),
   the program reports this and does NOT write to the output file.
 - If the soft constraint cannot be fully satisfied the program writes the
   best schedule found and prints a warning with the conflict count.
 - Reads same JSON inputs as before:
     1) students file
     2) exams file
     3) locations file
 - Output: JSON  courseID -> {day, time, location}
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ================================================================
   Utility: file reading
   ================================================================ */
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

static void trim_newline(char* s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[n-1] = '\0'; n--; }
}

/* ================================================================
   Dynamic arrays
   ================================================================ */
typedef struct { char** data; int size, capacity; } StringArray;
static void sa_init(StringArray* a) { a->data = NULL; a->size = a->capacity = 0; }
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
    free(a->data); a->data = NULL; a->size = a->capacity = 0;
}

typedef struct { int* data; int size, capacity; } IntArray;
static void ia_init(IntArray* a) { a->data = NULL; a->size = a->capacity = 0; }
static void ia_push(IntArray* a, int v) {
    if (a->size == a->capacity) {
        int nc = a->capacity == 0 ? 8 : a->capacity * 2;
        int* nd = (int*)realloc(a->data, (size_t)nc * sizeof(int));
        if (!nd) exit(1);
        a->data = nd; a->capacity = nc;
    }
    a->data[a->size++] = v;
}
static void ia_free(IntArray* a) { free(a->data); a->data = NULL; a->size = a->capacity = 0; }
static int ia_contains(const IntArray* a, int v) {
    for (int i = 0; i < a->size; i++) if (a->data[i] == v) return 1;
    return 0;
}

/* ================================================================
   JSON helpers  (unchanged from original)
   ================================================================ */
static const char* find_after_key_colon(const char* start, const char* key) {
    size_t buf_len = strlen(key) + 3;
    char* buf = (char*)malloc(buf_len);
    if (!buf) exit(1);
    snprintf(buf, buf_len, "\"%s\"", key);
    const char* quoted = strstr(start, buf);
    free(buf);
    if (!quoted) return NULL;
    const char* p = quoted + strlen(key) + 2;
    while (*p && *p != ':') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static void extract_all_string_values_in_array(const char* arr_start, StringArray* out) {
    const char* p = arr_start;
    if (*p != '[') return;
    int depth = 0;
    while (*p) {
        if (*p == '[') { depth++; p++; continue; }
        if (*p == ']') { depth--; p++; if (depth == 0) break; continue; }
        if (*p == '"') {
            p++;
            const char* s = p;
            while (*p && *p != '"') p++;
            size_t len = (size_t)(p - s);
            char* val = (char*)malloc(len + 1);
            if (!val) exit(1);
            memcpy(val, s, len); val[len] = '\0';
            sa_push(out, val);
            free(val);
            if (*p == '"') p++;
            continue;
        }
        p++;
    }
}

static void parse_exams_all_courses(const char* json, StringArray* out) {
    sa_init(out);
    const char* p = json;
    while ((p = strstr(p, "\"courseID\"")) != NULL) {
        p += strlen("\"courseID\"");
        while (*p && *p != ':') p++;
        if (*p != ':') break; p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '"') continue; p++;
        const char* s = p;
        while (*p && *p != '"') p++;
        size_t len = (size_t)(p - s);
        char* id = (char*)malloc(len + 1);
        if (!id) exit(1);
        memcpy(id, s, len); id[len] = '\0';
        sa_push(out, id); free(id);
        if (*p == '"') p++;
    }
}

static void parse_locations_array(const char* json, StringArray* out) {
    sa_init(out);
    const char* p = find_after_key_colon(json, "locations");
    if (!p) return;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '[') return;
    extract_all_string_values_in_array(p, out);
}

typedef struct { StringArray courses; } Student;

static void parse_students_enrollments(const char* json, Student** out, int* count) {
    *out = NULL; *count = 0;
    int cap = 0;
    const char* p = json;
    while ((p = strstr(p, "\"enrolledCourses\"")) != NULL) {
        p += strlen("\"enrolledCourses\"");
        while (*p && *p != ':') p++;
        if (*p != ':') break; p++;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p != '[') continue;
        const char* arr = p;
        if (*count == cap) {
            int nc = cap == 0 ? 16 : cap * 2;
            Student* ns = (Student*)realloc(*out, (size_t)nc * sizeof(Student));
            if (!ns) exit(1);
            *out = ns;
            for (int i = cap; i < nc; i++) sa_init(&(*out)[i].courses);
            cap = nc;
        }
        Student* st = &(*out)[*count];
        sa_init(&st->courses);
        StringArray vals; sa_init(&vals);
        extract_all_string_values_in_array(arr, &vals);
        for (int i = 0; i < vals.size; i++) sa_push(&st->courses, vals.data[i]);
        sa_free(&vals);
        (*count)++;
        int depth = 0;
        while (*p) {
            if (*p == '[') { depth++; p++; continue; }
            if (*p == ']') { depth--; p++; if (depth == 0) break; continue; }
            p++;
        }
    }
}

static int find_course_index(const StringArray* courses, const char* id) {
    for (int i = 0; i < courses->size; i++)
        if (strcmp(courses->data[i], id) == 0) return i;
    return -1;
}

/* ================================================================
   Course rosters
   ================================================================ */
typedef struct { int studentCount; IntArray students; } CourseRoster;

static CourseRoster* build_course_rosters(const StringArray* courses,
                                           const Student* students, int num_students) {
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

/* ================================================================
   Conflict graph (adjacency list)
   ================================================================ */
typedef struct {
    IntArray* neighbors;
    int size;
} ConflictGraph;

static ConflictGraph build_conflict_graph(int num_courses, const CourseRoster* rosters,
                                           int num_students) {
    ConflictGraph g;
    g.size = num_courses;
    g.neighbors = (IntArray*)malloc((size_t)num_courses * sizeof(IntArray));
    if (!g.neighbors) exit(1);
    for (int i = 0; i < num_courses; i++) ia_init(&g.neighbors[i]);

    IntArray* student_courses = (IntArray*)malloc((size_t)num_students * sizeof(IntArray));
    if (!student_courses) exit(1);
    for (int s = 0; s < num_students; s++) ia_init(&student_courses[s]);

    for (int c = 0; c < num_courses; c++)
        for (int k = 0; k < rosters[c].students.size; k++)
            ia_push(&student_courses[rosters[c].students.data[k]], c);

    for (int s = 0; s < num_students; s++) {
        IntArray* cl = &student_courses[s];
        for (int i = 0; i < cl->size; i++)
            for (int j = i + 1; j < cl->size; j++) {
                int a = cl->data[i], b = cl->data[j];
                if (!ia_contains(&g.neighbors[a], b)) ia_push(&g.neighbors[a], b);
                if (!ia_contains(&g.neighbors[b], a)) ia_push(&g.neighbors[b], a);
            }
    }

    for (int s = 0; s < num_students; s++) ia_free(&student_courses[s]);
    free(student_courses);
    return g;
}

static void graph_free(ConflictGraph* g) {
    for (int i = 0; i < g->size; i++) ia_free(&g->neighbors[i]);
    free(g->neighbors); g->neighbors = NULL;
}

/* ================================================================
   Shared-student count between two courses
   ================================================================ */
static int shared_students(int a, int b, const CourseRoster* rosters) {
    int count = 0;
    const IntArray* sa = &rosters[a].students;
    const IntArray* sb = &rosters[b].students;
    for (int i = 0; i < sa->size; i++)
        if (ia_contains(sb, sa->data[i])) count++;
    return count;
}

/* ================================================================
   Count student conflicts for a given slot assignment array.
   A conflict = a pair (a,b) sharing >=1 student AND same slot.
   ================================================================ */
static int count_student_conflicts(int num_courses, const int* slots,
                                    const ConflictGraph* g, const CourseRoster* rosters) {
    int conflicts = 0;
    for (int i = 0; i < num_courses; i++)
        for (int k = 0; k < g->neighbors[i].size; k++) {
            int j = g->neighbors[i].data[k];
            if (j > i && slots[i] == slots[j])
                conflicts += shared_students(i, j, rosters);
        }
    return conflicts;
}

/* ================================================================
   DSatur coloring — returns raw color array (may use more colors
   than total_slots; we will fix that in the rebalance step).
   ================================================================ */
static int* dsatur_coloring(int num_courses, const ConflictGraph* g) {
    int* colors = (int*)malloc((size_t)num_courses * sizeof(int));
    if (!colors) exit(1);
    for (int i = 0; i < num_courses; i++) colors[i] = -1;

    IntArray* saturation = (IntArray*)malloc((size_t)num_courses * sizeof(IntArray));
    if (!saturation) exit(1);
    for (int i = 0; i < num_courses; i++) ia_init(&saturation[i]);

    int num_colored = 0;
    while (num_colored < num_courses) {
        int best = -1;
        for (int i = 0; i < num_courses; i++) {
            if (colors[i] != -1) continue;
            if (best == -1) { best = i; continue; }
            int sat_i = saturation[i].size,  sat_b = saturation[best].size;
            int deg_i = g->neighbors[i].size, deg_b = g->neighbors[best].size;
            if (sat_i > sat_b) { best = i; continue; }
            if (sat_i == sat_b && deg_i > deg_b) { best = i; continue; }
            if (sat_i == sat_b && deg_i == deg_b && i < best) best = i;
        }

        int color = 0;
        while (ia_contains(&saturation[best], color)) color++;
        colors[best] = color;
        num_colored++;

        for (int k = 0; k < g->neighbors[best].size; k++) {
            int nb = g->neighbors[best].data[k];
            if (colors[nb] == -1 && !ia_contains(&saturation[nb], color))
                ia_push(&saturation[nb], color);
        }
    }

    for (int i = 0; i < num_courses; i++) ia_free(&saturation[i]);
    free(saturation);
    return colors;
}

/* ================================================================
   Rebalance slots so that every slot has at most num_locations
   exams.  Strategy:
     1. Count how many exams are in each slot.
     2. While any slot is over-capacity, move the exam in that slot
        that has the fewest shared students with its slot-mates to
        the slot with the most remaining capacity (ties broken by
        fewest student conflicts introduced).
     3. Repeat until all slots are within capacity or no progress
        is possible (should not happen given feasibility check).
   ================================================================ */
static void rebalance_slots(int num_courses, int* slots, int total_slots,
                             int num_locations, const ConflictGraph* g,
                             const CourseRoster* rosters) {
    /* slot_count[s] = number of exams currently in slot s */
    int* slot_count = (int*)calloc((size_t)total_slots, sizeof(int));
    if (!slot_count) exit(1);
    for (int i = 0; i < num_courses; i++) {
        if (slots[i] < 0 || slots[i] >= total_slots) {
            /* DSatur used a color >= total_slots — remap to slot 0 for now;
               we will fix it in the loop below */
            slots[i] = 0;
        }
        slot_count[slots[i]]++;
    }

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int s = 0; s < total_slots; s++) {
            if (slot_count[s] <= num_locations) continue;

            /* This slot is over-capacity. Find all exams in it. */
            int* in_slot = (int*)malloc((size_t)num_courses * sizeof(int));
            if (!in_slot) exit(1);
            int in_count = 0;
            for (int i = 0; i < num_courses; i++)
                if (slots[i] == s) in_slot[in_count++] = i;

            /* For each exam in the slot, compute how many shared students
               it has with the other exams currently in the same slot.
               Pick the one with the fewest — it's the cheapest to move. */
            int best_exam = -1, best_shared = -1;
            for (int k = 0; k < in_count; k++) {
                int exam = in_slot[k];
                int sh = 0;
                for (int j = 0; j < in_count; j++) {
                    if (j == k) continue;
                    sh += shared_students(exam, in_slot[j], rosters);
                }
                if (best_exam == -1 || sh < best_shared) {
                    best_exam = exam; best_shared = sh;
                }
            }
            free(in_slot);

            /* Find the destination slot with the most free space that
               minimises new student conflicts for best_exam. */
            int dest_slot = -1, dest_free = -1, dest_conflicts = -1;
            for (int t = 0; t < total_slots; t++) {
                if (t == s) continue;
                int free_space = num_locations - slot_count[t];
                if (free_space <= 0) continue;

                /* Count new conflicts introduced */
                int new_conf = 0;
                for (int i = 0; i < num_courses; i++) {
                    if (i == best_exam || slots[i] != t) continue;
                    if (ia_contains(&g->neighbors[best_exam], i))
                        new_conf += shared_students(best_exam, i, rosters);
                }

                /* Prefer fewer conflicts, break ties by more free space */
                if (dest_slot == -1 ||
                    new_conf < dest_conflicts ||
                    (new_conf == dest_conflicts && free_space > dest_free)) {
                    dest_slot = t;
                    dest_free = free_space;
                    dest_conflicts = new_conf;
                }
            }

            if (dest_slot == -1) {
                /* No valid destination found — feasibility was already
                   verified before calling this function, so this should
                   not happen. */
                fprintf(stderr, "Internal error: cannot rebalance slot %d.\n", s);
                free(slot_count);
                return;
            }

            slot_count[s]--;
            slot_count[dest_slot]++;
            slots[best_exam] = dest_slot;
            changed = 1;
            break; /* restart the outer loop after each move */
        }
    }
    free(slot_count);
}

/* ================================================================
   Assign locations within each slot (largest enrollment first).
   ================================================================ */
static int* assign_locations_in_slots(int num_courses, const int* slots,
                                       const CourseRoster* rosters,
                                       int total_slots, int num_locations) {
    int* loc_assign = (int*)malloc((size_t)num_courses * sizeof(int));
    if (!loc_assign) exit(1);
    for (int i = 0; i < num_courses; i++) loc_assign[i] = -1;

    for (int s = 0; s < total_slots; s++) {
        /* Collect exams in this slot */
        int* group = (int*)malloc((size_t)num_courses * sizeof(int));
        if (!group) exit(1);
        int gsize = 0;
        for (int i = 0; i < num_courses; i++)
            if (slots[i] == s) group[gsize++] = i;

        /* Sort descending by enrollment (bubble sort; groups are small) */
        for (int i = 0; i < gsize - 1; i++)
            for (int j = i + 1; j < gsize; j++)
                if (rosters[group[j]].studentCount > rosters[group[i]].studentCount) {
                    int tmp = group[i]; group[i] = group[j]; group[j] = tmp;
                }

        /* Assign locations 0..num_locations-1 in order.
           After rebalancing every slot has <= num_locations exams, so
           this will never exceed the available locations. */
        for (int i = 0; i < gsize; i++)
            loc_assign[group[i]] = i % num_locations; /* safety modulo */

        free(group);
    }
    return loc_assign;
}

/* ================================================================
   Slots and scheduling constants
   ================================================================ */
static const char* DAYS[]  = {"M", "T", "W", "R", "F"};
static const char* TIMES[] = {
    "8:00am-10:00am", "10:30am-12:30pm", "1:00pm-3:00pm", "3:30pm-5:30pm"
};
#define NUM_DAYS  5
#define NUM_TIMES 4

/* ================================================================
   Main
   ================================================================ */
int main(void) {
    char students_file[512], exams_file[512], locations_file[512], save_file[512];
    char students_path[1024], exams_path[1024], locations_path[1024], save_path[1024];

    printf("Input students json file (just filename, will look in ../Data/Students): ");
    if (!fgets(students_file, sizeof(students_file), stdin)) return 1;
    trim_newline(students_file);
    snprintf(students_path, sizeof(students_path), "../Data/Students/%s", students_file);

    printf("Input exam json file (just filename, will look in ../Data/Courses): ");
    if (!fgets(exams_file, sizeof(exams_file), stdin)) return 1;
    trim_newline(exams_file);
    snprintf(exams_path, sizeof(exams_path), "../Data/Courses/%s", exams_file);

    printf("Input file for locations (just filename, will look in ../Data/Locations): ");
    if (!fgets(locations_file, sizeof(locations_file), stdin)) return 1;
    trim_newline(locations_file);
    snprintf(locations_path, sizeof(locations_path), "../Data/Locations/%s", locations_file);

    printf("Input file to save results (just filename, will be saved in ../Data/Schedules): ");
    if (!fgets(save_file, sizeof(save_file), stdin)) return 1;
    trim_newline(save_file);
    snprintf(save_path, sizeof(save_path), "../Data/Schedules/%s", save_file);

    char* students_json  = read_entire_file(students_path);
    char* exams_json     = read_entire_file(exams_path);
    char* locations_json = read_entire_file(locations_path);
    if (!students_json || !exams_json || !locations_json) {
        fprintf(stderr, "Error: failed to read one or more input files.\n");
        free(students_json); free(exams_json); free(locations_json);
        return 1;
    }

    Student* students = NULL; int num_students = 0;
    parse_students_enrollments(students_json, &students, &num_students);

    StringArray courses;   parse_exams_all_courses(exams_json, &courses);
    StringArray locations; parse_locations_array(locations_json, &locations);

    if (courses.size == 0 || locations.size == 0) {
        fprintf(stderr, "Error: missing courses or locations.\n");
        free(students_json); free(exams_json); free(locations_json);
        sa_free(&courses); sa_free(&locations);
        return 1;
    }

    int total_slots    = NUM_DAYS * NUM_TIMES;        /* 20 */
    int num_locations  = locations.size;
    int total_capacity = total_slots * num_locations; /* max exams that can be placed */

    /* ----------------------------------------------------------------
       Hard feasibility check: can we fit all exams?
       ---------------------------------------------------------------- */
    if (courses.size > total_capacity) {
        fprintf(stderr,
            "Error: cannot schedule %d exams into %d slots x %d locations = %d capacity.\n"
            "No output file written.\n",
            courses.size, total_slots, num_locations, total_capacity);
        free(students_json); free(exams_json); free(locations_json);
        sa_free(&courses); sa_free(&locations);
        if (students) {
            for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
            free(students);
        }
        return 1;
    }

    CourseRoster* rosters = build_course_rosters(&courses, students, num_students);

    clock_t start = clock();

    ConflictGraph g = build_conflict_graph(courses.size, rosters, num_students);

    /* Step 1: DSatur gives us an initial coloring that minimises student
       conflicts (each color = a time slot).  It may use more than total_slots
       colors; we handle that in Step 2. */
    int* slots = dsatur_coloring(courses.size, &g);

    /* Step 2: Clamp any color >= total_slots by placing the exam in the
       slot with the most free space (fewest conflicts preferred). */
    {
        int* slot_count = (int*)calloc((size_t)total_slots, sizeof(int));
        if (!slot_count) exit(1);
        for (int i = 0; i < courses.size; i++)
            if (slots[i] >= 0 && slots[i] < total_slots)
                slot_count[slots[i]]++;

        for (int i = 0; i < courses.size; i++) {
            if (slots[i] < total_slots) continue;
            /* Find best slot: most free space, fewest new conflicts */
            int best_slot = -1, best_free = -1, best_conf = -1;
            for (int t = 0; t < total_slots; t++) {
                int free_space = num_locations - slot_count[t];
                if (free_space <= 0) continue;
                int new_conf = 0;
                for (int j = 0; j < courses.size; j++) {
                    if (j == i || slots[j] != t) continue;
                    if (ia_contains(&g.neighbors[i], j))
                        new_conf += shared_students(i, j, rosters);
                }
                if (best_slot == -1 || new_conf < best_conf ||
                    (new_conf == best_conf && free_space > best_free)) {
                    best_slot = t; best_free = free_space; best_conf = new_conf;
                }
            }
            if (best_slot == -1) {
                /* Should not reach here after feasibility check */
                fprintf(stderr, "Internal error: no slot for course %d.\n", i);
                free(slot_count); free(slots);
                return 1;
            }
            slots[i] = best_slot;
            slot_count[best_slot]++;
        }
        free(slot_count);
    }

    /* Step 3: Rebalance — move exams out of over-capacity slots while
       minimising new student conflicts. */
    rebalance_slots(courses.size, slots, total_slots, num_locations, &g, rosters);

    /* Verify hard constraint was met */
    {
        int* slot_count = (int*)calloc((size_t)total_slots, sizeof(int));
        if (!slot_count) exit(1);
        for (int i = 0; i < courses.size; i++) slot_count[slots[i]]++;
        int violated = 0;
        for (int t = 0; t < total_slots; t++)
            if (slot_count[t] > num_locations) violated++;
        free(slot_count);
        if (violated) {
            /* This really should not happen after the feasibility check +
               rebalance, but guard it anyway. */
            fprintf(stderr,
                "Error: could not produce a valid schedule respecting location "
                "capacity after rebalancing.\nNo output file written.\n");
            clock_t end = clock();
            printf("Total time: %.4f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);
            graph_free(&g); free(slots);
            for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
            free(rosters); sa_free(&courses); sa_free(&locations);
            if (students) {
                for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
                free(students);
            }
            free(students_json); free(exams_json); free(locations_json);
            return 1;
        }
    }

    /* Step 4: Assign a specific location within each slot */
    int* loc_assign = assign_locations_in_slots(courses.size, slots, rosters,
                                                 total_slots, num_locations);

    clock_t end = clock();

    /* Count student conflicts in final schedule */
    int student_conflicts = count_student_conflicts(courses.size, slots, &g, rosters);

    if (student_conflicts > 0) {
        printf("Warning: a conflict-free schedule could not be found.\n");
        printf("         %d student-conflict(s) remain in the best schedule found.\n",
               student_conflicts);
        printf("         Schedule written to file anyway.\n");
    } else {
        printf("Schedule is conflict-free.\n");
    }

    /* Count distinct time slots used */
    int* used = (int*)calloc((size_t)total_slots, sizeof(int));
    if (!used) exit(1);
    for (int i = 0; i < courses.size; i++) used[slots[i]] = 1;
    int slots_used = 0;
    for (int t = 0; t < total_slots; t++) if (used[t]) slots_used++;
    free(used);

    printf("Time slots used: %d / %d\n", slots_used, total_slots);
    printf("Total time: %.4f seconds\n", (double)(end - start) / CLOCKS_PER_SEC);

    /* Write output JSON */
    FILE* out = fopen(save_path, "wb");
    if (!out) { fprintf(stderr, "Error: cannot open output file.\n"); return 1; }

    fprintf(out, "{\n");
    for (int i = 0; i < courses.size; i++) {
        int slot     = slots[i];
        int day_idx  = slot / NUM_TIMES;
        int time_idx = slot % NUM_TIMES;
        const char* loc = locations.data[loc_assign[i]];
        fprintf(out, "  \"%s\": {\"day\": \"%s\", \"time\": \"%s\", \"location\": \"%s\"}",
                courses.data[i], DAYS[day_idx], TIMES[time_idx], loc);
        if (i + 1 < courses.size) fprintf(out, ",");
        fprintf(out, "\n");
    }
    fprintf(out, "}\n");
    fclose(out);

    /* Cleanup */
    free(students_json); free(exams_json); free(locations_json);
    graph_free(&g);
    free(slots);
    free(loc_assign);
    for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
    free(rosters);
    sa_free(&courses);
    sa_free(&locations);
    if (students) {
        for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
        free(students);
    }
    return 0;
}