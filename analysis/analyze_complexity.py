#!/usr/bin/env python3
"""
AI-Driven Mozilla 1.0 Codebase Modernization - Complexity Analysis Script
This script analyzes the Mozilla 1.0 codebase in detail to identify modernization opportunities.
"""

import os
import re
import json
from collections import defaultdict
from pathlib import Path
import argparse
from datetime import datetime

class CodebaseAnalyzer:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.results = {
            'memory_management': defaultdict(list),
            'error_handling': defaultdict(list),
            'cast_patterns': defaultdict(list),
            'reference_counting': defaultdict(list),
            'out_parameters': defaultdict(list),
            'null_checks': defaultdict(list),
            'summary': {}
        }
        
    def analyze_file(self, file_path):
        """Analyze a single source file for modernization patterns."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                lines = content.split('\n')
                
            relative_path = str(file_path.relative_to(self.root_dir))
            
            # Analyze different patterns
            self._analyze_memory_management(relative_path, content, lines)
            self._analyze_error_handling(relative_path, content, lines)
            self._analyze_cast_patterns(relative_path, content, lines)
            self._analyze_reference_counting(relative_path, content, lines)
            self._analyze_out_parameters(relative_path, content, lines)
            self._analyze_null_checks(relative_path, content, lines)
            
        except Exception as e:
            print(f"Error analyzing {file_path}: {e}")
    
    def _analyze_memory_management(self, file_path, content, lines):
        """Detect memory management patterns."""
        # malloc/free patterns
        malloc_matches = re.finditer(r'\bmalloc\s*\(', content)
        for match in malloc_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['memory_management']['malloc'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'malloc',
                'context': lines[line_num - 1].strip()
            })
        
        free_matches = re.finditer(r'\bfree\s*\(', content)
        for match in free_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['memory_management']['free'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'free',
                'context': lines[line_num - 1].strip()
            })
        
        # new/delete patterns
        new_matches = re.finditer(r'\bnew\s+\w+', content)
        for match in new_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['memory_management']['new'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'new',
                'context': lines[line_num - 1].strip()
            })
        
        delete_matches = re.finditer(r'\bdelete\s+', content)
        for match in delete_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['memory_management']['delete'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'delete',
                'context': lines[line_num - 1].strip()
            })
    
    def _analyze_error_handling(self, file_path, content, lines):
        """Detect error handling patterns."""
        # NS_SUCCEEDED/NS_FAILED patterns
        ns_succeeded_matches = re.finditer(r'\bNS_SUCCEEDED\s*\(', content)
        for match in ns_succeeded_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['error_handling']['NS_SUCCEEDED'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'NS_SUCCEEDED',
                'context': lines[line_num - 1].strip()
            })
        
        ns_failed_matches = re.finditer(r'\bNS_FAILED\s*\(', content)
        for match in ns_failed_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['error_handling']['NS_FAILED'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'NS_FAILED',
                'context': lines[line_num - 1].strip()
            })
        
        # Error code returns
        error_code_matches = re.finditer(r'\bNS_ERROR_\w+', content)
        for match in error_code_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['error_handling']['error_codes'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'error_code',
                'context': lines[line_num - 1].strip(),
                'error_code': match.group()
            })
    
    def _analyze_cast_patterns(self, file_path, content, lines):
        """Detect cast patterns."""
        # C-style casts
        c_cast_matches = re.finditer(r'\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\*\s*\)', content)
        for match in c_cast_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['cast_patterns']['c_style'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'c_style_cast',
                'context': lines[line_num - 1].strip(),
                'cast': match.group()
            })
        
        # static_cast usage
        static_cast_matches = re.finditer(r'\bstatic_cast\s*<[^>]+>', content)
        for match in static_cast_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['cast_patterns']['static_cast'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'static_cast',
                'context': lines[line_num - 1].strip()
            })
        
        # reinterpret_cast usage
        reinterpret_cast_matches = re.finditer(r'\breinterpret_cast\s*<[^>]+>', content)
        for match in reinterpret_cast_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['cast_patterns']['reinterpret_cast'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'reinterpret_cast',
                'context': lines[line_num - 1].strip()
            })
    
    def _analyze_reference_counting(self, file_path, content, lines):
        """Detect reference counting patterns."""
        # AddRef patterns
        addref_matches = re.finditer(r'\bAddRef\s*\(', content)
        for match in addref_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['reference_counting']['AddRef'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'AddRef',
                'context': lines[line_num - 1].strip()
            })
        
        # Release patterns
        release_matches = re.finditer(r'\bRelease\s*\(', content)
        for match in release_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['reference_counting']['Release'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'Release',
                'context': lines[line_num - 1].strip()
            })
        
        # nsCOMPtr usage (modern smart pointer)
        nscomptr_matches = re.finditer(r'\bnsCOMPtr\s*<[^>]+>', content)
        for match in nscomptr_matches:
            line_num = content[:match.start()].count('\n') + 1
            self.results['reference_counting']['nsCOMPtr'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'nsCOMPtr',
                'context': lines[line_num - 1].strip()
            })
    
    def _analyze_out_parameters(self, file_path, content, lines):
        """Detect out parameter patterns."""
        # Function parameters with ** (out parameters)
        out_param_matches = re.finditer(r'\w+\s*\*\*\s*\w+', content)
        for match in out_param_matches:
            line_num = content[:match.start()].count('\n') + 1
            # Check if this is in a function declaration/definition
            line_context = lines[line_num - 1].strip()
            if '(' in line_context and ')' in line_context:
                self.results['out_parameters']['double_pointer'].append({
                    'file': file_path,
                    'line': line_num,
                    'pattern': 'out_parameter',
                    'context': line_context,
                    'parameter': match.group()
                })
    
    def _analyze_null_checks(self, file_path, content, lines):
        """Detect null check patterns."""
        # Null checks
        null_checks = re.finditer(r'\b\w+\s*[!=]=\s*(?:null|nullptr|nsnull|0)\b', content)
        for match in null_checks:
            line_num = content[:match.start()].count('\n') + 1
            self.results['null_checks']['basic'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'null_check',
                'context': lines[line_num - 1].strip(),
                'check': match.group()
            })
        
        # if (!pointer) patterns
        if_not_checks = re.finditer(r'\bif\s*\(\s*!\s*\w+\s*\)', content)
        for match in if_not_checks:
            line_num = content[:match.start()].count('\n') + 1
            self.results['null_checks']['if_not'].append({
                'file': file_path,
                'line': line_num,
                'pattern': 'if_not_check',
                'context': lines[line_num - 1].strip(),
                'check': match.group()
            })
    
    def analyze_codebase(self):
        """Analyze the entire codebase."""
        print("Analyzing Mozilla 1.0 codebase for modernization opportunities...")
        
        # Find all C/C++ source files
        cpp_files = list(self.root_dir.rglob('*.cpp'))
        h_files = list(self.root_dir.rglob('*.h'))
        c_files = list(self.root_dir.rglob('*.c'))
        
        all_files = cpp_files + h_files + c_files
        
        print(f"Found {len(all_files)} source files to analyze")
        
        for i, file_path in enumerate(all_files):
            if i % 100 == 0:
                print(f"Processing file {i + 1}/{len(all_files)}: {file_path.name}")
            self.analyze_file(file_path)
        
        # Generate summary
        self._generate_summary()
        
        print("Analysis complete!")
    
    def _generate_summary(self):
        """Generate summary statistics."""
        self.results['summary'] = {
            'total_files_analyzed': len(set(
                item['file'] for category in self.results.values() 
                if isinstance(category, dict) 
                for pattern_list in category.values() 
                for item in pattern_list
            )),
            'memory_management': {
                'malloc_calls': len(self.results['memory_management']['malloc']),
                'free_calls': len(self.results['memory_management']['free']),
                'new_calls': len(self.results['memory_management']['new']),
                'delete_calls': len(self.results['memory_management']['delete'])
            },
            'error_handling': {
                'NS_SUCCEEDED_calls': len(self.results['error_handling']['NS_SUCCEEDED']),
                'NS_FAILED_calls': len(self.results['error_handling']['NS_FAILED']),
                'error_codes': len(self.results['error_handling']['error_codes'])
            },
            'cast_patterns': {
                'c_style_casts': len(self.results['cast_patterns']['c_style']),
                'static_casts': len(self.results['cast_patterns']['static_cast']),
                'reinterpret_casts': len(self.results['cast_patterns']['reinterpret_cast'])
            },
            'reference_counting': {
                'AddRef_calls': len(self.results['reference_counting']['AddRef']),
                'Release_calls': len(self.results['reference_counting']['Release']),
                'nsCOMPtr_usage': len(self.results['reference_counting']['nsCOMPtr'])
            },
            'out_parameters': {
                'double_pointer_params': len(self.results['out_parameters']['double_pointer'])
            },
            'null_checks': {
                'basic_null_checks': len(self.results['null_checks']['basic']),
                'if_not_checks': len(self.results['null_checks']['if_not'])
            }
        }
    
    def generate_report(self, output_file):
        """Generate a comprehensive modernization report."""
        report = []
        report.append("=" * 80)
        report.append("Mozilla 1.0 Codebase Modernization Analysis Report")
        report.append(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("=" * 80)
        
        # Summary section
        report.append("\nSUMMARY:")
        report.append("-" * 40)
        summary = self.results['summary']
        report.append(f"Total files analyzed: {summary['total_files_analyzed']}")
        
        # Memory management patterns
        report.append("\nMEMORY MANAGEMENT MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 50)
        mm = summary['memory_management']
        report.append(f"malloc/free pairs: {mm['malloc_calls']}/{mm['free_calls']}")
        report.append(f"new/delete pairs: {mm['new_calls']}/{mm['delete_calls']}")
        report.append("Recommendation: Replace with smart pointers (std::unique_ptr, std::shared_ptr)")
        
        # Error handling patterns
        report.append("\nERROR HANDLING MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 50)
        eh = summary['error_handling']
        report.append(f"NS_SUCCEEDED calls: {eh['NS_SUCCEEDED_calls']}")
        report.append(f"NS_FAILED calls: {eh['NS_FAILED_calls']}")
        report.append(f"Error codes: {eh['error_codes']}")
        report.append("Recommendation: Replace with Result<T> or std::expected")
        
        # Cast patterns
        report.append("\nCAST MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 40)
        cp = summary['cast_patterns']
        report.append(f"C-style casts: {cp['c_style_casts']}")
        report.append(f"static_cast usage: {cp['static_casts']}")
        report.append(f"reinterpret_cast usage: {cp['reinterpret_casts']}")
        report.append("Recommendation: Replace C-style casts with safe C++ casts")
        
        # Reference counting
        report.append("\nREFERENCE COUNTING MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 55)
        rc = summary['reference_counting']
        report.append(f"AddRef calls: {rc['AddRef_calls']}")
        report.append(f"Release calls: {rc['Release_calls']}")
        report.append(f"nsCOMPtr usage: {rc['nsCOMPtr_usage']}")
        report.append("Recommendation: Replace manual ref counting with smart pointers")
        
        # Out parameters
        report.append("\nOUT PARAMETER MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 45)
        op = summary['out_parameters']
        report.append(f"Double pointer parameters: {op['double_pointer_params']}")
        report.append("Recommendation: Replace with return values or std::optional")
        
        # Null checks
        report.append("\nNULL CHECK MODERNIZATION OPPORTUNITIES:")
        report.append("-" * 42)
        nc = summary['null_checks']
        report.append(f"Basic null checks: {nc['basic_null_checks']}")
        report.append(f"If-not checks: {nc['if_not_checks']}")
        report.append("Recommendation: Replace with std::optional or smart pointers")
        
        # Top files for modernization
        report.append("\nTOP FILES FOR MODERNIZATION:")
        report.append("-" * 35)
        file_scores = defaultdict(int)
        for category in self.results.values():
            if isinstance(category, dict):
                for pattern_list in category.values():
                    for item in pattern_list:
                        if 'file' in item:
                            file_scores[item['file']] += 1
        
        top_files = sorted(file_scores.items(), key=lambda x: x[1], reverse=True)[:10]
        for i, (file, score) in enumerate(top_files, 1):
            report.append(f"{i:2d}. {file}: {score} modernization opportunities")
        
        report.append("\n" + "=" * 80)
        
        # Write report to file
        with open(output_file, 'w') as f:
            f.write('\n'.join(report))
        
        print(f"Modernization report generated: {output_file}")
        
        # Also save detailed JSON results
        json_file = output_file.replace('.txt', '.json')
        with open(json_file, 'w') as f:
            json.dump(self.results, f, indent=2)
        
        print(f"Detailed analysis data saved: {json_file}")

def main():
    parser = argparse.ArgumentParser(description='Analyze Mozilla 1.0 codebase for modernization opportunities')
    parser.add_argument('--root', default='.', help='Root directory of the codebase')
    parser.add_argument('--output', default='analysis/reports/modernization_analysis.txt', 
                        help='Output file for the analysis report')
    
    args = parser.parse_args()
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    
    analyzer = CodebaseAnalyzer(args.root)
    analyzer.analyze_codebase()
    analyzer.generate_report(args.output)

if __name__ == '__main__':
    main()