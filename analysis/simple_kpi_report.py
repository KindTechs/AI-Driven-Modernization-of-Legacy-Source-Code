#!/usr/bin/env python3
"""
Simple KPI Report Generator
Creates comprehensive reports from KPI and progress data.
"""

import os
import json
import argparse
from datetime import datetime
from pathlib import Path


class SimpleKPIReportGenerator:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.reports_dir = self.root_dir / 'analysis' / 'reports'
        self.reports_dir.mkdir(parents=True, exist_ok=True)

    def load_json_data(self, file_path):
        """Load JSON data from file."""
        try:
            with open(file_path, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"File not found: {file_path}")
            return None
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON: {e}")
            return None

    def generate_comprehensive_report(self, kpi_file=None, progress_file=None):
        """Generate a comprehensive KPI report."""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        output_file = self.reports_dir / f'comprehensive_report_{timestamp}.txt'
        
        # Load data
        kpi_data = self.load_json_data(kpi_file) if kpi_file else None
        progress_data = self.load_json_data(progress_file) if progress_file else None
        
        report = []
        
        # Header
        report.append("=" * 80)
        report.append("MOZILLA 1.0 MODERNIZATION - COMPREHENSIVE REPORT")
        report.append(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("=" * 80)
        
        # Executive Summary
        report.append("\nEXECUTIVE SUMMARY")
        report.append("=" * 50)
        
        if kpi_data:
            kpi_summary = kpi_data.get('kpis', {}).get('summary', {})
            report.append(f"Files analyzed: {kpi_summary.get('files_analyzed', 0)}")
            report.append(f"Functions analyzed: {kpi_summary.get('total_functions', 0)}")
            report.append(f"Overall modernization score: {kpi_summary.get('modernization_score', 0):.1f}%")
            report.append(f"Code quality score: {kpi_summary.get('code_quality_score', 0):.1f}%")
            report.append(f"Documentation score: {kpi_summary.get('documentation_score', 0):.1f}%")
        
        if progress_data:
            progress_summary = progress_data.get('summary', {})
            report.append(f"\nnsSelection Progress:")
            report.append(f"Methods modernized: {progress_summary.get('methods_modernized', 0)}/{progress_summary.get('total_methods_tracked', 0)}")
            report.append(f"Modernization rate: {progress_summary.get('modernization_rate', 0):.1f}%")
            report.append(f"Average method score: {progress_summary.get('average_modernization_score', 0):.1f}%")
        
        # Detailed Metrics
        report.append("\n\nDETAILED METRICS")
        report.append("=" * 30)
        
        if kpi_data:
            kpis = kpi_data.get('kpis', {})
            
            # Code Quality
            cq = kpis.get('code_quality', {})
            if cq:
                report.append("\nCode Quality:")
                report.append(f"  Average cyclomatic complexity: {cq.get('average_cyclomatic_complexity', 0):.2f}")
                report.append(f"  Average function length: {cq.get('average_function_length', 0):.1f} lines")
                report.append(f"  Comment ratio: {cq.get('overall_comment_ratio', 0):.1f}%")
                report.append(f"  Total functions: {cq.get('total_functions', 0)}")
                report.append(f"  Total files: {cq.get('total_files', 0)}")
            
            # Modernization Coverage
            mc = kpis.get('modernization_coverage', {})
            if mc:
                report.append("\nModernization Coverage:")
                report.append(f"  Overall rate: {mc.get('modernization_rate', 0):.1f}%")
                report.append(f"  Legacy patterns: {mc.get('total_legacy_patterns', 0)}")
                report.append(f"  Modern patterns: {mc.get('total_modern_patterns', 0)}")
            
            # Documentation
            dm = kpis.get('documentation_metrics', {})
            if dm:
                report.append("\nDocumentation:")
                report.append(f"  Coverage: {dm.get('documentation_coverage', 0):.1f}%")
                report.append(f"  API docs: {dm.get('total_api_docs', 0)}")
                report.append(f"  Public methods: {dm.get('total_public_methods', 0)}")
        
        # Progress Highlights
        if progress_data:
            report.append("\n\nPROGRESS HIGHLIGHTS")
            report.append("=" * 35)
            
            summary = progress_data.get('summary', {})
            methods = progress_data.get('methods', {})
            method_scores = progress_data.get('method_scores', {})
            
            report.append(f"\nOverall Progress:")
            report.append(f"  {summary.get('methods_modernized', 0)} out of {summary.get('total_methods_tracked', 0)} methods modernized")
            report.append(f"  {summary.get('modernization_rate', 0):.1f}% completion rate")
            report.append(f"  {summary.get('average_modernization_score', 0):.1f}% average quality score")
            
            # Modernized methods
            modernized_methods = [name for name, info in methods.items() if info.get('modern_version', False)]
            if modernized_methods:
                report.append(f"\nModernized Methods:")
                for method in sorted(modernized_methods):
                    score = method_scores.get(method, 0)
                    report.append(f"  ✓ {method} (score: {score:.1f}%)")
            
            # Top priorities
            priorities = [(name, 100 - method_scores.get(name, 0)) for name in methods.keys() 
                         if methods[name].get('found', False) and not methods[name].get('modern_version', False)]
            priorities.sort(key=lambda x: x[1], reverse=True)
            
            if priorities:
                report.append(f"\nTop Priorities:")
                for method, priority in priorities[:5]:
                    report.append(f"  • {method} (needs {priority:.1f} points)")
        
        # Recommendations
        report.append("\n\nRECOMMENDATIONS")
        report.append("=" * 30)
        
        recommendations = []
        
        if kpi_data:
            kpis = kpi_data.get('kpis', {})
            cq = kpis.get('code_quality', {})
            mc = kpis.get('modernization_coverage', {})
            
            if cq.get('average_cyclomatic_complexity', 0) > 10:
                recommendations.append("🔧 Reduce cyclomatic complexity by breaking down complex functions")
            
            if cq.get('average_function_length', 0) > 50:
                recommendations.append("📏 Reduce function length by extracting helper functions")
            
            if cq.get('overall_comment_ratio', 0) < 20:
                recommendations.append("📝 Improve code documentation and inline comments")
            
            if mc.get('modernization_rate', 0) < 25:
                recommendations.append("🚀 Start with basic modernization patterns (smart pointers, Result types)")
            elif mc.get('modernization_rate', 0) < 50:
                recommendations.append("⚡ Focus on error handling modernization")
            elif mc.get('modernization_rate', 0) < 75:
                recommendations.append("🎯 Complete memory management modernization")
        
        if progress_data:
            summary = progress_data.get('summary', {})
            if summary.get('modernization_rate', 0) < 30:
                recommendations.append("🎯 Focus on modernizing GetRangeAt and GetRangeCount first")
        
        if not recommendations:
            recommendations.append("✅ Code quality metrics look good")
            recommendations.append("📈 Continue with systematic modernization")
            recommendations.append("🔍 Consider expanding analysis to other files")
        
        report.extend(recommendations)
        
        # Next Steps
        report.append("\n\nNEXT STEPS")
        report.append("=" * 20)
        
        if progress_data:
            summary = progress_data.get('summary', {})
            if summary.get('modernization_rate', 0) < 10:
                report.append("1. Create modern versions of GetRangeAt and GetRangeCount")
                report.append("2. Implement Result<T> error handling pattern")
                report.append("3. Add compatibility wrappers for existing code")
            elif summary.get('modernization_rate', 0) < 50:
                report.append("1. Continue modernizing core selection methods")
                report.append("2. Replace manual reference counting with smart pointers")
                report.append("3. Update out parameters to return values")
            elif summary.get('modernization_rate', 0) < 80:
                report.append("1. Complete modernization of remaining methods")
                report.append("2. Replace C-style casts with safe casts")
                report.append("3. Improve inline documentation")
            else:
                report.append("1. Finalize nsSelection modernization")
                report.append("2. Create comprehensive test suite")
                report.append("3. Plan modernization of additional files")
        else:
            report.append("1. Run progress analysis on nsSelection.cpp")
            report.append("2. Establish baseline measurements")
            report.append("3. Begin modernization of core methods")
        
        # Footer
        report.append("\n" + "=" * 80)
        report.append("End of Report")
        report.append("=" * 80)
        
        # Write report
        with open(output_file, 'w') as f:
            f.write('\n'.join(report))
        
        print(f"Comprehensive report generated: {output_file}")
        return output_file


def main():
    parser = argparse.ArgumentParser(description='Generate comprehensive KPI reports')
    parser.add_argument('--root', default='.', help='Root directory of the codebase')
    parser.add_argument('--kpi-data', help='KPI data JSON file')
    parser.add_argument('--progress-data', help='Progress data JSON file')
    
    args = parser.parse_args()
    
    generator = SimpleKPIReportGenerator(args.root)
    output_file = generator.generate_comprehensive_report(
        kpi_file=args.kpi_data,
        progress_file=args.progress_data
    )


if __name__ == '__main__':
    main()
