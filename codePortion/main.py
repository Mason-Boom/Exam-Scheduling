from generateData import mainGenerate
from runAlgorithm import mainRunAlgorithm

def main():

    choice:int = 0

    while choice != 3:

        print("------------ CSC 2400 Exam Scheduling Project ------------")
        print("What would you like to do?")
        print("1. Generate New Data")
        print("2. Run An Exam Scheduling Algorithm")
        print("3. Exit")

        choice = int(input(""))

        match choice:
            case 1:
                # Generate New Data
                mainGenerate()
            case 2:
                # Run An Exam Scheduling Algorithm
                mainRunAlgorithm()
            case 3:
                # Exit Program
                print("Exiting Program...")
            case _:
                print("Please input a valid value between 1 and 3.")
                print("")
                print("")
        
        print("")
        print("")

if __name__ == "__main__":
    main()