# Emergency Triage Queue Simulator

## Project Description
This project implements an emergency triage queue simulator for a casualty ward. It evaluates and compares the performance of an indexed binary min-heap (Structure A) and a binary heap with lazy invalidation (Structure B) against a baseline unsorted array oracle. The application replays 30 simulated days of patient arrivals, severity revisions, departures, and treatments under both linear aging and step-wise escalation policies[cite: 6].

## Team Roles
The six roles for this project are fixed and assigned as follows:
* **Structure A Owner:** Adityamoni
* **Structure B Owner:** - Anuj Kumar
* **Architect:** - Ashu Newatia
* **Oracle Owner:** - Rohit Jhajhra
* **Adversary:** - Kambhampaty Krupalini Paul
* **Analyst:** - Ritik Sarawag

## Build Instructions
To compile the core structures, application simulator, differential tests, and benchmarks[cite: 2], run the following command from the repository root:
`make all`

## Execution Command
Use this single command to sequentially run the differential tests, the triage application simulator, and every performance benchmark[cite: 2]:
`make run_all`