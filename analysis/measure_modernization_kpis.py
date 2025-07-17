#!/usr/bin/env python3
"""
AI-Driven Mozilla 1.0 Codebase Modernization - KPI Measurement Script
This script analyzes C/C++ code for modernization patterns and measures KPIs.
"""

import os
import re
import json
import ast
from collections import defaultdict
from pathlib import Path
import argparse
from datetime import datetime
import subprocess
import sys

class KPIMeasurer:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.kpis = {
            'code_quality': {},
            'modernization_coverage': {},
            'pattern_reduction': {},
            'documentation_metrics': {},
            'summary': {}
        }
        
    def measure_cyclomatic_complexity(self, file_path):
        """Measure cyclomatic complexity using a simplified approach."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Count decision points (simplified McCabe complexity)
            complexity_keywords = [
                r'\bif\b', r'\belse\b', r'\bwhile\b', r'\bfor\b', 
                r'\bdo\b', r'\bswitch\b', r'\bcase\b', r'\bcatch\b',
                r'\b\?\s*[^:]+:', r'\b&&\b', r'\b\|\|\b'
            ]
            
            total_complexity = 0
            function_count = 0
            
            # Find functions and measure their complexity
            function_pattern = r'(\w+\s+)+\w+\s*\([^)]*\)\s*\{'
            functions = re.finditer(function_pattern, content)
            
            for func_match in functions:
                func_start = func_match.start()
                # Find the end of the function (simplified - count braces)
                brace_count = 0
                func_end = func_start
                
                for i, char in enumerate(content[func_start:], func_start):
                    if char == '{':
                        brace_count += 1
                    elif char == '}':
                        brace_count -= 1
                        if brace_count == 0:
                            func_end = i
                            break
                
                func_content = content[func_start:func_end]
                func_complexity = 1  # Base complexity
                
                # Count complexity contributors
                for keyword in complexity_keywords:
                    matches = re.findall(keyword, func_content)
                    func_complexity += len(matches)
                
                total_complexity += func_complexity
                function_count += 1
            
            avg_complexity = total_complexity / function_count if function_count > 0 else 0
            return {
                'total_complexity': total_complexity,
                'function_count': function_count,
                'average_complexity': avg_complexity
            }
            
        except Exception as e:
            print(f"Error measuring complexity for {file_path}: {e}")
            return {'total_complexity': 0, 'function_count': 0, 'average_complexity': 0}
    
    def measure_function_length(self, file_path):
        """Measure average function length in lines."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            
            content = ''.join(lines)
            
            # Find functions and measure their length
            function_pattern = r'(\w+\s+)+\w+\s*\([^)]*\)\s*\{'
            functions = re.finditer(function_pattern, content)
            
            function_lengths = []
            
            for func_match in functions:
                func_start = func_match.start()
                start_line = content[:func_start].count('\n')
                
                # Find the end of the function
                brace_count = 0
                func_end = func_start
                
                for i, char in enumerate(content[func_start:], func_start):
                    if char == '{':
                        brace_count += 1
                    elif char == '}':
                        brace_count -= 1
                        if brace_count == 0:
                            func_end = i
                            break
                
                end_line = content[:func_end].count('\n')
                
                # Count non-comment, non-blank lines
                func_lines = lines[start_line:end_line + 1]
                non_empty_lines = 0
                
                for line in func_lines:
                    stripped = line.strip()
                    if stripped and not stripped.startswith('//') and not stripped.startswith('/*'):
                        non_empty_lines += 1
                
                function_lengths.append(non_empty_lines)
            
            avg_length = sum(function_lengths) / len(function_lengths) if function_lengths else 0
            return {
                'function_count': len(function_lengths),
                'total_lines': sum(function_lengths),
                'average_length': avg_length,
                'max_length': max(function_lengths) if function_lengths else 0,
                'min_length': min(function_lengths) if function_lengths else 0
            }
            
        except Exception as e:
            print(f"Error measuring function length for {file_path}: {e}")
            return {'function_count': 0, 'total_lines': 0, 'average_length': 0, 'max_length': 0, 'min_length': 0}
    
    def measure_comment_ratio(self, file_path):
        """Measure comment-to-code ratio."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            
            total_lines = len(lines)
            comment_lines = 0
            blank_lines = 0
            
            in_multiline_comment = False
            
            for line in lines:
                stripped = line.strip()
                
                if not stripped:
                    blank_lines += 1
                    continue
                
                # Check for multiline comments
                if '/*' in stripped and '*/' in stripped:
                    # Single line multiline comment
                    comment_lines += 1
                elif '/*' in stripped:
                    in_multiline_comment = True
                    comment_lines += 1
                elif '*/' in stripped:
                    in_multiline_comment = False
                    comment_lines += 1
                elif in_multiline_comment:
                    comment_lines += 1
                elif stripped.startswith('//'):
                    comment_lines += 1
                elif stripped.startswith('*'):
                    # Part of multiline comment
                    comment_lines += 1
            
            code_lines = total_lines - comment_lines - blank_lines
            comment_ratio = (comment_lines / code_lines * 100) if code_lines > 0 else 0
            
            return {
                'total_lines': total_lines,
                'comment_lines': comment_lines,
                'code_lines': code_lines,
                'blank_lines': blank_lines,
                'comment_ratio': comment_ratio
            }
            
        except Exception as e:
            print(f"Error measuring comment ratio for {file_path}: {e}")
            return {'total_lines': 0, 'comment_lines': 0, 'code_lines': 0, 'blank_lines': 0, 'comment_ratio': 0}
    
    def count_modernization_patterns(self, file_path):
        """Count various modernization patterns in the file."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            patterns = {
                'legacy_error_handling': {
                    'NS_SUCCEEDED': len(re.findall(r'\bNS_SUCCEEDED\s*\(', content)),
                    'NS_FAILED': len(re.findall(r'\bNS_FAILED\s*\(', content)),
                    'error_codes': len(re.findall(r'\bNS_ERROR_\w+', content))
                },
                'legacy_memory_management': {
                    'malloc_free': len(re.findall(r'\bmalloc\s*\(', content)) + len(re.findall(r'\bfree\s*\(', content)),
                    'new_delete': len(re.findall(r'\bnew\s+\w+', content)) + len(re.findall(r'\bdelete\s+', content)),
                    'addref_release': len(re.findall(r'\bAddRef\s*\(', content)) + len(re.findall(r'\bRelease\s*\(', content))
                },
                'legacy_casts': {
                    'c_style_casts': len(re.findall(r'\([A-Za-z_][A-Za-z0-9_]*\s*\*\s*\)', content)),
                    'static_casts': len(re.findall(r'\bstatic_cast\s*<', content)),
                    'dynamic_casts': len(re.findall(r'\bdynamic_cast\s*<', content)),
                    'reinterpret_casts': len(re.findall(r'\breinterpret_cast\s*<', content))
                },
                'modern_patterns': {
                    'smart_pointers': len(re.findall(r'\bnsCOMPtr\s*<', content)) + len(re.findall(r'\bRefPtr\s*<', content)),
                    'result_types': len(re.findall(r'\bResult\s*<', content)),
                    'optional_types': len(re.findall(r'\bMaybe\s*<', content)) + len(re.findall(r'\bOptional\s*<', content)),
                    'auto_keyword': len(re.findall(r'\bauto\s+\w+', content))
                },
                'out_parameters': {
                    'double_pointers': len(re.findall(r'\w+\s*\*\*\s*\w+', content)),
                    'reference_params': len(re.findall(r'\w+\s*&\s*\w+', content))
                },
                'null_checks': {
                    'basic_null_checks': len(re.findall(r'\w+\s*[!=]=\s*(?:null|nullptr|nsnull)', content)),
                    'if_not_checks': len(re.findall(r'\bif\s*\(\s*!\s*\w+\s*\)', content))
                }
            }
            
            return patterns
            
        except Exception as e:
            print(f"Error counting patterns for {file_path}: {e}")
            return {}
    
    def measure_documentation_metrics(self, file_path):
        """Measure documentation-related metrics."""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            # Count API documentation (simplified)
            api_doc_patterns = [
                r'/\*\*.*?\*/',  # Doxygen-style comments
                r'//\s*@\w+',    # JSDoc-style annotations
                r'//\s*\w+:',    # Parameter documentation
            ]
            
            api_doc_count = 0
            for pattern in api_doc_patterns:
                matches = re.findall(pattern, content, re.DOTALL)
                api_doc_count += len(matches)
            
            # Count functions
            function_count = len(re.findall(r'(\w+\s+)+\w+\s*\([^)]*\)\s*\{', content))
            
            # Count public methods (simplified)
            public_method_count = len(re.findall(r'public:\s*\n.*?(\w+\s+)+\w+\s*\([^)]*\)', content, re.DOTALL))
            
            documentation_coverage = (api_doc_count / function_count * 100) if function_count > 0 else 0
            
            return {
                'api_doc_count': api_doc_count,
                'function_count': function_count,
                'public_method_count': public_method_count,
                'documentation_coverage': documentation_coverage
            }
            
        except Exception as e:
            print(f"Error measuring documentation for {file_path}: {e}")
            return {'api_doc_count': 0, 'function_count': 0, 'public_method_count': 0, 'documentation_coverage': 0}
    
    def analyze_file(self, file_path):
        """Analyze a single file for all KPIs."""
        relative_path = str(file_path.relative_to(self.root_dir))
        print(f"Analyzing {relative_path}...")
        
        # Measure code quality metrics
        complexity = self.measure_cyclomatic_complexity(file_path)
        function_length = self.measure_function_length(file_path)
        comment_ratio = self.measure_comment_ratio(file_path)
        
        # Count modernization patterns
        patterns = self.count_modernization_patterns(file_path)
        
        # Measure documentation metrics
        documentation = self.measure_documentation_metrics(file_path)
        
        return {
            'file': relative_path,
            'code_quality': {
                'complexity': complexity,
                'function_length': function_length,
                'comment_ratio': comment_ratio
            },
            'patterns': patterns,
            'documentation': documentation
        }
    
    def analyze_codebase(self, file_patterns=None):
        """Analyze the entire codebase or specific files."""
        if file_patterns is None:
            file_patterns = ['*.cpp', '*.h', '*.c']
        
        all_files = []
        for pattern in file_patterns:
            all_files.extend(self.root_dir.rglob(pattern))
        
        print(f"Found {len(all_files)} files to analyze")
        
        file_results = []
        for file_path in all_files:
            result = self.analyze_file(file_path)
            file_results.append(result)
        
        # Calculate aggregate metrics
        self.calculate_aggregate_metrics(file_results)
        
        return file_results
    
    def calculate_aggregate_metrics(self, file_results):
        """Calculate aggregate KPIs from individual file results."""
        if not file_results:
            return
        
        # Code quality aggregates
        total_complexity = sum(r['code_quality']['complexity']['total_complexity'] for r in file_results)
        total_functions = sum(r['code_quality']['complexity']['function_count'] for r in file_results)
        avg_complexity = total_complexity / total_functions if total_functions > 0 else 0
        
        total_function_lines = sum(r['code_quality']['function_length']['total_lines'] for r in file_results)
        avg_function_length = total_function_lines / total_functions if total_functions > 0 else 0
        
        total_comment_lines = sum(r['code_quality']['comment_ratio']['comment_lines'] for r in file_results)
        total_code_lines = sum(r['code_quality']['comment_ratio']['code_lines'] for r in file_results)
        overall_comment_ratio = (total_comment_lines / total_code_lines * 100) if total_code_lines > 0 else 0
        
        self.kpis['code_quality'] = {
            'average_cyclomatic_complexity': avg_complexity,
            'average_function_length': avg_function_length,
            'overall_comment_ratio': overall_comment_ratio,
            'total_functions': total_functions,
            'total_files': len(file_results)
        }
        
        # Pattern aggregates
        legacy_patterns = defaultdict(int)
        modern_patterns = defaultdict(int)
        
        for result in file_results:
            patterns = result.get('patterns', {})
            
            # Count legacy patterns
            for category, subcategory in patterns.items():
                if category.startswith('legacy_'):
                    for pattern, count in subcategory.items():
                        legacy_patterns[f"{category}_{pattern}"] += count
            
            # Count modern patterns
            if 'modern_patterns' in patterns:
                for pattern, count in patterns['modern_patterns'].items():
                    modern_patterns[pattern] += count
        
        total_legacy = sum(legacy_patterns.values())
        total_modern = sum(modern_patterns.values())
        modernization_rate = (total_modern / (total_legacy + total_modern) * 100) if (total_legacy + total_modern) > 0 else 0
        
        self.kpis['modernization_coverage'] = {
            'modernization_rate': modernization_rate,
            'total_legacy_patterns': total_legacy,
            'total_modern_patterns': total_modern,
            'legacy_breakdown': dict(legacy_patterns),
            'modern_breakdown': dict(modern_patterns)
        }
        
        # Documentation aggregates
        total_api_docs = sum(r['documentation']['api_doc_count'] for r in file_results)
        total_public_methods = sum(r['documentation']['public_method_count'] for r in file_results)
        documentation_coverage = (total_api_docs / total_functions * 100) if total_functions > 0 else 0
        
        self.kpis['documentation_metrics'] = {
            'documentation_coverage': documentation_coverage,
            'total_api_docs': total_api_docs,
            'total_public_methods': total_public_methods
        }
        
        # Summary
        self.kpis['summary'] = {
            'files_analyzed': len(file_results),
            'total_functions': total_functions,
            'modernization_score': modernization_rate,
            'code_quality_score': (100 - min(avg_complexity * 5, 100)),  # Simplified score
            'documentation_score': documentation_coverage
        }
    
    def generate_report(self, output_file, file_results):
        """Generate a comprehensive KPI report."""
        report = []
        report.append("=" * 80)
        report.append("Mozilla 1.0 Modernization KPI Report")
        report.append(f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("=" * 80)
        
        # Summary
        summary = self.kpis['summary']
        report.append(f"\nSUMMARY:")
        report.append(f"Files analyzed: {summary['files_analyzed']}")
        report.append(f"Total functions: {summary['total_functions']}")
        report.append(f"Modernization score: {summary['modernization_score']:.2f}%")
        report.append(f"Code quality score: {summary['code_quality_score']:.2f}%")
        report.append(f"Documentation score: {summary['documentation_score']:.2f}%")
        
        # Code Quality Metrics
        cq = self.kpis['code_quality']
        report.append(f"\nCODE QUALITY METRICS:")
        report.append(f"Average cyclomatic complexity: {cq['average_cyclomatic_complexity']:.2f}")
        report.append(f"Average function length: {cq['average_function_length']:.2f} lines")
        report.append(f"Overall comment ratio: {cq['overall_comment_ratio']:.2f}%")
        
        # Modernization Coverage
        mc = self.kpis['modernization_coverage']
        report.append(f"\nMODERNIZATION COVERAGE:")
        report.append(f"Modernization rate: {mc['modernization_rate']:.2f}%")
        report.append(f"Total legacy patterns: {mc['total_legacy_patterns']}")
        report.append(f"Total modern patterns: {mc['total_modern_patterns']}")
        
        report.append(f"\nLegacy pattern breakdown:")
        for pattern, count in mc['legacy_breakdown'].items():
            report.append(f"  {pattern}: {count}")
        
        report.append(f"\nModern pattern breakdown:")
        for pattern, count in mc['modern_breakdown'].items():
            report.append(f"  {pattern}: {count}")
        
        # Documentation Metrics
        dm = self.kpis['documentation_metrics']
        report.append(f"\nDOCUMENTATION METRICS:")
        report.append(f"Documentation coverage: {dm['documentation_coverage']:.2f}%")
        report.append(f"Total API docs: {dm['total_api_docs']}")
        report.append(f"Total public methods: {dm['total_public_methods']}")
        
        # Top files needing modernization
        report.append(f"\nTOP FILES NEEDING MODERNIZATION:")
        file_scores = []
        for result in file_results:
            patterns = result.get('patterns', {})
            legacy_count = 0
            for category, subcategory in patterns.items():
                if category.startswith('legacy_'):
                    legacy_count += sum(subcategory.values())
            file_scores.append((result['file'], legacy_count))
        
        file_scores.sort(key=lambda x: x[1], reverse=True)
        for i, (file, score) in enumerate(file_scores[:10], 1):
            report.append(f"{i:2d}. {file}: {score} legacy patterns")
        
        report.append("\n" + "=" * 80)
        
        # Write report
        with open(output_file, 'w') as f:
            f.write('\n'.join(report))
        
        print(f"KPI report generated: {output_file}")
        
        # Also save detailed JSON
        json_file = output_file.replace('.txt', '.json')
        with open(json_file, 'w') as f:
            json.dump({
                'kpis': self.kpis,
                'file_results': file_results,
                'timestamp': datetime.now().isoformat()
            }, f, indent=2)
        
        print(f"Detailed KPI data saved: {json_file}")
    
    def analyze_documentation(self):
        """Analyze documentation metrics for the codebase."""
        doc_metrics = {
            'total_doc_files': 0,
            'total_doc_lines': 0,
            'modernization_docs': 0,
            'template_docs': 0,
            'api_docs': 0,
            'doc_coverage': 0.0
        }
        
        # Find documentation files
        doc_patterns = ['*.md', '*.txt', '*.rst', '*.doc']
        doc_files = []
        
        for pattern in doc_patterns:
            doc_files.extend(glob.glob(os.path.join(self.root_dir, '**', pattern), recursive=True))
        
        doc_metrics['total_doc_files'] = len(doc_files)
        
        # Count documentation lines and categorize
        for file_path in doc_files:
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    lines = content.split('\n')
                    doc_metrics['total_doc_lines'] += len(lines)
                    
                    # Check for modernization documentation
                    if any(keyword in content.lower() for keyword in ['modern', 'result<', 'smart pointer', 'template']):
                        doc_metrics['modernization_docs'] += 1
                    
                    # Check for template documentation
                    if 'template' in os.path.basename(file_path).lower():
                        doc_metrics['template_docs'] += 1
                    
                    # Check for API documentation
                    if any(keyword in content.lower() for keyword in ['api', 'method', 'function', 'class']):
                        doc_metrics['api_docs'] += 1
                        
            except Exception as e:
                print(f"Warning: Could not read {file_path}: {e}")
        
        # Calculate documentation coverage
        if doc_metrics['total_doc_files'] > 0:
            doc_metrics['doc_coverage'] = (doc_metrics['modernization_docs'] / doc_metrics['total_doc_files']) * 100
        
        return doc_metrics
    
    def calculate_documentation_score(self, doc_metrics):
        """Calculate an overall documentation score."""
        if doc_metrics['total_doc_files'] == 0:
            return 0.0
        
        # Score based on various factors
        coverage_score = min(doc_metrics['doc_coverage'], 100) * 0.4
        template_score = min((doc_metrics['template_docs'] / max(doc_metrics['total_doc_files'], 1)) * 100, 100) * 0.3
        api_score = min((doc_metrics['api_docs'] / max(doc_metrics['total_doc_files'], 1)) * 100, 100) * 0.3
        
        return coverage_score + template_score + api_score

def main():
    parser = argparse.ArgumentParser(description='Measure modernization KPIs for Mozilla 1.0 codebase')
    parser.add_argument('--root', default='.', help='Root directory of the codebase')
    parser.add_argument('--output', default='analysis/reports/kpi_report.txt', help='Output file for the KPI report')
    parser.add_argument('--files', nargs='*', help='Specific files to analyze (default: all .cpp, .h, .c files)')
    parser.add_argument('--patterns', nargs='*', default=['*.cpp', '*.h', '*.c'], help='File patterns to analyze')
    
    args = parser.parse_args()
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    
    measurer = KPIMeasurer(args.root)
    
    if args.files:
        # Analyze specific files
        file_results = []
        for file_path in args.files:
            path = Path(file_path)
            if path.exists():
                result = measurer.analyze_file(path)
                file_results.append(result)
        measurer.calculate_aggregate_metrics(file_results)
    else:
        # Analyze entire codebase
        file_results = measurer.analyze_codebase(args.patterns)
    
    measurer.generate_report(args.output, file_results)

if __name__ == '__main__':
    main()