/*
 Greedy Exam Scheduler (C)
 - Reads the same kinds of JSON inputs as BF.py:
   1) students file: {"students":[{"studentID":"...","enrolledCourses":["C1","C2", ...]}, ...]}
   2) exams file: {"departments":[{"dept":"...","courses":[{"courseID":"C1"}, ...]}, ...]}
   3) locations file: {"locations":["Room A","Room B", ...]}
 - Greedy strategy:
   * Generate all (day,time,location) slots
   * Order exams by number of enrolled students (descending)
   * For each exam, pick the first unused slot where none of its students already have an exam at the same (day,time)
 - Output:
   * Writes one schedule (mapping courseID -> {day,time,location}) as a JSON object to the save file
 
 No external libraries are required; JSON is parsed minimally for the expected shapes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}
static void sa_push(StringArray* a, const char* s) {
    if (a->size == a->capacity) {
        int nc = a->capacity == 0 ? 8 : a->capacity * 2;
        char** nd = (char**)realloc(a->data, (size_t)nc * sizeof(char*));
        if (!nd) exit(1);
        a->data = nd;
        a->capacity = nc;
    }
    char* cpy = (char*)malloc(strlen(s) + 1);
    if (!cpy) exit(1);
    strcpy(cpy, s);
    a->data[a->size++] = cpy;
}
static void sa_free(StringArray* a) {
    for (int i = 0; i < a->size; i++) free(a->data[i]);
    free(a->data);
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

typedef struct {
    int* data;
    int size;
    int capacity;
} IntArray;

static void ia_init(IntArray* a) {
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}
static void ia_push(IntArray* a, int v) {
    if (a->size == a->capacity) {
        int nc = a->capacity == 0 ? 8 : a->capacity * 2;
        int* nd = (int*)realloc(a->data, (size_t)nc * sizeof(int));
        if (!nd) exit(1);
        a->data = nd;
        a->capacity = nc;
    }
    a->data[a->size++] = v;
}
static void ia_free(IntArray* a) {
    free(a->data);
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
}

/* ---------- Trim helpers ---------- */
static void trim_newline(char* s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

/* ---------- Minimal JSON scanning helpers ---------- */
static const char* find_key(const char* hay, const char* key) {
    /* Finds the first occurrence of "key" (quoted) */
    size_t klen = strlen(key);
    const char* p = hay;
    while ((p = strstr(p, key)) != NULL) {
        /* Expecting pattern like "key" */
        if (p > hay && p[-1] == '\"' && p[klen] == '\"') {
            return p - 1; /* points at first quote */
        }
        p += klen;
    }
    return NULL;
}

static const char* find_after_key_colon(const char* start, const char* key_unquoted) {
    /* Find "key_unquoted" then the following ':' and return pointer after ':' */
    const char* quoted_key = NULL;
    {
        size_t buf_len = strlen(key_unquoted) + 3;
        char* buf = (char*)malloc(buf_len);
        if (!buf) exit(1);
        snprintf(buf, buf_len, "\"%s\"", key_unquoted);
        quoted_key = strstr(start, buf);
        free((void*)buf);
    }
    if (!quoted_key) return NULL;
    const char* p = quoted_key + strlen(key_unquoted) + 2; /* move past "key" */
    while (*p && *p != ':') p++;
    if (*p != ':') return NULL;
    p++; /* after ':' */
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int extract_string_value_after_colon(const char* p, char* out, size_t out_cap) {
    /* Expects p at start of value. If value is "string", copies into out. Returns 1 on success. */
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '\"') {
        if (i + 1 < out_cap) out[i++] = *p;
        p++;
    }
    if (*p != '\"') return 0;
    out[i] = '\0';
    return 1;
}

static void extract_all_string_values_in_array(const char* arr_start, StringArray* out_values) {
    /* arr_start points at '['; extracts all "...." until matching ']' */
    const char* p = arr_start;
    if (*p != '[') return;
    int depth = 0;
    while (*p) {
        if (*p == '[') { depth++; p++; continue; }
        if (*p == ']') {
            depth--;
            p++;
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
    StringArray courses; /* for each student: enrolled courses by ID */
} Student;

typedef struct {
    int studentCount;
    IntArray students; /* indices of students enrolled */
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

/* ---------- Parse students: collect each student's enrolledCourses list ---------- */
static void parse_students_enrollments(const char* json, Student** students_out, int* num_students_out) {
    *students_out = NULL;
    *num_students_out = 0;
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
        StringArray vals;
        sa_init(&vals);
        extract_all_string_values_in_array(arr, &vals);
        for (int i = 0; i < vals.size; i++) {
            sa_push(&st->courses, vals.data[i]);
        }
        sa_free(&vals);
        (*num_students_out)++;
        /* advance p beyond this array to avoid re-detection */
        int depth = 0;
        while (*p) {
            if (*p == '[') { depth++; p++; continue; }
            if (*p == ']') { depth--; p++; if (depth == 0) break; continue; }
            p++;
        }
    }
}

/* ---------- Parse locations: collect strings from "locations" array ---------- */
static void parse_locations_array(const char* json, StringArray* locations_out) {
    sa_init(locations_out);
    const char* p = find_after_key_colon(json, "locations");
    if (!p) return;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '[') return;
    extract_all_string_values_in_array(p, locations_out);
}

/* ---------- Find course index helper ---------- */
static int find_course_index(const StringArray* courses, const char* id) {
    for (int i = 0; i < courses->size; i++) {
        if (strcmp(courses->data[i], id) == 0) return i;
    }
    return -1;
}

/* ---------- Build course rosters from students ---------- */
static CourseRoster* build_course_rosters(const StringArray* courses, const Student* students, int num_students) {
    int n = courses->size;
    CourseRoster* rosters = (CourseRoster*)malloc((size_t)n * sizeof(CourseRoster));
    if (!rosters) exit(1);
    for (int i = 0; i < n; i++) {
        rosters[i].studentCount = 0;
        ia_init(&rosters[i].students);
    }
    for (int s = 0; s < num_students; s++) {
        for (int c = 0; c < students[s].courses.size; c++) {
            int idx = find_course_index(courses, students[s].courses.data[c]);
            if (idx >= 0) {
                rosters[idx].studentCount++;
                ia_push(&rosters[idx].students, s);
            }
        }
    }
    return rosters;
}

/* ---------- Build slots ---------- */
static const char* DAYS[] = {"M", "T", "W", "R", "F"};
static const char* TIMES[] = {
    "8:00am-10:00am",
    "10:30am-12:30pm",
    "1:00pm-3:00pm",
    "3:30pm-5:30pm"
};

static Slot* build_all_slots(const StringArray* locations, int* num_slots_out) {
    int nd = (int)(sizeof(DAYS) / sizeof(DAYS[0]));
    int nt = (int)(sizeof(TIMES) / sizeof(TIMES[0]));
    int nl = locations->size;
    int total = nd * nt * nl;
    *num_slots_out = total;
    Slot* slots = (Slot*)malloc((size_t)total * sizeof(Slot));
    if (!slots) exit(1);
    int k = 0;
    for (int d = 0; d < nd; d++) {
        for (int t = 0; t < nt; t++) {
            for (int l = 0; l < nl; l++) {
                slots[k].day = DAYS[d];
                slots[k].time = TIMES[t];
                /* own the location copy to keep lifetimes simple */
                slots[k].location = (char*)malloc(strlen(locations->data[l]) + 1);
                if (!slots[k].location) exit(1);
                strcpy(slots[k].location, locations->data[l]);
                k++;
            }
        }
    }
    return slots;
}

/* ---------- Student used-time tracking ---------- */
typedef struct {
    /* We mark usage by day-time index (flattened as d*nt + t). Keep a small dynamic set per student. */
    IntArray usedDayTime; 
} StudentUsage;

static int day_index(const char* d) {
    for (int i = 0; i < 5; i++) if (strcmp(DAYS[i], d) == 0) return i;
    return -1;
}
static int time_index(const char* t) {
    for (int i = 0; i < 4; i++) if (strcmp(TIMES[i], t) == 0) return i;
    return -1;
}
static int contains_int(const IntArray* a, int v) {
    for (int i = 0; i < a->size; i++) if (a->data[i] == v) return 1;
    return 0;
}

/* ---------- Greedy assignment ---------- */
int main(void) {
    char students_path[1024];
    char exams_path[1024];
    char locations_path[1024];
    char save_path[1024];
    clock_t generation_start, generation_end, conflict_removal_end;

    printf("Input students json file: ");
    if (!fgets(students_path, sizeof(students_path), stdin)) return 1;
    trim_newline(students_path);
    printf("Input exam json file: ");
    if (!fgets(exams_path, sizeof(exams_path), stdin)) return 1;
    trim_newline(exams_path);
    printf("Input file for locations: ");
    if (!fgets(locations_path, sizeof(locations_path), stdin)) return 1;
    trim_newline(locations_path);
    printf("Input file to save results: ");
    if (!fgets(save_path, sizeof(save_path), stdin)) return 1;
    trim_newline(save_path);

    char* students_json = read_entire_file(students_path);
    char* exams_json = read_entire_file(exams_path);
    char* locations_json = read_entire_file(locations_path);
    if (!students_json || !exams_json || !locations_json) {
        fprintf(stderr, "Error: failed to read one or more input files.\n");
        free(students_json); free(exams_json); free(locations_json);
        return 1;
    }

    /* Parse inputs */
    Student* students = NULL;
    int num_students = 0;
    parse_students_enrollments(students_json, &students, &num_students);

    StringArray courses;
    parse_exams_all_courses(exams_json, &courses);

    StringArray locations;
    parse_locations_array(locations_json, &locations);

    if (courses.size == 0) {
        fprintf(stderr, "Error: no courses found in exams file.\n");
        free(students_json); free(exams_json); free(locations_json);
        sa_free(&courses);
        sa_free(&locations);
        if (students) {
            for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
            free(students);
        }
        return 1;
    }
    if (locations.size == 0) {
        fprintf(stderr, "Error: no locations found in locations file.\n");
        free(students_json); free(exams_json); free(locations_json);
        sa_free(&courses);
        sa_free(&locations);
        if (students) {
            for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
            free(students);
        }
        return 1;
    }

    /* Build rosters and slots */
    CourseRoster* rosters = build_course_rosters(&courses, students, num_students);
    int num_slots = 0;
    Slot* slots = build_all_slots(&locations, &num_slots);

    if (num_slots < courses.size) {
        fprintf(stderr, "Error: not enough slots (%d) for courses (%d).\n", num_slots, courses.size);
        /* cleanup */
        free(students_json); free(exams_json); free(locations_json);
        for (int i = 0; i < courses.size; i++) free(courses.data[i]);
        free(courses.data);
        for (int i = 0; i < locations.size; i++) free(locations.data[i]);
        free(locations.data);
        for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
        free(rosters);
        for (int i = 0; i < num_slots; i++) free(slots[i].location);
        free(slots);
        if (students) {
            for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
            free(students);
        }
        return 1;
    }

    /* Order courses by enrollment size */
    CourseOrder* order = (CourseOrder*)malloc((size_t)courses.size * sizeof(CourseOrder));
    if (!order) exit(1);
    for (int i = 0; i < courses.size; i++) {
        order[i].courseIndex = i;
        order[i].numStudents = rosters[i].studentCount;
    }
    qsort(order, (size_t)courses.size, sizeof(CourseOrder), cmp_course_order_desc);

    /* Track assignment */
    int* course_to_slot = (int*)malloc((size_t)courses.size * sizeof(int));
    if (!course_to_slot) exit(1);
    for (int i = 0; i < courses.size; i++) course_to_slot[i] = -1;
    char* slot_used = (char*)calloc((size_t)num_slots, 1);
    if (!slot_used) exit(1);

    StudentUsage* s_usage = (StudentUsage*)malloc((size_t)num_students * sizeof(StudentUsage));
    if (!s_usage) exit(1);
    for (int s = 0; s < num_students; s++) {
        ia_init(&s_usage[s].usedDayTime);
    }

    /* Greedy place each course */
    generation_start = clock();
    int assigned = 0;
    for (int oi = 0; oi < courses.size; oi++) {
        int ci = order[oi].courseIndex;
        int placed = 0;
        for (int si = 0; si < num_slots && !placed; si++) {
            if (slot_used[si]) continue;
            int di = day_index(slots[si].day);
            int ti = time_index(slots[si].time);
            if (di < 0 || ti < 0) continue;
            int dti = di * 4 + ti;
            /* Check all students in this course for time conflict */
            int ok = 1;
            for (int k = 0; k < rosters[ci].students.size; k++) {
                int stud = rosters[ci].students.data[k];
                if (contains_int(&s_usage[stud].usedDayTime, dti)) {
                    ok = 0; break;
                }
            }
            if (!ok) continue;
            /* Assign */
            course_to_slot[ci] = si;
            slot_used[si] = 1;
            for (int k = 0; k < rosters[ci].students.size; k++) {
                int stud = rosters[ci].students.data[k];
                ia_push(&s_usage[stud].usedDayTime, dti);
            }
            placed = 1;
            assigned++;
        }
        if (!placed) {
            fprintf(stderr, "Failed to place course %s without conflicts.\n", courses.data[ci]);
            /* continue attempting others; we will report partial failure */
        }
    }

    if (assigned != courses.size) {
        fprintf(stderr, "Warning: only assigned %d/%d courses.\n", assigned, courses.size);
    }
    generation_end = clock();
    /* No separate conflict removal phase for greedy; mirror BF.py by setting equal */
    conflict_removal_end = generation_end;

    /* Write single schedule JSON: { "COURSEID": {"day":"D","time":"T","location":"L"}, ... } */
    FILE* out = fopen(save_path, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot open output file: %s\n", save_path);
        /* cleanup */
        free(students_json); free(exams_json); free(locations_json);
        for (int i = 0; i < courses.size; i++) free(courses.data[i]);
        free(courses.data);
        for (int i = 0; i < locations.size; i++) free(locations.data[i]);
        free(locations.data);
        for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
        free(rosters);
        for (int i = 0; i < num_slots; i++) free(slots[i].location);
        free(slots);
        for (int s = 0; s < num_students; s++) ia_free(&s_usage[s].usedDayTime);
        free(s_usage);
        free(slot_used);
        free(course_to_slot);
        if (students) {
            for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
            free(students);
        }
        return 1;
    }

    /* Output as a single schedule object */
    fprintf(out, "{\n");
    for (int i = 0; i < courses.size; i++) {
        fprintf(out, "  \"%s\": ", courses.data[i]);
        if (course_to_slot[i] >= 0) {
            Slot* sl = &slots[course_to_slot[i]];
            fprintf(out, "{\"day\": \"%s\", \"time\": \"%s\", \"location\": \"%s\"}", sl->day, sl->time, sl->location);
        } else {
            fprintf(out, "null");
        }
        if (i + 1 < courses.size) fprintf(out, ",");
        fprintf(out, "\n");
    }
    fprintf(out, "}\n");
    fclose(out);

    printf("Greedy schedule written to %s\n", save_path);
    /* Mirror BF.py timing/summary printout */
    double gen_secs = (double)(generation_end - generation_start) / CLOCKS_PER_SEC;
    double rem_secs = (double)(conflict_removal_end - generation_end) / CLOCKS_PER_SEC;
    double total_secs = (double)(conflict_removal_end - generation_start) / CLOCKS_PER_SEC;
    printf("Generation Time: %.2f seconds\n\n", gen_secs);
    printf("Removal Time: %.2f seconds\n\n", rem_secs);
    printf("Total Time: %.2f seconds\n\n", total_secs);
    printf("Total schedules generated: %d\n\n", 1);

    /* cleanup */
    free(students_json); free(exams_json); free(locations_json);
    for (int i = 0; i < courses.size; i++) free(courses.data[i]);
    free(courses.data);
    for (int i = 0; i < locations.size; i++) free(locations.data[i]);
    free(locations.data);
    for (int i = 0; i < courses.size; i++) ia_free(&rosters[i].students);
    free(rosters);
    for (int i = 0; i < num_slots; i++) free(slots[i].location);
    free(slots);
    for (int s = 0; s < num_students; s++) ia_free(&s_usage[s].usedDayTime);
    free(s_usage);
    free(slot_used);
    free(course_to_slot);
    if (students) {
        for (int i = 0; i < num_students; i++) sa_free(&students[i].courses);
        free(students);
    }
    return 0;
}