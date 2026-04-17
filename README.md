# Exam Scheduling Problem - Various Algorithmic Solutions

## Group Members:

- Mason Thomas - [Github](https://github.com/Mason-Boom)
- Joseph Dye - [Github](https://github.com/DyeJoseph)
- Nicholas Russell - [Github](https://github.com/NickSwine)
- Simon Glashauser - [Github](https://github.com/sglasha)


## How to run the project?

You will interface with the project entirely through the [main.py](./codePortion/main.py) file in the codePortion folder. This will allow you to create courses, students and locations. Once you've created or otherwise collected your data files, you can load them into any of the 4 algorithms using the same interface. Data files will be automatically placed in theie respective repositories within the Data folder. The algorithms will look for the files from these repositories, so it is important to place all needed data in the correct location.

You can run the `main.py` file by typing into the terminal (from the root directory) `py .\codePortion\main.py` if on Windows or `python3 ./codePortion/main.py` if on Mac/Linux.
If your IDE allows, the play button on the `main.py` file should also work.

If you wish to modify any of the algorithm code ensure that you properly name the new binary and place it it in the `~/codePortion/alg_binaries` folder. The proper names are:

| Algorithm | Binary Name |
|---:|:---:|
| Brute Force  | bf |
| Greedy | gr |
| Graph Coloring | gc |
| Genetic | ga |

*If you need to compile on a Windows machine, make sure these binary files have the `.exe` file extension.*

### Runtime Inputs
When running the program, the user can choose one of three options.

1. Generate New Data

    - From here, the user may generate courses or students.
    - Generating students required an already generated courses JSON file, so it is important to create courses first.

2. Run An Exam Scheduling Algorithm

    - From here, the user selects to run either the brute force, greedy, graph coloring, or genetic algorithm.
    - Then, the user selects which system they are running so the program knows which binary file to execute
    - JSON file inputs are then required once the executable is run.

3. Exit the Program

***It is very important to run `main.py` from the root directory!***


## Project Description
The Exam Scheduling Problem is an optimization problem that aims to create a conflict free exam timetable. Our algorithm will consider the following constraints: exam length, student schedules, location, and time for when exams can be held. Since the Exam Scheduling Problem will generate an optimized schedule in nonpolynomial time, it is NP-hard.

The Exam Scheduling Problem arises from the need to organize exams efficiently while avoiding conflicts. Each student may be enrolled in multiple courses, and exams must be assigned to time slots and locations such that:

a. No student has overlapping exams
b. No two exams occupy the same room at the same time
c. Exams occur within allowable days and times

The idea is that, given a set of exams for classes at a university, no student is scheduled to be in two exams at once, no exams happen in the same space at the same time, and all exams occur within a certain time of the day over a certain number of days.

The real-world application of the Exam Scheduling Problem is fairly obvious, that being, it used to scheduling exams for schools and universities. Particularly, large universities have thousands of students with hundreds of exams that require a complex solution to efficiently schedule university-wide exams (such as for finals). The Exam Scheduling Problem is not necessarily limited to scheduling exams, however, as it can also be used for any type of scheduling such as employee work schedules (who works where and for how long), event planning (which events are where and making sure they do not overlap), and sports tournaments (making sure a team is not supposed to be in two places at once and games do not overlap or exceed time).

When choosing data (Students, Courses, and Locations) to run the algorithms with, it is important that the Students JSON file matches a corresponding Courses JSON file, as, were a mismatch to occur, students could be mapped to courses that are not present in the courses file. Here are the correlations of which files to use:

|Student Count           | Course Count |
|:----------------------:|:------------:|
|                      4 | 4            |
|                      8 | 4            |
|                     20 | 4            |
|                     40 | 8            |
|                     80 | 16           |
|                    160 | 32           |
|                    320 | 64           |
|                    640 | 128          |
|                   1280 | 256          |
|                   2560 | 512          |
|                   5120 | 1024         |
|                  10240 | 2048         |
|                  20480 | 4096         |
|                  40960 | 8192         |
|                  81920 | 16384        |
|                 123840 | 32768        |
|not present (too large) | 65536        |

It is also important that there are actually enough timeslots to map every exam to a distict timeslot so that two exams do not occupy the same space.
The equation for this is $numLocations * 20 \ge numExams$. The constant $20$ comes from $4$ timeslots a day for $5$ days, so $20$ timeslots per location. 
This formula can be written as $numLocations \ge \frac{numExams}{20}$.

## Checking Results
After running an algorithm (via `main.py`), the results are automatically saved in the `codePortion\Data\Schedules` directory.

To check these results for conflicts (where conflicts are defined as a student scheduled for more than one exam at a time or two exams in the same timeslot and location), run the `check_conflicts.py` file from the root directory by typing into the terminal `py .\codePortion\check_conflicts.py` if on Windows or `python3 ./codePortion/check_conflicts.py` if on Mac/Linux. 
If your IDE allows, the play button on the `check_conflicts.py` file should also work.

## Compiling C-Files

If any alterations are made to the C files, they will require recompilation. Here are the lines to recompile (enter in the terminal while in the `~/codePortion/CFiles` directory):

- Brute Force: `gcc brute_force.c cJSON.c -o bf`
- Greedy: `gcc greedy_algorithm -o gr`
- Graph Coloring: `gcc graph_coloring.c -o gc`
- Genetic: `gcc genetic_algorithm.c cJSON.c -o ga`