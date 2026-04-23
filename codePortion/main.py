# You can run this file either via terminal or your IDEs play button. ***Either way, you must run this file from the project's root directory.*** For the terminal option, if on Windows enter into the terminal 'py .\codePortion\main.py' or if on macOS / Linux enter 'python3 ./codePortion/main.py'. Remember to run these commands from the project's root directory.

from generate_data import mainGenerate
from run_algorithm import mainRunAlgorithm

def main():

    choice:int = 0

    while choice != 3:

        print("------------ Exam Scheduling Algorithms ------------")
        print("What would you like to do?")
        print("\t1. Generate New Data")
        print("\t2. Run An Exam Scheduling Algorithm")
        print("\t3. Exit")

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