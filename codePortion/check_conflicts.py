import json
import sys
from collections import defaultdict

def main(schedule_path, students_path):
    # Load schedule and students
    with open(schedule_path, 'r', encoding='utf-8') as f:
        schedule = json.load(f)
    with open(students_path, 'r', encoding='utf-8') as f:
        students_data = json.load(f)
    students = students_data['students']

    # Build a mapping: course_id -> (day, time)
    course_to_slot = {}
    for course_id, slot in schedule.items():
        if slot is not None:
            key = (slot['day'], slot['time'])
            course_to_slot[course_id] = key

    # For each student, check for conflicts
    total_conflicts = 0
    for student in students:
        enrolled = student.get('enrolledCourses', [])
        slot_count = defaultdict(list)  # (day, time) -> list of courses
        for course_id in enrolled:
            if course_id in course_to_slot:
                slot = course_to_slot[course_id]
                slot_count[slot].append(course_id)
        # Count conflicts: more than one course in the same slot
        for slot, courses in slot_count.items():
            if len(courses) > 1:
                print(f"Student {student.get('studentID', '?')} has conflict at {slot}: {courses}")
                total_conflicts += len(courses) - 1

    # Check for location conflicts (same location, day, and time)
    location_slot_courses = defaultdict(list)  # (location, day, time) -> list of courses
    for course_id, slot in schedule.items():
        if slot is not None:
            key = (slot['location'], slot['day'], slot['time'])
            location_slot_courses[key].append(course_id)

    location_conflict_found = False
    for loc_slot, courses in location_slot_courses.items():
        if len(courses) > 1:
            location_conflict_found = True
            location, day, time = loc_slot
            print(f"Location conflict at {location} on {day} at {time}: {courses}")

    if total_conflicts == 0:
        print("No student conflicts found!")
    else:
        print(f"Total student conflicts found: {total_conflicts}")
    if not location_conflict_found:
        print("No location conflicts found!")

if __name__ == "__main__":
    import os
    folder = os.path.dirname(os.path.abspath(__file__))
    schedule_file = input("Enter schedule JSON filename (just the filename, will look in Data/Schedules): ").strip()
    students_file = input("Enter **students** JSON filename (just the filename, will look in Data/Students): ").strip()
    schedule_path = os.path.abspath(os.path.join(folder, "./Data/Schedules", schedule_file))
    students_path = os.path.abspath(os.path.join(folder, "./Data/Students", students_file))
    main(schedule_path, students_path)
