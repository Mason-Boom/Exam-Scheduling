import os
import json
import uuid
import random

def generateNewCourses(outputName:str, numToGenerate:int):

    if not outputName.endswith(".json"):
        outputName += ".json"

    if os.path.exists(outputName):
        print("Error - A generated dataset already exists by that name")
        print("Exiting...")
        return

    allCourses = {
        "courses": []
    }

    for _ in range(numToGenerate):
        courseRecord = {
            "courseID": str(uuid.uuid4())
        }
        allCourses["courses"].append(courseRecord)

    jsonEncode = json.dumps(allCourses, indent=4)

    with open(outputName, "w") as file:
            file.write(jsonEncode)
            file.write("\n")


def generateNewStudents(existingCourses:str, outputName:str, numToGenerate:int):

    if not os.path.exists(existingCourses):
        print(f"Error - A course dataset by the name {existingCourses} was not found!")
        print("Exiting...")
        return
    
    with open(existingCourses, "r") as courseFile:
        courseData = json.load(courseFile)

    listedCourses = courseData["courses"]

    numCourses = len(listedCourses)

    allStudents = {
        "students": []
    }

    for _ in range(numToGenerate):
        studentRecord = {
            "studentID": str(uuid.uuid4()),
            "enrolledCourses": []
        }
        for _ in range(5):

            randomIndex = random.randint(0, numCourses-1)
            randomCourse = listedCourses[randomIndex]
            randomCourseID = randomCourse["courseID"]

            studentRecord["enrolledCourses"].append(randomCourseID)
        allStudents["students"].append(studentRecord)
    
    jsonEncode = json.dumps(allStudents, indent=4)

    with open(outputName, "w") as file:
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

                outputName:str = input("What would you like the output file to be called? ")
                numToGen:int = int(input("How many courses should be generated? "))

                generateNewCourses(outputName, numToGen)
            case 2:
                # Generate Students Courses

                existingCourses:str = input("What the file name of the courses would you like to be used? Include the file extension. ")
                outputName:str = input("What would you like the output file to be called? ")
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