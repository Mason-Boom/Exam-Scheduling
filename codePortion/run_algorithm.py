import os
import subprocess

def mainRunAlgorithm():

    print("----------------------------------------------------------")
    print("-------------------- Run An Algorithm --------------------")


    print("")
    print("What courses need an exam schedule?")
    coursesFile = input("")

    print("What students are enrolled in those courses?")
    studentsFile = input("")

    print("What locations are available for exams?")
    locationsFile = input("")

    print("What should the output file be called?")
    outputFile = input("")

    algorithmChoice = 0
    while algorithmChoice not in range(1, 5):
        print("Finally, what algorithm would you like to use?")
        print("\t1. Brute Force")
        print("\t2. Greedy")
        print("\t3. Graph Coloring")
        print("\t4. Genetic")
        algorithmChoice = int(input(""))

    executables = {
        1: "./alg_ binaries/bf",
        2: "./alg_ binaries/gr",
        3: "./alg_ binaries/gc",
        4: "./alg_ binaries/ga",
    }
    executable = executables[algorithmChoice]

    if not os.path.exists(executable):
        print(f"Error: executable '{executable}' not found. Did you compile it with the correct name?")
        return

    # All four C programs prompt for the same four inputs in the same order so we can pipe in data via newline separated stdin
    stdin_input = "\n".join([studentsFile, coursesFile, locationsFile, outputFile]) + "\n"

    print(f"\nLaunching {executable}...\n")

    result = subprocess.run(
        [executable],
        input=stdin_input,
        text=True           # strings instead of bytes
    )

    if result.returncode != 0:
        print(f"\nAlgorithm exited with error code {result.returncode}.")

    print("")
    print("")
    
    return