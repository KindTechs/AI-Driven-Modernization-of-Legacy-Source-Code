#!/usr/bin/env python3
"""
Track nsSelection modernization progress.
"""

import os
import re
import json
import argparse
from datetime import datetime
from collections import defaultdict


class nsSelectionProgressTracker:
    def __init__(self, root_dir):
        self.root_dir = root_dir
        self.progress_data = None
        
        # Methods to track for modernization
        self.tracked_methods = [
            'GetRangeAt',
            'GetRangeCount',
            'AddRange',
            'RemoveRange',
            'GetIsCollapsed',
            'GetPresContext',
            'AddItem',
            'RemoveItem',
            'Clear',
            'GetAnchorNode',
            'GetFocusNode',
            'Collapse',
            'Extend',
            'SelectAllChildren'
        ]
        
        # Patterns to look for
        self.modernization_patterns = {
            'error_handling': {
                'legacy': ['NS_SUCCEEDED', 'NS_FAILED', 'NS_ERROR_', 'nsresult'],
                'modern': ['Result<', 'Ok(', 'Err(', 'mozilla::Result']
            },
            'memory_management': {
                'legacy': ['malloc', 'free', 'new ', 'delete ', 'AddRef', 'Release'],
                'modern': ['nsCOMPtr', 'RefPtr', 'UniquePtr', 'std::unique_ptr', 'std::shared_ptr']
            },
            'casts': {
                'legacy': ['(PRInt32)', '(PRUint32)', '(PRBool)', '(char*)', '(void*)'],
                'modern': ['static_cast<', 'dynamic_cast<', 'const_cast<', 'reinterpret_cast<']
            },
            'out_parameters': {
                'legacy': ['**', '&aReturn', '&aCount', '&aOffset'],
                'modern': ['Result<', 'Maybe<', 'Optional<', 'std::optional']
            },
            'null_checks': {
                'legacy': ['!aPtr', '== nsnull', '!= nsnull', '== nullptr'],
                'modern': ['Maybe<', 'Optional<', 'std::optional']
            }
        }
    
    def find_nsselection_files(self):
        """Find all files related to nsSelection."""
        files = []
        patterns = ['*nsSelection*.cpp', '*nsSelection*.h', '*selection*.cpp', '*selection*.h']
        
        for root, dirs, filenames in os.walk(self.root_dir):
            for filename in filenames:
                if any(pattern.replace('*', '') in filename.lower() for pattern in patterns):
                    files.append(os.path.join(root, filename))
        
        # Also check for specific modernized files in root
        modernized_files = [
            'nsSelection_modernized.cpp',
            'nsSelection_modernized.h', 
            'nsSelection_additional_methods.cpp',
            'test_nsSelection_modernized.cpp',
            'test_additional_methods.cpp'
        ]
        
        for filename in modernized_files:
            filepath = os.path.join(self.root_dir, filename)
            if os.path.exists(filepath):
                files.append(filepath)
        
        return files
    
    def extract_methods_from_file(self, file_path):
        """Extract method information from a file."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading {file_path}: {e}")
            return {}
        
        methods = {}
        
        for method_name in self.tracked_methods:
            method_info = {
                'found': False,
                'modern_version': False,
                'legacy_version': False,
                'patterns': defaultdict(lambda: {'legacy': 0, 'modern': 0})
            }
            
            # Look for method signatures
            modern_pattern = f"{method_name}Modern"
            legacy_pattern = f"NS_IMETHODIMP.*{method_name}"
            
            if re.search(modern_pattern, content):
                method_info['found'] = True
                method_info['modern_version'] = True
            
            if re.search(legacy_pattern, content):
                method_info['found'] = True
                method_info['legacy_version'] = True
            
            # Extract method body if found
            if method_info['found']:
                method_body = self.extract_method_body(content, method_name)
                if method_body:
                    patterns = self.count_patterns_in_text(method_body)
                    for pattern_type, pattern_data in patterns.items():
                        method_info['patterns'][pattern_type] = pattern_data
            
            methods[method_name] = method_info
        
        return methods
    
    def extract_method_body(self, content, method_name):
        """Extract the body of a method from content."""
        # This is a simplified implementation
        # In practice, you'd want more sophisticated parsing
        patterns = [
            f"Result<.*>{method_name}Modern",
            f"NS_IMETHODIMP.*{method_name}",
            f"nsresult.*{method_name}"
        ]
        
        for pattern in patterns:
            match = re.search(pattern, content)
            if match:
                start = match.start()
                # Find the opening brace
                brace_pos = content.find('{', start)
                if brace_pos == -1:
                    continue
                
                # Find the matching closing brace
                brace_count = 1
                end_pos = brace_pos + 1
                for i in range(brace_pos + 1, len(content)):
                    if content[i] == '{':
                        brace_count += 1
                    elif content[i] == '}':
                        brace_count -= 1
                        if brace_count == 0:
                            end_pos = i
                            break
                
                return content[brace_pos:end_pos + 1]
        
        return ""
    
    def count_patterns_in_text(self, text):
        """Count modernization patterns in a text."""
        pattern_counts = {}
        
        for pattern_type, pattern_info in self.modernization_patterns.items():
            counts = {'legacy': 0, 'modern': 0}
            
            # Count legacy patterns
            for legacy_pattern in pattern_info['legacy']:
                counts['legacy'] += text.count(legacy_pattern)
            
            # Count modern patterns
            for modern_pattern in pattern_info['modern']:
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
                    existing['found'] = existing['found'] or method_info['found']
                    existing['modern_version'] = existing['modern_version'] or method_info['modern_version']
                    existing['legacy_version'] = existing['legacy_version'] or method_info['legacy_version']
                    
                    # Merge pattern counts
                    for pattern_type in existing['patterns']:
                        existing['patterns'][pattern_type]['legacy'] += method_info['patterns'][pattern_type]['legacy']
                        existing['patterns'][pattern_type]['modern'] += method_info['patterns'][pattern_type]['modern']
        
        # Calculate scores and progress
        method_scores = {}
        total_score = 0
        methods_found = 0
        
        for method_name, method_info in all_methods.items():
            if method_info['found']:
                score = self.calculate_modernization_score(method_info)
                method_scores[method_name] = score
                total_score += score
                methods_found += 1
        
        average_score = total_score / methods_found if methods_found > 0 else 0
        
        # Count modernized methods
        modernized_methods = sum(1 for info in all_methods.values() if info['modern_version'])
        modernization_rate = (modernized_methods / len(self.tracked_methods) * 100) if self.tracked_methods else 0
        
        # Aggregate pattern counts
        total_patterns = defaultdict(lambda: {'legacy': 0, 'modern': 0})
        for method_info in all_methods.values():
            if method_info['found']:
                for pattern_type, pattern_data in method_info['patterns'].items():
                    total_patterns[pattern_type]['legacy'] += pattern_data['legacy']
                    total_patterns[pattern_type]['modern'] += pattern_data['modern']
        
        # Store results
        self.progress_data = {
            'methods': all_methods,
            'method_scores': method_scores,
            'patterns': dict(total_patterns),
            'summary': {
                'total_methods_tracked': len(self.tracked_methods),
                'methods_found': methods_found,
                'methods_modernized': modernized_methods,
                'modernization_rate': modernization_rate,
                'average_modernization_score': average_score,
                'analysis_date': datetime.now().isoformat()
            }
        }
    
    def generate_progress_report(self, output_file):
        """Generate a progress report for nsSelection modernization."""
        if not self.progress_data:
            self.analyze_nsselection_progress()
        
        report = []
        report.append("=" * 80)
        report.append("nsSelection Modernization Progress Report")
        report.append(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("=" * 80)
        
        # Summary
        summary = self.progress_data['summary']
        report.append(f"\nSUMMARY:")
        report.append(f"Total methods tracked: {summary['total_methods_tracked']}")
        report.append(f"Methods found in codebase: {summary['methods_found']}")
        report.append(f"Methods modernized: {summary['methods_modernized']}")
        report.append(f"Modernization rate: {summary['modernization_rate']:.1f}%")
        report.append(f"Average modernization score: {summary['average_modernization_score']:.1f}%")
        
        # Method-by-method progress
        report.append(f"\nMETHOD MODERNIZATION STATUS:")
        report.append(f"{'Method':<25} {'Found':<8} {'Modern':<8} {'Legacy':<8} {'Score':<8}")
        report.append("-" * 65)
        
        methods = self.progress_data['methods']
        scores = self.progress_data['method_scores']
        
        for method_name in self.tracked_methods:
            info = methods.get(method_name, {'found': False, 'modern_version': False, 'legacy_version': False})
            score = scores.get(method_name, 0)
            
            found = "Yes" if info['found'] else "No"
            modern = "Yes" if info['modern_version'] else "No"
            legacy = "Yes" if info['legacy_version'] else "No"
            
            report.append(f"{method_name:<25} {found:<8} {modern:<8} {legacy:<8} {score:<8.1f}")
        
        # Pattern analysis
        report.append(f"\nPATTERN MODERNIZATION ANALYSIS:")
        patterns = self.progress_data['patterns']
        
        for pattern_type, pattern_data in patterns.items():
            legacy_count = pattern_data['legacy']
            modern_count = pattern_data['modern']
            total = legacy_count + modern_count
            
            if total > 0:
                modern_pct = (modern_count / total * 100)
                report.append(f"{pattern_type}:")
                report.append(f"  Legacy patterns: {legacy_count}")
                report.append(f"  Modern patterns: {modern_count}")
                report.append(f"  Modernization: {modern_pct:.1f}%")
                report.append("")
        
        # Top priorities for modernization
        report.append(f"\nTOP PRIORITIES FOR MODERNIZATION:")
        method_priorities = [(name, 100 - scores.get(name, 0)) for name in self.tracked_methods 
                           if methods.get(name, {}).get('found', False)]
        method_priorities.sort(key=lambda x: x[1], reverse=True)
        
        for i, (method_name, priority_score) in enumerate(method_priorities[:10], 1):
            report.append(f"{i:2d}. {method_name}: {priority_score:.1f} points needed")
        
        # Recommendations
        report.append(f"\nRECOMMENDATIONS:")
        if summary['modernization_rate'] < 25:
            report.append("- Focus on creating modern versions of core methods")
        if summary['modernization_rate'] < 50:
            report.append("- Prioritize error handling modernization (Result<T> types)")
        if summary['modernization_rate'] < 75:
            report.append("- Convert manual memory management to smart pointers")
        report.append("- Update out parameters to return values")
        report.append("- Replace C-style casts with safe casts")
        
        report.append("\n" + "=" * 80)
        
        # Write report
        with open(output_file, 'w') as f:
            f.write('\n'.join(report))
        
        print(f"Progress report generated: {output_file}")
        
        # Save detailed JSON data
        json_file = output_file.replace('.txt', '.json')
        with open(json_file, 'w') as f:
            json.dump(self.progress_data, f, indent=2)
        
        print(f"Detailed progress data saved: {json_file}")


def main():
    parser = argparse.ArgumentParser(description='Track nsSelection modernization progress')
    parser.add_argument('--root', default='.', help='Root directory of the codebase')
    parser.add_argument('--output', default='analysis/reports/nsSelection_progress.txt', help='Output file for progress report')
    
    args = parser.parse_args()
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    
    tracker = nsSelectionProgressTracker(args.root)
    tracker.analyze_nsselection_progress()
    tracker.generate_progress_report(args.output)


if __name__ == '__main__':
    main()
