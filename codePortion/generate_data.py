import os
import json
import uuid
import random


def getPossibleDepartments():
    return ["CSC", "ECE", "ART", "MUS", "GEO"]

# Returns a tuple (departmentName, departmentIndex)
def getRandomWeighedDepartment():

    randomIndex = random.randint(1, 100)

    # The end num is not included in the range. Ex: Range(1,5) would only include 1,2,3,4
    if randomIndex in range(1,50):
        return ("CSC", 0)
    elif randomIndex in range(50,70):
        return ("ECE", 1)
    elif randomIndex in range(70,80):
        return ("ART", 2)
    elif randomIndex in range(80,95):
        return ("MUS", 3)
    elif randomIndex in range(95,101):
        return ("GEO", 4)
    
    raise ValueError


def generateNewCourses(outputName:str, numToGenerate:int):

    # Ensure output is in Data/Courses directory
    courses_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Courses")
    os.makedirs(courses_dir, exist_ok=True)
    if not outputName.endswith(".json"):
        outputName += ".json"
    output_path = os.path.join(courses_dir, outputName)
    if os.path.exists(output_path):
        print("Error - A generated dataset already exists by that name in Data/Courses/")
        print("Exiting...")
        return

    overallJSON = {
        "departments": []
    }

    possibleDepartments = getPossibleDepartments()
    for i in range(len(possibleDepartments)):

        departmentRecord = {
            "name":  possibleDepartments[i],
            "courses": []
        }
        overallJSON["departments"].append(departmentRecord)


    for _ in range(numToGenerate):
        ranDepartmentIndex = getRandomWeighedDepartment()[1]

        courseRecord = {
            "courseID": str(uuid.uuid4())
        }

        overallJSON["departments"][ranDepartmentIndex]["courses"].append(courseRecord)

    jsonEncode = json.dumps(overallJSON, indent=4)

    with open(output_path, "w") as file:
        file.write(jsonEncode)
        file.write("\n")


def generateNewStudents(existingCourses:str, outputName:str, numToGenerate:int):

    # Look for courses in Data/Courses directory
    courses_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Courses")
    course_path = os.path.join(courses_dir, existingCourses) if not os.path.isabs(existingCourses) else existingCourses
    if not course_path.endswith(".json"):
        course_path += ".json"
    if not os.path.exists(course_path):
        print(f"Error - A course dataset by the name {course_path} was not found!")
        print("Exiting...")
        return

    # Ensure output is in Data/Students directory
    students_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Data", "Students")
    os.makedirs(students_dir, exist_ok=True)
    if not outputName.endswith(".json"):
        outputName += ".json"
    output_path = os.path.join(students_dir, outputName)
    if os.path.exists(output_path):
        print("Error - A generated student dataset already exists by that name in Data/Students/")
        print("Exiting...")
        return
    
    with open(course_path, "r") as courseFile:
        courseData = json.load(courseFile)

    listedDepartments = courseData["departments"]

    overallJSON = {
        "students": []
    }

    for _ in range(numToGenerate):

        studentMajor = getRandomWeighedDepartment()

        studentRecord = {
            "studentID": str(uuid.uuid4()),
            "major": studentMajor[0],
            "enrolledCourses": []
        }

        enrolledCount = 0
        # targetEnrolledCount = random.randint(4,6)
        targetEnrolledCount = 4
        while enrolledCount < targetEnrolledCount:

            departmentToPullFrom = studentMajor

            # 2/10 chance that the student chooses a course from a diff department
            chooseRandDepartment = random.randint(1,10)
            if chooseRandDepartment >= 8:
                departmentToPullFrom = getRandomWeighedDepartment()

            numCourses = len(listedDepartments[departmentToPullFrom[1]]["courses"])
            while numCourses == 0:
                # No courses were generated for this department. Student must choose a diff one
                departmentToPullFrom = getRandomWeighedDepartment()
                numCourses = len(listedDepartments[departmentToPullFrom[1]]["courses"])
            

            randomIndex = random.randint(0, numCourses-1)
            randomCourse = listedDepartments[departmentToPullFrom[1]]["courses"][randomIndex]
            randomCourseID = randomCourse["courseID"]

            if randomCourseID not in studentRecord["enrolledCourses"]:
                studentRecord["enrolledCourses"].append(randomCourseID)
                enrolledCount += 1 # Only increment when a unique course is added
        
        overallJSON["students"].append(studentRecord)
    
    jsonEncode = json.dumps(overallJSON, indent=4)

    with open(output_path, "w") as file:
        file.write(jsonEncode)
        file.write("\n")



def mainGenerate():

    choice:int = 0

    while choice != 3:

        print("----------------------------------------------------------")
        print("--------------------- Generate Data ----------------------")
        print("What would you like to do?")
        print("1. Generate New Courses")
        print("2. Generate New Students")
        print("3. Exit")

        choice = int(input(""))

        match choice:
            case 1:
                # Generate New Courses

                outputName:str = input("What would you like the output file to be called? (It will be saved in Data/Courses/) ")
                numToGen:int = int(input("How many courses should be generated? "))

                generateNewCourses(outputName, numToGen)
            case 2:
                # Generate Students Courses

                existingCourses:str = input("What the file name of the courses would you like to be used? Include the file extension. ")
                outputName:str = input("What would you like the output file to be called? (It will be saved in Data/Students/) ")
                numToGen:int = int(input("How many students should be generated? "))

                generateNewStudents(existingCourses, outputName, numToGen)

            case 3:
                # Exit Program
                print("Exiting Program...")
            case _:
                print("Please input a valid value between 1 and 3.")
                print("")
                print("")
        
        print("")
        print("")
    
    return