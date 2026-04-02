import json
import time


def build_student_course_map(students_data):
    student_courses = {}
    for student in students_data.get("students", []):
        sid = student.get("studentID")
        enrolled = student.get("enrolledCourses", [])
        if sid is not None:
            student_courses[sid] = enrolled
    return student_courses


def extract_exams(exams_data):
    exams = []
    for dept in exams_data.get("departments", []):
        for course in dept.get("courses", []):
            cid = course.get("courseID")
            if cid is not None:
                exams.append(cid)
    return exams


def build_course_students(exams, student_courses):
    # course -> list of students enrolled in that course
    course_students = {exam: [] for exam in exams}
    for student_id, courses in student_courses.items():
        for course in courses:
            if course in course_students:
                course_students[course].append(student_id)
    return course_students


def generate_slots(days, timeslots, locations):
    slots = []
    for day in days:
        for time_slot in timeslots:
            for location in locations:
                slots.append({"day": day, "time": time_slot, "location": location})
    return slots


def greedy_schedule(exams, course_students, slots):
    # Order exams by largest enrollment first (tie-break by course id for stable results)
    ordered_exams = sorted(
        exams,
        key=lambda exam: (-len(course_students.get(exam, [])), exam)
    )

    schedule = {}
    used_slot_indices = set()
    # student -> set of (day, time) already assigned
    student_used_times = {}

    for exam in ordered_exams:
        enrolled_students = course_students.get(exam, [])
        placed = False

        for idx, slot in enumerate(slots):
            if idx in used_slot_indices:
                continue

            key = (slot["day"], slot["time"])
            has_conflict = any(
                key in student_used_times.get(student_id, set())
                for student_id in enrolled_students
            )
            if has_conflict:
                continue

            schedule[exam] = slot
            used_slot_indices.add(idx)
            for student_id in enrolled_students:
                if student_id not in student_used_times:
                    student_used_times[student_id] = set()
                student_used_times[student_id].add(key)
            placed = True
            break

        if not placed:
            schedule[exam] = None

    return schedule


def main():
    students_file = input("Input students json file: ")
    exam_file = input("Input exam json file: ")
    space_file = input("Input file for locations: ")
    save_file = input("Input file to save results: ")

    days = ["M", "T", "W", "R", "F"]
    timeslots = [
        "8:00am-10:00am",
        "10:30am-12:30pm",
        "1:00pm-3:00pm",
        "3:30pm-5:30pm",
    ]

    with open(students_file, "r") as stud_file:
        students_data = json.load(stud_file)
    with open(exam_file, "r") as exam_file_obj:
        exams_data = json.load(exam_file_obj)
    with open(space_file, "r") as space_file_obj:
        locations_data = json.load(space_file_obj)

    locations = locations_data.get("locations", [])
    exams = extract_exams(exams_data)
    student_courses = build_student_course_map(students_data)
    course_students = build_course_students(exams, student_courses)
    slots = generate_slots(days, timeslots, locations)

    generation_start = time.time()
    schedule = greedy_schedule(exams, course_students, slots)
    generation_end = time.time()
    # Greedy version has no separate conflict-removal phase.
    conflict_removal_end = generation_end

    with open(save_file, "w") as save_file_obj:
        json.dump(schedule, save_file_obj, indent=4)

    print(f"Generation Time: {generation_end - generation_start:.2f} seconds\n")
    print(f"Removal Time: {conflict_removal_end - generation_end:.2f} seconds\n")
    print(f"Total Time: {conflict_removal_end - generation_start:.2f} seconds\n")
    print("Total schedules generated: 1\n")


if __name__ == "__main__":
    main()

