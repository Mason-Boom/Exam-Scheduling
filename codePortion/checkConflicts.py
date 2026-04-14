import json
import sys
from collections import defaultdict

def load_json(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

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
    if total_conflicts == 0:
        print("No conflicts found!")
    else:
        print(f"Total conflicts found: {total_conflicts}")

if __name__ == "__main__":
    import os
    folder = os.path.dirname(os.path.abspath(__file__))
    schedule_file = input("Enter schedule JSON filename: ").strip()
    students_file = input("Enter students JSON filename: ").strip()
    schedule_path = os.path.join(folder, schedule_file)
    students_path = os.path.join(folder, students_file)
    main(schedule_path, students_path)
