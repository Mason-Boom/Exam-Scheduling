#!/usr/bin/env python3
"""
benchmark.py — Batch benchmark runner for exam-scheduling algorithms.

IMPORTANT: Run this script from the PROJECT ROOT directory
           (the folder that contains the 'codePortion' directory).

Usage:
    python3 benchmark.py           # on Mac / Linux
    py      benchmark.py           # on Windows
"""

import os
import sys
import re
import csv
import time
import subprocess
import platform
from datetime import datetime
from math import ceil

# ─── Student count → Course count pairs (from README table) ─────────────────
STUDENT_COURSE_PAIRS = [
    (4,       4),
    (8,       4),
    (20,      4),
    (40,      8),
    (80,      16),
    (160,     32),
    (320,     64),
    (640,     128),
    (1280,    256),
    (2560,    512),
    (5120,    1024),
    (10240,   2048),
    (20480,   4096),
    (40960,   8192),
    (81920,   16384),
    (123840,  32768),
]

# ─── Location counts: 2^x for x ∈ {2, 3, …, 11} ────────────────────────────
#     i.e. 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048
LOCATION_COUNTS = [2 ** x for x in range(2, 12)]

# ─── Feasibility constant from README ────────────────────────────────────────
#     numLocations * 20 >= numCourses must hold (5 days × 4 slots = 20 per room)
SLOTS_PER_LOCATION = 20

# ─── Default per-run timeout (seconds) ───────────────────────────────────────
DEFAULT_TIMEOUT = 7200 # 2 Hours - 60 * 60 * 2

# ─── Algorithm registry ──────────────────────────────────────────────────────
ALGORITHMS = {
    "1": {"name": "Brute Force",    "short": "bf"},
    "2": {"name": "Greedy",         "short": "gr"},
    "3": {"name": "Graph Coloring", "short": "gc"},
    "4": {"name": "Genetic",        "short": "ga"},
}

# ─── CSV columns ─────────────────────────────────────────────────────────────
#     Columns that do not apply to a given algorithm are left blank.
CSV_COLUMNS = [
    # Run identity
    "student_count",
    "course_count",
    "location_count",
    "status",                       # success | timeout | error | skipped

    # Timing (seconds)
    "total_time_sec",

    # BF-specific counts
    "complete_assignments_tried",   # BF: total complete slot-assignments evaluated
    "valid_schedule_found",         # BF: Yes | No

    # Non-BF schedule counts
    "total_schedules_generated",    # GR=1, GC=1, GA=population*generations_run

    # Quality reported by the algorithm itself
    "conflict_free",                # True | False (from algorithm output)
    "student_conflicts",            # GR / GC / GA: conflicts the alg. reports

    # GC-specific
    "time_slots_used",              # GC: "used/total"

    # GA-specific
    "best_fitness",
    "perfect_fitness",

    # check_conflicts.py verification (all algorithms)
    "checker_student_conflicts",    # exact summary line from check_conflicts.py
    "checker_location_conflicts",   # exact summary line from check_conflicts.py

    # Freeform
    "notes",
]

# ═════════════════════════════════════════════════════════════════════════════
# Output parsers — one per algorithm
# Each receives the full combined stdout string and returns a partial dict
# matching CSV_COLUMNS.
# ═════════════════════════════════════════════════════════════════════════════

def _search(pattern: str, text: str, group: int = 1, default: str = "") -> str:
    """Return the first regex match group, or `default` if no match."""
    m = re.search(pattern, text)
    return m.group(group).strip() if m else default


def parse_brute_force(output: str) -> dict:
    # New BF is a first-valid-solution backtracker with pruning.
    # Relevant output lines:
    #   "Valid schedule found after evaluating %d complete assignment(s)."  (only if found)
    #   "No valid schedule exists for the given inputs."                    (only if not found)
    #   "Total Time:              %.2f seconds"
    #   "Complete assignments tried: %d"
    #   "Valid schedule found:    Yes | No"
    row: dict = {}
    row["total_time_sec"]            = _search(r"Total Time:\s+([\d.]+)", output)
    row["complete_assignments_tried"] = _search(r"Complete assignments tried:\s+(\d+)", output)

    found_str = _search(r"Valid schedule found:\s+(\w+)", output)
    row["valid_schedule_found"]      = found_str   # "Yes" or "No"

    # A "Yes" from the backtracker means it produced a conflict-free schedule by
    # construction (pruning guarantees it), so conflict_free mirrors that directly.
    row["conflict_free"]             = "True" if found_str == "Yes" else "False"

    # Fields that belong to other algorithms — leave blank
    row["total_schedules_generated"] = ""
    row["student_conflicts"]         = ""
    row["time_slots_used"]           = ""
    row["best_fitness"]              = ""
    row["perfect_fitness"]           = ""
    row["notes"]                     = "" if found_str == "Yes" else "No valid schedule exists"
    return row


def parse_greedy(output: str) -> dict:
    row: dict = {}
    row["total_time_sec"]            = _search(r"Total Time:\s+([\d.]+)", output)
    row["total_schedules_generated"] = _search(r"Total schedules generated:\s+(\d+)", output)
    row["complete_assignments_tried"] = ""
    row["valid_schedule_found"]      = ""
    row["time_slots_used"]           = ""
    row["best_fitness"]              = ""
    row["perfect_fitness"]           = ""

    if "CONFLICT-FREE" in output:
        row["conflict_free"]     = "True"
        row["student_conflicts"] = "0"
        row["notes"]             = ""
    else:
        row["conflict_free"]     = "False"
        row["student_conflicts"] = _search(r"conflicts introduced:\s+(\d+)", output)
        row["notes"]             = "Greedy could not achieve a conflict-free schedule"
    return row


def parse_graph_coloring(output: str) -> dict:
    row: dict = {}
    row["total_time_sec"]            = _search(r"Total time:\s+([\d.]+)", output)
    row["total_schedules_generated"] = "1"
    row["complete_assignments_tried"] = ""
    row["valid_schedule_found"]      = ""
    row["best_fitness"]              = ""
    row["perfect_fitness"]           = ""

    m = re.search(r"Time slots used:\s+(\d+)\s*/\s*(\d+)", output)
    row["time_slots_used"] = f"{m.group(1)}/{m.group(2)}" if m else ""

    has_warning    = "Warning" in output
    has_cf_phrase  = "conflict-free" in output.lower()
    if has_cf_phrase and not has_warning:
        row["conflict_free"]     = "True"
        row["student_conflicts"] = "0"
        row["notes"]             = ""
    else:
        row["conflict_free"]     = "False"
        row["student_conflicts"] = _search(r"(\d+) student-conflict\(s\) remain", output)
        row["notes"]             = "GC schedule has remaining student conflicts"
    return row


def parse_genetic(output: str) -> dict:
    row: dict = {}
    row["total_time_sec"]            = _search(r"Total time\s*:\s+([\d.]+)", output)
    row["complete_assignments_tried"] = ""
    row["valid_schedule_found"]      = ""
    row["time_slots_used"]           = ""

    # Use population size * actual generations run as "chromosomes evaluated"
    pop  = _search(r"Population\s*:\s+(\d+)", output)
    # Count how many "Gen XXXX |" lines actually appeared (early-exit aware)
    actual_gens = len(re.findall(r"^Gen\s+\d+\s+\|", output, re.MULTILINE))
    if pop and actual_gens:
        row["total_schedules_generated"] = str(int(pop) * actual_gens)
    else:
        row["total_schedules_generated"] = ""

    # Fitness can be negative when heavy penalties dominate — use -? to capture sign
    best_fit = _search(r"Best fitness\s*:\s+(-?\d+)", output)
    perf_fit = _search(r"perfect\s*=\s*(-?\d+)", output)
    row["best_fitness"]    = best_fit
    row["perfect_fitness"] = perf_fit

    approx_conflicts = _search(
        r"Approx\.\s+unresolved student conflicts\s*:\s+(\d+)", output
    )
    row["student_conflicts"] = approx_conflicts

    if best_fit and perf_fit and best_fit == perf_fit:
        row["conflict_free"] = "True"
        row["notes"]         = "Perfect fitness achieved"
    elif best_fit and perf_fit:
        row["conflict_free"] = "False"
        row["notes"]         = f"Best fitness {best_fit} / {perf_fit}"
    else:
        row["conflict_free"] = ""
        row["notes"]         = "Could not parse fitness from output"
    return row


PARSERS = {
    "bf": parse_brute_force,
    "gr": parse_greedy,
    "gc": parse_graph_coloring,
    "ga": parse_genetic,
}


# ═════════════════════════════════════════════════════════════════════════════
# Helpers
# ═════════════════════════════════════════════════════════════════════════════

def resolve_binary(short: str) -> str:
    """Return the path to the algorithm binary (adds .exe on Windows)."""
    ext = ".exe" if platform.system() == "Windows" else ""
    return os.path.join(".", "codePortion", "alg_binaries", short + ext)


def build_stdin(short: str, students_file: str, courses_file: str,
                locations_file: str, schedule_file: str) -> str:
    """Assemble the exact stdin the binary expects, newline-terminated."""
    lines = [students_file, courses_file, locations_file, schedule_file]
    if short == "ga":
        lines.append("N")   # decline parameter customisation → use defaults
    return "\n".join(lines) + "\n"


def run_binary(binary_path: str, stdin_text: str,
               timeout: int) -> tuple:
    """
    Execute the binary and return (returncode, stdout, stderr, timed_out).
    timed_out=True means the process was killed by the timeout.
    """
    try:
        result = subprocess.run(
            [binary_path],
            input=stdin_text,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.returncode, result.stdout, result.stderr, False
    except subprocess.TimeoutExpired:
        return -1, "", "", True
    except FileNotFoundError:
        return -2, "", f"Binary not found: {binary_path}", False
    except Exception as exc:
        return -3, "", str(exc), False


def fmt_time(value: str, wall: float) -> str:
    """Return a display-friendly time string for the console progress line."""
    try:
        return f"{float(value):.3f}s"
    except (ValueError, TypeError):
        return f"~{wall:.1f}s (wall)"


def data_file_path(subfolder: str, filename: str) -> str:
    return os.path.join(".", "codePortion", "Data", subfolder, filename)


def run_conflict_checker(schedule_file: str, students_file: str,
                         timeout: int) -> tuple:
    """
    Run check_conflicts.py and return (student_summary, location_summary).

    student_summary  — one of:
        "No student conflicts found!"
        "Total student conflicts found: N"
        "(checker skipped)" / "(checker error)"

    location_summary — one of:
        "No location conflicts found!"
        "(checker skipped)" / "(checker error)"
        (location conflicts themselves are not stored, only the summary line)

    check_conflicts.py lives at ./codePortion/check_conflicts.py and expects
    two interactive prompts: schedule filename, then students filename.
    """
    checker_path = os.path.join(".", "codePortion", "check_conflicts.py")
    if not os.path.isfile(checker_path):
        return "(checker skipped: check_conflicts.py not found)", "(checker skipped)"

    # Verify the schedule file was actually written before bothering to run
    schedule_path = data_file_path("Schedules", schedule_file)
    if not os.path.isfile(schedule_path):
        return "(checker skipped: no schedule file)", "(checker skipped)"

    stdin_text = f"{schedule_file}\n{students_file}\n"
    try:
        result = subprocess.run(
            [sys.executable, checker_path],
            input=stdin_text,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        out = result.stdout
    except subprocess.TimeoutExpired:
        return "(checker timeout)", "(checker timeout)"
    except Exception as exc:
        return f"(checker error: {exc})", "(checker error)"

    # Pull out exactly the two summary lines we care about
    student_summary  = ""
    location_summary = ""
    for line in out.splitlines():
        line = line.strip()
        if line.startswith("No student conflicts found!") or \
                line.startswith("Total student conflicts found:"):
            student_summary = line
        if line.startswith("No location conflicts found!"):
            location_summary = line

    if not student_summary:
        student_summary = "(checker ran but produced no student summary)"
    if not location_summary:
        # Location conflicts were found — the checker prints individual conflict
        # lines but no "total" line.  Record that conflicts were detected.
        location_summary = "(location conflicts detected — see schedule file)"

    return student_summary, location_summary


# ═════════════════════════════════════════════════════════════════════════════
# Main
# ═════════════════════════════════════════════════════════════════════════════

def main() -> None:
    print("=" * 65)
    print("  Exam Scheduling Algorithm Batch Benchmark")
    print("=" * 65)
    print()

    # ── Algorithm selection ──────────────────────────────────────────────
    print("Select an algorithm to benchmark:")
    for key, info in ALGORITHMS.items():
        print(f"  {key}. {info['name']}")
    print()
    choice = input("Enter choice (1-4): ").strip()
    if choice not in ALGORITHMS:
        print("Invalid choice. Exiting.")
        sys.exit(1)

    alg   = ALGORITHMS[choice]
    short = alg["short"]
    name  = alg["name"]

    # ── Verify binary exists ─────────────────────────────────────────────
    bin_path = resolve_binary(short)
    if not os.path.isfile(bin_path):
        print(f"\nERROR: Binary not found at '{bin_path}'.")
        print(
            "Make sure you are running this script from the project root\n"
            "and that the binary has been compiled and placed in\n"
            "  codePortion/alg_binaries/\n"
            "with the correct name (bf / gr / gc / ga, +.exe on Windows)."
        )
        sys.exit(1)

    # ── Timeout ──────────────────────────────────────────────────────────
    timeout_raw = input(
        f"Per-run timeout in seconds (press Enter for default {DEFAULT_TIMEOUT}s): "
    ).strip()
    timeout = int(timeout_raw) if timeout_raw.isdigit() else DEFAULT_TIMEOUT

    # ── Resume prompt ────────────────────────────────────────────────────
    print()
    resume_raw = input("Resume a previous run? (Y/N): ").strip().upper()
    resuming   = resume_raw == "Y"

    start_at = 1   # 1-based index of the first feasible run to actually execute

    if resuming:
        start_raw = input(
            "Start at iteration number (e.g. 76 to resume from 76/115): "
        ).strip()
        if start_raw.isdigit() and int(start_raw) >= 1:
            start_at = int(start_raw)
        else:
            print("Invalid number — starting from the beginning.")

        csv_raw = input(
            "CSV filename to append to (press Enter to create a new one): "
        ).strip()
        if csv_raw and os.path.isfile(csv_raw):
            csv_filename = csv_raw
            csv_open_mode  = "a"   # append — no header written
            write_header   = False
        else:
            if csv_raw and not os.path.isfile(csv_raw):
                print(f"  '{csv_raw}' not found — creating a new CSV instead.")
            csv_filename   = f"{short}-{datetime.now().strftime('%Y%m%d-%H%M%S')}.csv"
            csv_open_mode  = "w"
            write_header   = True
    else:
        csv_filename  = f"{short}-{datetime.now().strftime('%Y%m%d-%H%M%S')}.csv"
        csv_open_mode = "w"
        write_header  = True

    # ── Pre-compute total run count (for progress display) ───────────────
    total_runs = sum(
        1
        for (students, courses) in STUDENT_COURSE_PAIRS
        for locs in LOCATION_COUNTS
        if locs * SLOTS_PER_LOCATION >= courses
    )

    print()
    print(f"Algorithm      : {name}")
    print(f"Binary         : {bin_path}")
    print(f"Timeout/run    : {timeout}s")
    print(f"Total valid runs: {total_runs}  (infeasible location/course combos skipped)")
    if resuming:
        print(f"Resuming from  : iteration {start_at}/{total_runs}")
    print(f"Output CSV     : {csv_filename}  ({'appending' if csv_open_mode == 'a' else 'new file'})")
    print()
    print("Starting benchmark — results are flushed to CSV after every run,")
    print("so partial data is safe even if you interrupt early.")
    print("-" * 65)

    run_num = 0

    with open(csv_filename, csv_open_mode, newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_COLUMNS)
        if write_header:
            writer.writeheader()
        csvfile.flush()

        for (students, courses) in STUDENT_COURSE_PAIRS:
            for locs in LOCATION_COUNTS:

                # ── Feasibility gate ─────────────────────────────────────
                #    numLocations * 20 >= numCourses  (README constraint)
                if locs * SLOTS_PER_LOCATION < courses:
                    continue   # silently skip — too few rooms to schedule all exams

                run_num += 1

                # ── Resume skip ──────────────────────────────────────────
                #    Silently fast-forward until we reach the requested
                #    start iteration; nothing is printed or executed.
                if run_num < start_at:
                    continue

                label = (
                    f"[{run_num:>{len(str(total_runs))}}/{total_runs}]"
                    f"  S={students:<7d} C={courses:<6d} L={locs:<5d}"
                )
                print(f"{label} … ", end="", flush=True)

                # ── Build file names ─────────────────────────────────────
                students_file  = f"{students}Students.json"
                courses_file   = f"{courses}Courses.json"
                locations_file = f"{locs}Locations.json"
                schedule_file  = f"{short}-{students}-{courses}-{locs}.json"

                # ── Verify data files exist ──────────────────────────────
                needed = {
                    "Students":  data_file_path("Students",  students_file),
                    "Courses":   data_file_path("Courses",   courses_file),
                    "Locations": data_file_path("Locations", locations_file),
                }
                missing = [fn for fn, fp in needed.items() if not os.path.isfile(fp)]
                if missing:
                    print(f"SKIPPED  (missing: {', '.join(missing)})")
                    row = {col: "" for col in CSV_COLUMNS}
                    row.update(
                        student_count  = students,
                        course_count   = courses,
                        location_count = locs,
                        status         = "skipped",
                        notes          = f"Missing data files: {', '.join(missing)}",
                    )
                    writer.writerow(row)
                    csvfile.flush()
                    continue

                # ── Run the binary ───────────────────────────────────────
                stdin_text = build_stdin(
                    short, students_file, courses_file, locations_file, schedule_file
                )
                wall_start = time.perf_counter()
                rc, stdout, stderr, timed_out = run_binary(bin_path, stdin_text, timeout)
                wall_time  = time.perf_counter() - wall_start

                # ── Build CSV row ────────────────────────────────────────
                row = {col: "" for col in CSV_COLUMNS}
                row["student_count"]  = students
                row["course_count"]   = courses
                row["location_count"] = locs

                if timed_out:
                    print(f"TIMEOUT  (>{timeout}s)")
                    row["status"]         = "timeout"
                    row["total_time_sec"] = f">{timeout}"
                    row["notes"]          = f"Process killed after {timeout}s"

                elif rc == -2:
                    # Binary disappeared between the initial check and the run
                    print("ERROR    (binary not found)")
                    row["status"] = "error"
                    row["notes"]  = "Binary not found at runtime"

                elif rc not in (0, -1, -2, -3) or rc != 0:
                    # Non-zero exit — the binary itself reported an error
                    print(f"ERROR    (exit code {rc})")
                    row["status"] = "error"
                    # Capture the first meaningful error line from stderr or stdout
                    err_text = (stderr.strip() or stdout.strip())
                    row["notes"] = err_text[:300] if err_text else f"exit code {rc}"

                else:
                    # ── Success: parse stdout for stats ──────────────────
                    parsed = PARSERS[short](stdout)
                    row.update(parsed)
                    row["status"] = "success"

                    # ── Run check_conflicts.py to independently verify ────
                    s_summary, l_summary = run_conflict_checker(
                        schedule_file, students_file, timeout
                    )
                    row["checker_student_conflicts"]  = s_summary
                    row["checker_location_conflicts"] = l_summary

                    t_display = fmt_time(row.get("total_time_sec", ""), wall_time)
                    cf        = row.get("conflict_free", "?")
                    print(f"OK       ({t_display}  conflict-free={cf})")

                writer.writerow(row)
                csvfile.flush()   # incremental write — safe on interruption

    print("-" * 65)
    print(f"\nAll done!  Results saved to: {csv_filename}")
    print(
        f"Completed {run_num} run(s).  "
        f"Open the CSV in Excel or any spreadsheet tool to analyse results."
    )


if __name__ == "__main__":
    main()