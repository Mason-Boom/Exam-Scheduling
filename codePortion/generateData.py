def generateNewCourses(outputName:str):
    print(outputName)

def generateNewStudents(existingCourses:str, outputName:str):
    print(existingCourses)
    print(outputName)

def mainGenerate():

    choice:int = 0

    while choice != 3:

        print("------------ --- ---- ---- ---------- ------- ------------")
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

                outputName:str = input("What would you like the output file to be called?")

                generateNewCourses(outputName)
            case 2:
                # Generate Students Courses

                existingCourses:str = input("What the file name of the courses would you like to be used? Include the file extension.")
                outputName:str = input("What would you like the output file to be called?")

                generateNewStudents(existingCourses, outputName)

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