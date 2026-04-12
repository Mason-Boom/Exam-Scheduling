#Steps:
#Generate all permutations of exams and store in queue
#Check each permutation to see if a student is in multiple exams at the same time, if so, remove it from queue


import json
import time

master_schedule_list = []
master_student_list = {} #key, value = student, exam list
improper_schedules = []
count = 0

def generateSchedules(exam_list, days_list, timeslot_list, location_list):
    slot_list = []
    for d in days_list:
        for t in timeslot_list:
            for l in location_list:
                slot_list.append({"day":d, "time":t, "location":l})
    schedule = {}
    used_slots = []

    backtrack(0, exam_list, slot_list, schedule, used_slots)

def backtrack(index, exam_list, slot_list, schedule, used_slots):
    global count
    if index == len(exam_list):
        master_schedule_list.append(schedule.copy())
        count += 1
        print(f"Added Schedule {count}")
        return
    
    e = exam_list[index]

    for slot in slot_list:
        if slot not in used_slots:
            schedule[e] = slot
            used_slots.append(slot)

            backtrack(index+1, exam_list, slot_list, schedule, used_slots)

            del schedule[e]
            used_slots.remove(slot)

def removeConflicts():
    for schedule in master_schedule_list:
        if hasConflict(schedule):
            improper_schedules.append(schedule)
            print(f"Appended Improper Schedule - {schedule}")
    for schedule in improper_schedules:
        master_schedule_list.remove(schedule)
        print(f"Removed Improper Schedule - {schedule}")
                            

def hasConflict(schedule): #TODO: Fix this so that it checks whether a student has two exams at the same time, not the other way around
    for student, exams in master_student_list.items():
        slots = set()
        for e in exams:
            slot = (schedule[e]["day"], schedule[e]["time"])
            if slot in slots:
                return True
            slots.add(slot)
    return False

def main():
    global count
    students_file = input("Input students json file: ")
    exam_file = input("Input exam json file: ")
    space_file = input("Input file for locations: ")
    save_file = input("Input file to save results: ")

    days = ["M", "T", "W", "R", "F"]
    timeslots = ["8:00am-10:00am", "10:30am-12:30pm", "1:00pm-3:00pm", "3:30pm-5:30pm"]
    locations = []
    exams = []

    with open(students_file, 'r') as studFile:
        data = json.load(studFile)
        for student in data["students"]:
            master_student_list[student["studentID"]] = student["enrolledCourses"]

    with open(exam_file, 'r') as examFile:
        data = json.load(examFile)
        for dept in data["departments"]:
            for course in dept["courses"]:
                exams.append(course["courseID"])
    
    with open(space_file, 'r') as spaceFile:
        data = json.load(spaceFile)
        for location in data["locations"]:
            locations.append(location)


    count = 0  # Reset count before generating schedules
    generation_start = time.time()
    generateSchedules(exams, days, timeslots, locations)
    generation_end = time.time()
    removeConflicts()
    conflict_removal_end = time.time()

    print(f"Generation Time: {generation_end - generation_start:.2f} seconds\n")
    print(f"Removal Time: {conflict_removal_end - generation_end:.2f} seconds\n")
    print(f"Total Time: { conflict_removal_end - generation_start:.2f} seconds\n")
    print(f"Total schedules generated: {count}\n")

    with open(save_file, 'w') as saveFile:
        data = json.dump(master_schedule_list, saveFile, indent = 4)


if __name__ == "__main__":
    main()


            