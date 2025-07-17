#!/usr/bin/env python3
"""
AI-Driven Mozilla 1.0 Codebase Modernization - nsSelection Progress Tracking Script
This script tracks which methods in nsSelection.cpp have been modernized and calculates progress.
"""

import os
import re
import json
from pathlib import Path
import argparse
from datetime import datetime
from collections import defaultdict

class nsSelectionProgressTracker:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.progress_data = {
            'methods': {},
            'patterns': {},
            'summary': {},
            'history': []
        }
        
        # Define methods to track for nsSelection
        self.tracked_methods = [
            'GetRangeAt',
            'GetRangeCount',
            'GetIsCollapsed',
            'GetAnchorNode',
            'GetAnchorOffset',
            'GetFocusNode',
            'GetFocusOffset',
            'Collapse',
            'Extend',
            'CollapseToStart',
            'CollapseToEnd',
            'SelectAllChildren',
            'AddRange',
            'RemoveRange',
            'RemoveAllRanges',
            'DeleteFromDocument',
            'ToString',
            'ContainsNode',
            'GetPresShell',
            'GetPresContext',
            'AddItem',
            'RemoveItem',
            'Clear',
            'GetStartContainer',
            'GetStartOffset',
            'GetEndContainer',
            'GetEndOffset'
        ]
        
        # Define modernization patterns to track
        self.modernization_patterns = {
            'error_handling': {
                'legacy': ['NS_SUCCEEDED', 'NS_FAILED', 'NS_ERROR_'],
                'modern': ['Result<', 'Ok(', 'Err(', 'isOk()', 'isErr()']
            },
            'memory_management': {
                'legacy': ['AddRef()', 'Release()', 'new ', 'delete ', 'malloc', 'free'],
                'modern': ['nsCOMPtr<', 'RefPtr<', 'std::unique_ptr<', 'std::shared_ptr<']
            },
            'casts': {
                'legacy': [r'\([A-Za-z_][A-Za-z0-9_]*\s*\*\s*\)'],
                'modern': ['static_cast<', 'dynamic_cast<', 'const_cast<', 'reinterpret_cast<']
            },
            'out_parameters': {
                'legacy': [r'\w+\s*\*\*\s*\w+'],
                'modern': ['Result<', 'Maybe<', 'Optional<']
            },
            'null_checks': {
                'legacy': ['!= nullptr', '== nullptr', '!= nsnull', '== nsnull', '!= NULL', '== NULL'],
                'modern': ['has_value()', 'value_or()', 'value()']
            }
        }
    
    def find_nsselection_files(self):
        """Find nsSelection.cpp and related files."""
        files = []
        
        # Look for nsSelection files
        for pattern in ['**/nsSelection.cpp', '**/nsSelection.h']:
            files.extend(self.root_dir.rglob(pattern))
        
        return files
    
    def extract_methods_from_file(self, file_path):
        """Extract method implementations from a C++ file."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            methods = {}
            
            # Find method implementations
            for method_name in self.tracked_methods:
                # Look for method definitions
                patterns = [
                    rf'NS_IMETHODIMP\s+nsSelection::{method_name}\s*\([^)]*\)\s*{{',
                    rf'nsresult\s+nsSelection::{method_name}\s*\([^)]*\)\s*{{',
                    rf'Result<[^>]+>\s+nsSelection::{method_name}Modern\s*\([^)]*\)\s*{{',
                    rf'auto\s+nsSelection::{method_name}Modern\s*\([^)]*\)\s*{{',
                    rf'[A-Za-z_][A-Za-z0-9_<>]*\s+nsSelection::{method_name}\s*\([^)]*\)\s*{{'
                ]
                
                method_info = {
                    'found': False,
                    'modern_version': False,
                    'legacy_version': False,
                    'patterns': {
                        'error_handling': {'legacy': 0, 'modern': 0},
                        'memory_management': {'legacy': 0, 'modern': 0},
                        'casts': {'legacy': 0, 'modern': 0},
                        'out_parameters': {'legacy': 0, 'modern': 0},
                        'null_checks': {'legacy': 0, 'modern': 0}
                    }
                }
                
                for pattern in patterns:
                    matches = re.finditer(pattern, content, re.IGNORECASE)
                    for match in matches:
                        method_info['found'] = True
                        
                        # Check if this is a modern version
                        if 'Modern' in match.group() or 'Result<' in match.group():
                            method_info['modern_version'] = True
                        else:
                            method_info['legacy_version'] = True
                        
                        # Extract method body
                        method_start = match.start()
                        method_body = self.extract_method_body(content, method_start)
                        
                        # Count patterns in method body
                        method_patterns = self.count_patterns_in_text(method_body)
                        
                        # Accumulate pattern counts
                        for pattern_type, counts in method_patterns.items():
                            method_info['patterns'][pattern_type]['legacy'] += counts['legacy']
                            method_info['patterns'][pattern_type]['modern'] += counts['modern']
                
                methods[method_name] = method_info
            
            return methods
            
        except Exception as e:
            print(f"Error extracting methods from {file_path}: {e}")
            return {}
    
    def extract_method_body(self, content, start_pos):
        """Extract the body of a method starting at start_pos."""
        # Find the opening brace
        brace_pos = content.find('{', start_pos)
        if brace_pos == -1:
            return ""
        
        # Count braces to find the end
        brace_count = 0
        end_pos = brace_pos
        
        for i, char in enumerate(content[brace_pos:], brace_pos):
            if char == '{':
                brace_count += 1
            elif char == '}':
                brace_count -= 1
                if brace_count == 0:
                    end_pos = i
                    break
        
        return content[brace_pos:end_pos + 1]
    
    def count_patterns_in_text(self, text):
        """Count modernization patterns in a text."""
        pattern_counts = {}
        
        for pattern_type, pattern_info in self.modernization_patterns.items():
            counts = {'legacy': 0, 'modern': 0}
            
            # Count legacy patterns
            for legacy_pattern in pattern_info['legacy']:
                if legacy_pattern.startswith(r'\\'):
                    # Regex pattern
                    matches = re.findall(legacy_pattern, text)
                    counts['legacy'] += len(matches)
                else:
                    # Simple string pattern
                    counts['legacy'] += text.count(legacy_pattern)
            
            # Count modern patterns
            for modern_pattern in pattern_info['modern']:
                if modern_pattern.startswith(r'\\'):
                    # Regex pattern
                    matches = re.findall(modern_pattern, text)
                    counts['modern'] += len(matches)
                else:
                    # Simple string pattern
                    counts['modern'] += text.count(modern_pattern)
            
            pattern_counts[pattern_type] = counts
        
        return pattern_counts
    
    def calculate_modernization_score(self, method_info):
        """Calculate a modernization score for a method."""
        if not method_info['found']:
            return 0
        
        score = 0
        total_weight = 0
        
        # Weight factors for different aspects
        weights = {
            'has_modern_version': 40,
            'error_handling': 20,
            'memory_management': 20,
            'casts': 10,
            'out_parameters': 5,
            'null_checks': 5
        }
        
        # Score for having modern version
        if method_info['modern_version']:
            score += weights['has_modern_version']
        total_weight += weights['has_modern_version']
        
        # Score for modernization patterns
        for pattern_type, pattern_data in method_info['patterns'].items():
            if pattern_type in weights:
                legacy_count = pattern_data['legacy']
                modern_count = pattern_data['modern']
                total_patterns = legacy_count + modern_count
                
                if total_patterns > 0:
                    pattern_score = (modern_count / total_patterns) * weights[pattern_type]
                    score += pattern_score
                
                total_weight += weights[pattern_type]
        
        return (score / total_weight * 100) if total_weight > 0 else 0
    
    def analyze_nsselection_progress(self):
        """Analyze the modernization progress of nsSelection.cpp."""
        files = self.find_nsselection_files()
        
        if not files:
            print("No nsSelection files found")
            return
        
        all_methods = {}
        
        for file_path in files:
            print(f"Analyzing {file_path}...")
            file_methods = self.extract_methods_from_file(file_path)
            
            # Merge method information
            for method_name, method_info in file_methods.items():
                if method_name not in all_methods:
                    all_methods[method_name] = method_info
                else:
                    # Merge information from multiple files
                    existing = all_methods[method_name]
                    existing['found'] = existing['found'] or method_info['found']\n                    existing['modern_version'] = existing['modern_version'] or method_info['modern_version']\n                    existing['legacy_version'] = existing['legacy_version'] or method_info['legacy_version']\n                    \n                    # Merge pattern counts\n                    for pattern_type in existing['patterns']:\n                        existing['patterns'][pattern_type]['legacy'] += method_info['patterns'][pattern_type]['legacy']\n                        existing['patterns'][pattern_type]['modern'] += method_info['patterns'][pattern_type]['modern']\n        \n        # Calculate scores and progress\n        method_scores = {}\n        total_score = 0\n        methods_found = 0\n        \n        for method_name, method_info in all_methods.items():\n            if method_info['found']:\n                score = self.calculate_modernization_score(method_info)\n                method_scores[method_name] = score\n                total_score += score\n                methods_found += 1\n        \n        average_score = total_score / methods_found if methods_found > 0 else 0\n        \n        # Count modernized methods\n        modernized_methods = sum(1 for info in all_methods.values() if info['modern_version'])\n        modernization_rate = (modernized_methods / len(self.tracked_methods) * 100) if self.tracked_methods else 0\n        \n        # Aggregate pattern counts\n        total_patterns = defaultdict(lambda: {'legacy': 0, 'modern': 0})\n        for method_info in all_methods.values():\n            if method_info['found']:\n                for pattern_type, pattern_data in method_info['patterns'].items():\n                    total_patterns[pattern_type]['legacy'] += pattern_data['legacy']\n                    total_patterns[pattern_type]['modern'] += pattern_data['modern']\n        \n        # Store results\n        self.progress_data = {\n            'methods': all_methods,\n            'method_scores': method_scores,\n            'patterns': dict(total_patterns),\n            'summary': {\n                'total_methods_tracked': len(self.tracked_methods),\n                'methods_found': methods_found,\n                'methods_modernized': modernized_methods,\n                'modernization_rate': modernization_rate,\n                'average_modernization_score': average_score,\n                'analysis_date': datetime.now().isoformat()\n            }\n        }\n    \n    def generate_progress_report(self, output_file):\n        \"\"\"Generate a progress report for nsSelection modernization.\"\"\"\n        if not self.progress_data:\n            self.analyze_nsselection_progress()\n        \n        report = []\n        report.append(\"=\" * 80)\n        report.append(\"nsSelection Modernization Progress Report\")\n        report.append(f\"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\")\n        report.append(\"=\" * 80)\n        \n        # Summary\n        summary = self.progress_data['summary']\n        report.append(f\"\\nSUMMARY:\")\n        report.append(f\"Total methods tracked: {summary['total_methods_tracked']}\")\n        report.append(f\"Methods found in codebase: {summary['methods_found']}\")\n        report.append(f\"Methods modernized: {summary['methods_modernized']}\")\n        report.append(f\"Modernization rate: {summary['modernization_rate']:.1f}%\")\n        report.append(f\"Average modernization score: {summary['average_modernization_score']:.1f}%\")\n        \n        # Method-by-method progress\n        report.append(f\"\\nMETHOD MODERNIZATION STATUS:\")\n        report.append(f\"{'Method':<25} {'Found':<8} {'Modern':<8} {'Legacy':<8} {'Score':<8}\")\n        report.append(\"-\" * 65)\n        \n        methods = self.progress_data['methods']\n        scores = self.progress_data['method_scores']\n        \n        for method_name in self.tracked_methods:\n            info = methods.get(method_name, {'found': False, 'modern_version': False, 'legacy_version': False})\n            score = scores.get(method_name, 0)\n            \n            found = \"Yes\" if info['found'] else \"No\"\n            modern = \"Yes\" if info['modern_version'] else \"No\"\n            legacy = \"Yes\" if info['legacy_version'] else \"No\"\n            \n            report.append(f\"{method_name:<25} {found:<8} {modern:<8} {legacy:<8} {score:<8.1f}\")\n        \n        # Pattern analysis\n        report.append(f\"\\nPATTERN MODERNIZATION ANALYSIS:\")\n        patterns = self.progress_data['patterns']\n        \n        for pattern_type, pattern_data in patterns.items():\n            legacy_count = pattern_data['legacy']\n            modern_count = pattern_data['modern']\n            total = legacy_count + modern_count\n            \n            if total > 0:\n                modern_pct = (modern_count / total * 100)\n                report.append(f\"{pattern_type}:\")\n                report.append(f\"  Legacy patterns: {legacy_count}\")\n                report.append(f\"  Modern patterns: {modern_count}\")\n                report.append(f\"  Modernization: {modern_pct:.1f}%\")\n                report.append(\"\")\n        \n        # Top priorities for modernization\n        report.append(f\"\\nTOP PRIORITIES FOR MODERNIZATION:\")\n        method_priorities = [(name, 100 - scores.get(name, 0)) for name in self.tracked_methods \n                           if methods.get(name, {}).get('found', False)]\n        method_priorities.sort(key=lambda x: x[1], reverse=True)\n        \n        for i, (method_name, priority_score) in enumerate(method_priorities[:10], 1):\n            report.append(f\"{i:2d}. {method_name}: {priority_score:.1f} points needed\")\n        \n        # Recommendations\n        report.append(f\"\\nRECOMMENDATIONS:\")\n        if summary['modernization_rate'] < 25:\n            report.append(\"- Focus on creating modern versions of core methods\")\n        if summary['modernization_rate'] < 50:\n            report.append(\"- Prioritize error handling modernization (Result<T> types)\")\n        if summary['modernization_rate'] < 75:\n            report.append(\"- Convert manual memory management to smart pointers\")\n        report.append(\"- Update out parameters to return values\")\n        report.append(\"- Replace C-style casts with safe casts\")\n        \n        report.append(\"\\n\" + \"=\" * 80)\n        \n        # Write report\n        with open(output_file, 'w') as f:\n            f.write('\\n'.join(report))\n        \n        print(f\"Progress report generated: {output_file}\")\n        \n        # Save detailed JSON data\n        json_file = output_file.replace('.txt', '.json')\n        with open(json_file, 'w') as f:\n            json.dump(self.progress_data, f, indent=2)\n        \n        print(f\"Detailed progress data saved: {json_file}\")\n    \n    def save_baseline(self, baseline_file):\n        \"\"\"Save current state as baseline for future comparisons.\"\"\"\n        if not self.progress_data:\n            self.analyze_nsselection_progress()\n        \n        baseline_data = {\n            'baseline_date': datetime.now().isoformat(),\n            'summary': self.progress_data['summary'],\n            'method_scores': self.progress_data['method_scores'],\n            'patterns': self.progress_data['patterns']\n        }\n        \n        with open(baseline_file, 'w') as f:\n            json.dump(baseline_data, f, indent=2)\n        \n        print(f\"Baseline saved: {baseline_file}\")\n    \n    def compare_with_baseline(self, baseline_file):\n        \"\"\"Compare current progress with baseline.\"\"\"\n        if not self.progress_data:\n            self.analyze_nsselection_progress()\n        \n        try:\n            with open(baseline_file, 'r') as f:\n                baseline = json.load(f)\n        except FileNotFoundError:\n            print(f\"Baseline file not found: {baseline_file}\")\n            return\n        \n        current = self.progress_data['summary']\n        baseline_summary = baseline['summary']\n        \n        comparison = {\n            'methods_modernized': {\n                'current': current['methods_modernized'],\n                'baseline': baseline_summary['methods_modernized'],\n                'change': current['methods_modernized'] - baseline_summary['methods_modernized']\n            },\n            'modernization_rate': {\n                'current': current['modernization_rate'],\n                'baseline': baseline_summary['modernization_rate'],\n                'change': current['modernization_rate'] - baseline_summary['modernization_rate']\n            },\n            'average_score': {\n                'current': current['average_modernization_score'],\n                'baseline': baseline_summary['average_modernization_score'],\n                'change': current['average_modernization_score'] - baseline_summary['average_modernization_score']\n            }\n        }\n        \n        print(\"\\nCOMPARISON WITH BASELINE:\")\n        print(f\"Methods modernized: {comparison['methods_modernized']['current']} (was {comparison['methods_modernized']['baseline']}, change: {comparison['methods_modernized']['change']:+d})\")\n        print(f\"Modernization rate: {comparison['modernization_rate']['current']:.1f}% (was {comparison['modernization_rate']['baseline']:.1f}%, change: {comparison['modernization_rate']['change']:+.1f}%)\")\n        print(f\"Average score: {comparison['average_score']['current']:.1f}% (was {comparison['average_score']['baseline']:.1f}%, change: {comparison['average_score']['change']:+.1f}%)\")\n        \n        return comparison\n\ndef main():\n    parser = argparse.ArgumentParser(description='Track nsSelection modernization progress')\n    parser.add_argument('--root', default='.', help='Root directory of the codebase')\n    parser.add_argument('--output', default='analysis/reports/nsSelection_progress.txt', help='Output file for progress report')\n    parser.add_argument('--baseline', help='Baseline file for comparison')\n    parser.add_argument('--save-baseline', help='Save current state as baseline')\n    \n    args = parser.parse_args()\n    \n    # Ensure output directory exists\n    os.makedirs(os.path.dirname(args.output), exist_ok=True)\n    \n    tracker = nsSelectionProgressTracker(args.root)\n    tracker.analyze_nsselection_progress()\n    \n    if args.save_baseline:\n        tracker.save_baseline(args.save_baseline)\n    \n    if args.baseline:\n        tracker.compare_with_baseline(args.baseline)\n    \n    tracker.generate_progress_report(args.output)\n\nif __name__ == '__main__':\n    main()"