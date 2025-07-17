#!/bin/bash

# AI-Driven Mozilla 1.0 Codebase Modernization - KPI Update Script
# This script runs all measurement scripts and generates timestamped reports

echo "===== Mozilla 1.0 Modernization KPI Update ====="
echo "Started at: $(date)"
echo "=============================================="

# Set base directory
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"

# Create necessary directories
mkdir -p analysis/reports
mkdir -p analysis/baselines

# Set timestamp for this run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DATE_ONLY=$(date +%Y%m%d)

# Define output files
KPI_REPORT="analysis/reports/kpi_report_${TIMESTAMP}.txt"
KPI_DATA="analysis/reports/kpi_report_${TIMESTAMP}.json"
PROGRESS_REPORT="analysis/reports/nsSelection_progress_${TIMESTAMP}.txt"
PROGRESS_DATA="analysis/reports/nsSelection_progress_${TIMESTAMP}.json"
COMPREHENSIVE_REPORT="analysis/reports/comprehensive_report_${TIMESTAMP}.txt"

echo
echo "Step 1: Running codebase analysis..."
echo "==================================="

# Run the main codebase analysis script
if [ -f "analysis/index_codebase.sh" ]; then
    echo "Running codebase indexing..."
    bash analysis/index_codebase.sh > "analysis/reports/codebase_analysis_${TIMESTAMP}.txt"
    echo "Codebase analysis completed: analysis/reports/codebase_analysis_${TIMESTAMP}.txt"
else
    echo "Warning: Codebase analysis script not found"
fi

echo
echo "Step 2: Running complexity analysis..."
echo "======================================"

# Run the complexity analysis script
if [ -f "analysis/analyze_complexity.py" ]; then
    echo "Running complexity analysis..."
    python3 analysis/analyze_complexity.py --root . --output "analysis/reports/complexity_analysis_${TIMESTAMP}.txt"
    echo "Complexity analysis completed"
else
    echo "Warning: Complexity analysis script not found"
fi

echo
echo "Step 3: Measuring modernization KPIs..."
echo "======================================="

# Run the KPI measurement script
if [ -f "analysis/measure_modernization_kpis.py" ]; then
    echo "Measuring KPIs..."
    python3 analysis/measure_modernization_kpis.py --root . --output "$KPI_REPORT"
    
    if [ -f "$KPI_DATA" ]; then
        echo "KPI measurement completed: $KPI_DATA"
    else
        echo "Warning: KPI measurement may have failed"
    fi
else
    echo "Warning: KPI measurement script not found"
fi

echo
echo "Step 4: Tracking nsSelection progress..."
echo "======================================="

# Run the progress tracking script
if [ -f "analysis/track_nsSelection_progress.py" ]; then
    echo "Tracking nsSelection progress..."
    
    # Check if baseline exists
    BASELINE_FILE="analysis/baselines/nsSelection_baseline.json"
    if [ -f "$BASELINE_FILE" ]; then
        echo "Comparing with baseline..."
        python3 analysis/track_nsSelection_progress.py --root . --output "$PROGRESS_REPORT" --baseline "$BASELINE_FILE"
    else
        echo "No baseline found, creating initial baseline..."
        python3 analysis/track_nsSelection_progress.py --root . --output "$PROGRESS_REPORT" --save-baseline "$BASELINE_FILE"
    fi
    
    if [ -f "$PROGRESS_DATA" ]; then
        echo "Progress tracking completed: $PROGRESS_DATA"
    else
        echo "Warning: Progress tracking may have failed"
    fi
else
    echo "Warning: Progress tracking script not found"
fi

echo
echo "Step 5: Generating comprehensive report..."
echo "=========================================="

# Run the comprehensive report generator
if [ -f "analysis/generate_kpi_report.py" ]; then
    echo "Generating comprehensive report..."
    
    # Build arguments for trend analysis
    TREND_ARGS=""
    if ls analysis/reports/kpi_report_*.json > /dev/null 2>&1; then
        TREND_FILES=(analysis/reports/kpi_report_*.json)
        # Get the last 5 files for trend analysis
        if [ ${#TREND_FILES[@]} -gt 5 ]; then
            TREND_FILES=("${TREND_FILES[@]: -5}")
        fi
        TREND_ARGS="--trend-files ${TREND_FILES[*]}"
    fi
    
    # Generate comprehensive report
    python3 analysis/generate_kpi_report.py \
        --root . \
        --kpi-data "$KPI_DATA" \
        --progress-data "$PROGRESS_DATA" \
        --baseline "analysis/baselines/nsSelection_baseline.json" \
        --report-type comprehensive \
        $TREND_ARGS
    
    echo "Comprehensive report generated"
else
    echo "Warning: Report generator script not found"
fi

echo
echo "Step 6: Creating symbolic links to latest reports..."
echo "=================================================="

# Create symbolic links to the latest reports
cd analysis/reports

# Remove old symbolic links
rm -f latest_kpi_report.txt latest_kpi_data.json
rm -f latest_progress_report.txt latest_progress_data.json
rm -f latest_comprehensive_report.txt

# Create new symbolic links
ln -sf "$(basename "$KPI_REPORT")" latest_kpi_report.txt
ln -sf "$(basename "$KPI_DATA")" latest_kpi_data.json
ln -sf "$(basename "$PROGRESS_REPORT")" latest_progress_report.txt
ln -sf "$(basename "$PROGRESS_DATA")" latest_progress_data.json

# Find the latest comprehensive report
LATEST_COMPREHENSIVE=$(ls -t comprehensive_report_*.txt 2>/dev/null | head -1)
if [ -n "$LATEST_COMPREHENSIVE" ]; then
    ln -sf "$LATEST_COMPREHENSIVE" latest_comprehensive_report.txt
fi

echo "Symbolic links created:"
echo "  latest_kpi_report.txt -> $(basename "$KPI_REPORT")"
echo "  latest_kpi_data.json -> $(basename "$KPI_DATA")"
echo "  latest_progress_report.txt -> $(basename "$PROGRESS_REPORT")"
echo "  latest_progress_data.json -> $(basename "$PROGRESS_DATA")"

cd "$BASE_DIR"

echo
echo "Step 7: Cleaning up old reports..."
echo "==================================="

# Clean up old reports (keep last 10 of each type)
cd analysis/reports

# Function to clean up old files
cleanup_old_files() {
    local pattern=$1
    local keep_count=$2
    
    if ls ${pattern}_*.txt > /dev/null 2>&1; then
        # Get list of files sorted by date (newest first)
        local files=($(ls -t ${pattern}_*.txt))
        
        # If we have more than keep_count files, remove the oldest ones
        if [ ${#files[@]} -gt $keep_count ]; then
            local files_to_remove=("${files[@]:$keep_count}")
            for file in "${files_to_remove[@]}"; do
                echo "Removing old report: $file"
                rm -f "$file"
                # Also remove corresponding JSON file if it exists
                json_file="${file%.txt}.json"
                if [ -f "$json_file" ]; then
                    rm -f "$json_file"
                fi
            done
        fi
    fi
}

# Clean up different report types
cleanup_old_files "kpi_report" 10
cleanup_old_files "nsSelection_progress" 10
cleanup_old_files "comprehensive_report" 10
cleanup_old_files "codebase_analysis" 5
cleanup_old_files "complexity_analysis" 5

cd "$BASE_DIR"

echo
echo "Step 8: Generating summary..."
echo "============================"

# Generate a quick summary
echo "KPI Update Summary:"
echo "==================="
echo "Timestamp: $TIMESTAMP"
echo "Reports generated:"
echo "  - Codebase analysis: analysis/reports/codebase_analysis_${TIMESTAMP}.txt"
echo "  - Complexity analysis: analysis/reports/complexity_analysis_${TIMESTAMP}.txt"
echo "  - KPI report: $KPI_REPORT"
echo "  - Progress report: $PROGRESS_REPORT"
echo "  - Comprehensive report: Available in analysis/reports/"
echo ""
echo "Latest reports accessible via symbolic links in analysis/reports/"
echo ""

# Display quick stats if data files exist
if [ -f "$KPI_DATA" ]; then
    echo "Quick KPI Summary:"
    echo "=================="
    python3 -c "
import json
try:
    with open('$KPI_DATA', 'r') as f:
        data = json.load(f)
    summary = data.get('kpis', {}).get('summary', {})
    print(f\"Files analyzed: {summary.get('files_analyzed', 0)}\")
    print(f\"Total functions: {summary.get('total_functions', 0)}\")
    print(f\"Modernization score: {summary.get('modernization_score', 0):.1f}%\")
    print(f\"Code quality score: {summary.get('code_quality_score', 0):.1f}%\")
    print(f\"Documentation score: {summary.get('documentation_score', 0):.1f}%\")
except Exception as e:
    print(f\"Error reading KPI data: {e}\")
"
fi

if [ -f "$PROGRESS_DATA" ]; then
    echo ""
    echo "Quick Progress Summary:"
    echo "======================="
    python3 -c "
import json
try:
    with open('$PROGRESS_DATA', 'r') as f:
        data = json.load(f)
    summary = data.get('summary', {})
    print(f\"Methods modernized: {summary.get('methods_modernized', 0)}/{summary.get('total_methods_tracked', 0)}\")
    print(f\"Modernization rate: {summary.get('modernization_rate', 0):.1f}%\")
    print(f\"Average method score: {summary.get('average_modernization_score', 0):.1f}%\")
except Exception as e:
    print(f\"Error reading progress data: {e}\")
"
fi

echo
echo "=============================================="
echo "KPI Update completed at: $(date)"
echo "=============================================="

# Exit with success
exit 0