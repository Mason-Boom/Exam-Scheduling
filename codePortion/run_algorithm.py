import os
import subprocess

def mainRunAlgorithm():

    print("----------------------------------------------------------")
    print("-------------------- Run An Algorithm --------------------")

    algorithmChoice = 0
    while algorithmChoice not in range(1, 5):
        print("What algorithm would you like to use?")
        print("\t1. Brute Force")
        print("\t2. Greedy")
        print("\t3. Graph Coloring")
        print("\t4. Genetic")
        algorithmChoice = int(input(""))

    systemChoice = 0
    while systemChoice not in range (1, 3):
        print("")
        print("Which system are you using?")
        print("\t1. Windows")
        print("\t2. Mac/Linux")
        systemChoice = int(input("Enter 1 or 2: "))

    if systemChoice == 1:
        executables = {
            1: "./codePortion/alg_binaries/bf.exe",
            2: "./codePortion/alg_binaries/gr.exe",
            3: "./codePortion/alg_binaries/gc.exe",
            4: "./codePortion/alg_binaries/ga.exe",
        }
    else:
        executables = {
            1: "./codePortion/alg_binaries/bf",
            2: "./codePortion/alg_binaries/gr",
            3: "./codePortion/alg_binaries/gc",
            4: "./codePortion/alg_binaries/ga",
        }

    executable = executables[algorithmChoice]

    if not os.path.exists(executable):
        print(f"Error: executable '{executable}' not found. Did you compile it with the correct name?")
        return

    print(f"\nLaunching {executable}...\n")

    #Prompts for the JSON files will come via running the executable
    result = subprocess.run([executable])

    if result.returncode != 0:
        print(f"\nAlgorithm exited with error code {result.returncode}.")

    print("")
    print("")
    
    return