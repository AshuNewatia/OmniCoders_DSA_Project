# OMNICODERS DSA PROJECT — D5 ADVERSARY CONTRIBUTION

This package contains only the D5/Adversary deliverables:

adversary/
  Adversary.cpp             Input-pattern generator
  D5_HeapBenchmark.cpp      Benchmark for Structure A and Structure B
  run_experiments.py        Compile and run the experiments
  Report.md                 D5 mathematical proof, methodology, results and conclusion

results/
  d5_experiment_results.csv Raw measurements
  d5_worst_patterns.csv     Aggregate pattern ranking
  d5_comparison_plot.png    Comparison-growth plot

## IMPORTANT — HOW TO USE THIS PACKAGE

These D5 files are designed to be placed into the original OmniCoders_DSA_Project-main
repository, preserving the adversary/ and results/ folder locations. The benchmark
uses the team's existing implementations under:
  src/structA/structure_A.cpp
  src/structB/structureB.cpp

Those source files are intentionally not included here because this is the D5-only
contribution, not a replacement for the full team repository.

To reproduce the experiments:
1. Extract this package.
2. Copy its adversary/ and results/ folders into the root of the original repository,
   merging/replacing the corresponding D5 files.
3. Open a terminal at the repository root.
4. Ensure Python 3 and g++ are available on PATH.
5. Run:
     python adversary/run_experiments.py

The runner writes fresh results to results/d5_experiment_results.csv and
results/d5_worst_patterns.csv. The report describes the recorded run and its limits.