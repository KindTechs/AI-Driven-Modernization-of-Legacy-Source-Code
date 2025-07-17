#!/bin/bash

# AI-Driven Mozilla 1.0 Codebase Modernization - Initial Analysis Script
# This script indexes the Mozilla 1.0 codebase and gathers comprehensive statistics

echo "===== Mozilla 1.0 Codebase Analysis Report ====="
echo "Generated on: $(date)"
echo "========================================"

# Set base directory
BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"

# Create analysis output directory
mkdir -p analysis/reports

# Function to count files by extension
count_by_extension() {
    local ext="$1"
    local desc="$2"
    local count=$(find . -name "*.$ext" -type f | wc -l)
    echo "$desc files: $count"
}

echo
echo "File Type Distribution:"
echo "======================"
count_by_extension "cpp" "C++ Source"
count_by_extension "h" "Header"
count_by_extension "c" "C Source"
count_by_extension "idl" "Interface Definition Language"
count_by_extension "js" "JavaScript"
count_by_extension "mk" "Makefile"
count_by_extension "mak" "Makefile"
count_by_extension "py" "Python"
count_by_extension "pl" "Perl"
count_by_extension "sh" "Shell Script"

echo
echo "Total Files: $(find . -type f | wc -l)"
echo

# Directory structure analysis
echo "Directory Structure (Top Level):"
echo "================================"
find . -maxdepth 1 -type d -name "*" | sort | head -20

echo
echo "Core Module Directories:"
echo "======================="
for dir in js layout content netwerk xpcom dom widget gfx htmlparser editor; do
    if [ -d "$dir" ]; then
        file_count=$(find "$dir" -type f | wc -l)
        echo "$dir: $file_count files"
    fi
done

echo
echo "Lines of Code Analysis:"
echo "======================"

# Function to count lines in files of specific type
count_lines() {
    local pattern="$1"
    local desc="$2"
    local files=$(find . -name "$pattern" -type f)
    if [ -n "$files" ]; then
        local total_lines=$(echo "$files" | xargs wc -l | tail -1 | awk '{print $1}')
        local file_count=$(echo "$files" | wc -l)
        echo "$desc: $total_lines lines in $file_count files"
    else
        echo "$desc: 0 lines (no files found)"
    fi
}

count_lines "*.cpp" "C++ Source"
count_lines "*.h" "Header"
count_lines "*.c" "C Source"
count_lines "*.idl" "IDL"
count_lines "*.js" "JavaScript"

echo
echo "Largest Files (Potential Complexity Hotspots):"
echo "============================================="
find . -name "*.cpp" -o -name "*.h" -o -name "*.c" | xargs wc -l | sort -nr | head -20

echo
echo "Comment Density Analysis:"
echo "========================"
# Simple comment analysis for C/C++ files
cpp_files=$(find . -name "*.cpp" -o -name "*.h" -o -name "*.c" | head -10)
if [ -n "$cpp_files" ]; then
    echo "Sample analysis of first 10 C/C++ files:"
    for file in $cpp_files; do
        total_lines=$(wc -l < "$file")
        comment_lines=$(grep -c "^\s*//\|^\s*/\*\|\*/" "$file" 2>/dev/null || echo 0)
        if [ "$total_lines" -gt 0 ]; then
            ratio=$(echo "scale=2; $comment_lines * 100 / $total_lines" | bc -l 2>/dev/null || echo "0")
            echo "  $file: $comment_lines/$total_lines lines (${ratio}%)"
        fi
    done
fi

echo
echo "Memory Management Pattern Analysis:"
echo "=================================="
echo "malloc/free patterns:"
grep -r "malloc\|free" --include="*.cpp" --include="*.c" . | wc -l

echo "new/delete patterns:"
grep -r "new \|delete " --include="*.cpp" --include="*.c" . | wc -l

echo "AddRef/Release patterns:"
grep -r "AddRef\|Release" --include="*.cpp" --include="*.h" . | wc -l

echo
echo "Error Handling Pattern Analysis:"
echo "==============================="
echo "NS_SUCCEEDED/NS_FAILED patterns:"
grep -r "NS_SUCCEEDED\|NS_FAILED" --include="*.cpp" --include="*.h" . | wc -l

echo "Return error codes (NS_ERROR_*):"
grep -r "NS_ERROR_" --include="*.cpp" --include="*.h" . | wc -l

echo
echo "Cast Pattern Analysis:"
echo "====================="
echo "C-style casts:"
grep -r "([A-Za-z_][A-Za-z0-9_]*\*)" --include="*.cpp" --include="*.h" . | wc -l

echo "static_cast usage:"
grep -r "static_cast" --include="*.cpp" --include="*.h" . | wc -l

echo "reinterpret_cast usage:"
grep -r "reinterpret_cast" --include="*.cpp" --include="*.h" . | wc -l

echo
echo "Module-Level Statistics:"
echo "======================="
for module in js layout content netwerk xpcom dom widget gfx; do
    if [ -d "$module" ]; then
        echo "=== $module ==="
        echo "  Total files: $(find "$module" -type f | wc -l)"
        echo "  C++ files: $(find "$module" -name "*.cpp" | wc -l)"
        echo "  Header files: $(find "$module" -name "*.h" | wc -l)"
        echo "  Total lines: $(find "$module" -name "*.cpp" -o -name "*.h" -o -name "*.c" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)"
        echo
    fi
done

echo
echo "===== End of Analysis Report ====="
echo "Report saved to: analysis/reports/codebase_analysis_$(date +%Y%m%d_%H%M%S).txt"