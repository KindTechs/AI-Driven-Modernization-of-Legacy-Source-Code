#!/bin/bash

# Script to establish baseline measurements for Mozilla 1.0 modernization

echo "===== Establishing Baseline Measurements ====="
echo "Date: $(date)"
echo "============================================="

# Set base directory
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$BASE_DIR"

# Create baseline directory
mkdir -p analysis/baselines

# Run KPI measurement on nsSelection.cpp specifically
echo "Step 1: Measuring nsSelection.cpp KPIs..."
echo "======================================="

NSSELECTION_FILE="content/base/src/nsSelection.cpp"
if [ -f "$NSSELECTION_FILE" ]; then
    python3 analysis/measure_modernization_kpis.py \
        --root . \
        --files "$NSSELECTION_FILE" \
        --output "analysis/baselines/nsSelection_baseline_kpis.txt"
    
    echo "nsSelection.cpp KPIs measured"
else
    echo "Warning: nsSelection.cpp not found at expected location"
fi

# Run progress tracking to establish baseline
echo
echo "Step 2: Establishing nsSelection progress baseline..."
echo "================================================="

python3 analysis/track_nsSelection_progress.py \
    --root . \
    --output "analysis/baselines/nsSelection_baseline_progress.txt" \
    --save-baseline "analysis/baselines/nsSelection_baseline.json"

echo "nsSelection progress baseline established"

# Run full codebase analysis for context
echo
echo "Step 3: Running full codebase analysis..."
echo "======================================="

python3 analysis/measure_modernization_kpis.py \
    --root . \
    --output "analysis/baselines/codebase_baseline_kpis.txt"

echo "Full codebase baseline established"

# Generate initial comprehensive report
echo
echo "Step 4: Generating baseline report..."
echo "==================================="

python3 analysis/generate_kpi_report.py \
    --root . \
    --kpi-data "analysis/baselines/codebase_baseline_kpis.json" \
    --progress-data "analysis/baselines/nsSelection_baseline.json" \
    --report-type comprehensive \
    --output "analysis/baselines/baseline_comprehensive_report.txt"

echo "Baseline comprehensive report generated"

echo
echo "Step 5: Summary of baseline files created..."
echo "=========================================="

echo "Baseline files created:"
echo "- analysis/baselines/nsSelection_baseline_kpis.txt"
echo "- analysis/baselines/nsSelection_baseline_kpis.json"
echo "- analysis/baselines/nsSelection_baseline_progress.txt"
echo "- analysis/baselines/nsSelection_baseline.json"
echo "- analysis/baselines/codebase_baseline_kpis.txt"
echo "- analysis/baselines/codebase_baseline_kpis.json"
echo "- analysis/baselines/baseline_comprehensive_report.txt"

echo
echo "Baseline establishment completed!"
echo "==============================="