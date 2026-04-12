# CSC2400FinalProject

## Group Members:

- Mason Thomas - [Github](https://github.com/Mason-Boom)
- Joseph Dye - [Github](https://github.com/DyeJoseph)
- Nicholas Russell - [Github](https://github.com/NickSwine)
- Simon Glashauser - [Github](https://github.com/sglasha)


## How to run the project?

You will interface with the project entirely through the [main.py](./codePortion/main.py) file in the codePortion folder. This will allow you to create courses, students and locations. Once you've created or otherwise collected your data files, you can load them into any of the 4 algorithms using the same interface. 

If you wish to modify any of the algorithm code ensure that you properly name the new binary and place it it in the `~/codePortion/alg_binaries` folder. The proper names are:

| Algorithm | Binary Name |
|---:|:---:|
| Brute Force  | bf |
| Greedy | gr |
| Graph Coloring | gc |
| Genetic | ga |


## Project Description
The Exam Scheduling Problem is an optimization problem that aims to create a conflict free exam timetable. Our algorithm will consider the following constraints: exam length, student schedules, location, and time for when exams can be held. Since the Exam Scheduling Problem will generate an optimized schedule in nonpolynomial time, it is NP-hard.

The Exam Scheduling Problem arises from the need to organize exams efficiently while avoiding conflicts. Each student may be enrolled in multiple courses, and exams must be assigned to time slots and locations such that:

a. No student has overlapping exams
b. No two exams occupy the same room at the same time
c. Exams occur within allowable days and times

The idea is that, given a set of exams for classes at a university, no student is scheduled to be in two exams at once, no exams happen in the same space at the same time, and all exams occur within a certain time of the day over a certain number of days.

The real-world application of the Exam Scheduling Problem is fairly obvious, that being, it used to scheduling exams for schools and universities. Particularly, large universities have thousands of students with hundreds of exams that require a complex solution to efficiently schedule university-wide exams (such as for finals). The Exam Scheduling Problem is not necessarily limited to scheduling exams, however, as it can also be used for any type of scheduling such as employee work schedules (who works where and for how long), event planning (which events are where and making sure they do not overlap), and sports tournaments (making sure a team is not supposed to be in two places at once and games do not overlap or exceed time).