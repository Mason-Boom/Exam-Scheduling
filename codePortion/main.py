from generate_data import mainGenerate
from run_algorithm import mainRunAlgorithm

def main():

    choice:int = 0

    while choice != 3:

        print("------------ Exam Scheduling Algorithms ------------")
        print("What would you like to do?")
        print("1. Generate New Data")
        print("2. Run An Exam Scheduling Algorithm")
        print("3. Exit")

        print("")
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