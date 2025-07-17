#!/usr/bin/env python3
"""
AI-Driven Mozilla 1.0 Codebase Modernization - KPI Report Generator
This script creates text-based reports of KPI measurements and progress tracking.
"""

import os
import json
import argparse
from datetime import datetime
from pathlib import Path


class KPIReportGenerator:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.reports_dir = self.root_dir / 'analysis' / 'reports'
        self.reports_dir.mkdir(parents=True, exist_ok=True)

    def load_kpi_data(self, kpi_file):
        """Load KPI data from JSON file."""
        try:
            with open(kpi_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"KPI data file not found: {kpi_file}")
            return None
        except json.JSONDecodeError as e:
            print(f"Error parsing KPI data: {e}")
            return None

    def load_progress_data(self, progress_file):
        """Load progress tracking data from JSON file."""
        try:
            with open(progress_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"Progress data file not found: {progress_file}")
            return None
        except json.JSONDecodeError as e:
            print(f"Error parsing progress data: {e}")
            return None

    def generate_executive_summary(self, kpi_data, progress_data):
        """Generate executive summary section."""
        summary = []
        summary.append("EXECUTIVE SUMMARY")
        summary.append("=" * 50)

        if kpi_data:
            kpi_summary = kpi_data.get('kpis', {}).get('summary', {})
            summary.append(f"Files analyzed: {kpi_summary.get('files_analyzed', 0)}")
            summary.append(f"Functions analyzed: {kpi_summary.get('total_functions', 0)}")
            summary.append(f"Overall modernization score: {kpi_summary.get('modernization_score', 0):.1f}%")
            summary.append(f"Code quality score: {kpi_summary.get('code_quality_score', 0):.1f}%")
            summary.append(f"Documentation score: {kpi_summary.get('documentation_score', 0):.1f}%")

        if progress_data:
            progress_summary = progress_data.get('summary', {})
            summary.append(f"\nnsSelection Progress:")
            summary.append(f"Methods modernized: {progress_summary.get('methods_modernized', 0)}/{progress_summary.get('total_methods_tracked', 0)}")
            summary.append(f"Modernization rate: {progress_summary.get('modernization_rate', 0):.1f}%")
            summary.append(f"Average method score: {progress_summary.get('average_modernization_score', 0):.1f}%")

        return summary

    def generate_kpi_trends(self, kpi_data_list):
        """Generate KPI trends section from multiple data points."""
        if not kpi_data_list or len(kpi_data_list) < 2:
            return ["KPI TRENDS", "=" * 20, "Insufficient data for trend analysis"]

        trends = []
        trends.append("KPI TRENDS")
        trends.append("=" * 20)

        # Extract trend data
        timestamps = []
        modernization_scores = []
        code_quality_scores = []
        documentation_scores = []

        for data in kpi_data_list:
            timestamp = data.get('timestamp', '')
            kpi_summary = data.get('kpis', {}).get('summary', {})

            timestamps.append(timestamp)
            modernization_scores.append(kpi_summary.get('modernization_score', 0))
            code_quality_scores.append(kpi_summary.get('code_quality_score', 0))
            documentation_scores.append(kpi_summary.get('documentation_score', 0))

        # Calculate trends
        if len(modernization_scores) >= 2:
            mod_trend = modernization_scores[-1] - modernization_scores[0]
            cq_trend = code_quality_scores[-1] - code_quality_scores[0]
            doc_trend = documentation_scores[-1] - documentation_scores[0]

            trends.append(f"Modernization score trend: {mod_trend:+.1f}%")
            trends.append(f"Code quality score trend: {cq_trend:+.1f}%")
            trends.append(f"Documentation score trend: {doc_trend:+.1f}%")

        # Recent changes
        if len(modernization_scores) >= 2:
            recent_mod = modernization_scores[-1] - modernization_scores[-2]
            recent_cq = code_quality_scores[-1] - code_quality_scores[-2]
            recent_doc = documentation_scores[-1] - documentation_scores[-2]

            trends.append(f"\nRecent changes:")
            trends.append(f"Modernization: {recent_mod:+.1f}%")
            trends.append(f"Code quality: {recent_cq:+.1f}%")
            trends.append(f"Documentation: {recent_doc:+.1f}%")

        return trends

    def generate_detailed_metrics(self, kpi_data):
        """Generate detailed metrics section."""
        if not kpi_data:
            return ["DETAILED METRICS", "=" * 30, "No KPI data available"]

        metrics = []
        metrics.append("DETAILED METRICS")
        metrics.append("=" * 30)

        kpis = kpi_data.get('kpis', {})

        # Code Quality Metrics
        cq = kpis.get('code_quality', {})
        if cq:
            metrics.append("\nCode Quality:")
            metrics.append(f"  Average cyclomatic complexity: {cq.get('average_cyclomatic_complexity', 0):.2f}")
            metrics.append(f"  Average function length: {cq.get('average_function_length', 0):.1f} lines")
            metrics.append(f"  Comment ratio: {cq.get('overall_comment_ratio', 0):.1f}%")
            metrics.append(f"  Total functions: {cq.get('total_functions', 0)}")
            metrics.append(f"  Total files: {cq.get('total_files', 0)}")

        # Modernization Coverage
        mc = kpis.get('modernization_coverage', {})
        if mc:
            metrics.append("\nModernization Coverage:")
            metrics.append(f"  Overall rate: {mc.get('modernization_rate', 0):.1f}%")
            metrics.append(f"  Legacy patterns: {mc.get('total_legacy_patterns', 0)}")
            metrics.append(f"  Modern patterns: {mc.get('total_modern_patterns', 0)}")

            # Pattern breakdown
            legacy_breakdown = mc.get('legacy_breakdown', {})
            ...

#!/usr/bin/env python3
"""
AI-Driven Mozilla 1.0 Codebase Modernization - KPI Report Generator
This script creates text-based reports of KPI measurements and progress tracking.
"""

import os
import json
import argparse
from datetime import datetime
from pathlib import Path
import subprocess
import sys

class KPIReportGenerator:
    def __init__(self, root_dir):
        self.root_dir = Path(root_dir)
        self.reports_dir = self.root_dir / 'analysis' / 'reports'
        self.reports_dir.mkdir(parents=True, exist_ok=True)
        
    def load_kpi_data(self, kpi_file):
        """Load KPI data from JSON file."""
        try:
            with open(kpi_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"KPI data file not found: {kpi_file}")
            return None
        except json.JSONDecodeError as e:
            print(f"Error parsing KPI data: {e}")
            return None
    
    def load_progress_data(self, progress_file):
        """Load progress tracking data from JSON file."""
        try:
            with open(progress_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            print(f"Progress data file not found: {progress_file}")
            return None
        except json.JSONDecodeError as e:
            print(f"Error parsing progress data: {e}")
            return None
    
    def generate_executive_summary(self, kpi_data, progress_data):
        """Generate executive summary section."""
        summary = []\n        summary.append(\"EXECUTIVE SUMMARY\")\n        summary.append(\"=\" * 50)\n        \n        if kpi_data:\n            kpi_summary = kpi_data.get('kpis', {}).get('summary', {})\n            summary.append(f\"Files analyzed: {kpi_summary.get('files_analyzed', 0)}\")\n            summary.append(f\"Functions analyzed: {kpi_summary.get('total_functions', 0)}\")\n            summary.append(f\"Overall modernization score: {kpi_summary.get('modernization_score', 0):.1f}%\")\n            summary.append(f\"Code quality score: {kpi_summary.get('code_quality_score', 0):.1f}%\")\n            summary.append(f\"Documentation score: {kpi_summary.get('documentation_score', 0):.1f}%\")\n        \n        if progress_data:\n            progress_summary = progress_data.get('summary', {})\n            summary.append(f\"\\nnsSelection Progress:\")\n            summary.append(f\"Methods modernized: {progress_summary.get('methods_modernized', 0)}/{progress_summary.get('total_methods_tracked', 0)}\")\n            summary.append(f\"Modernization rate: {progress_summary.get('modernization_rate', 0):.1f}%\")\n            summary.append(f\"Average method score: {progress_summary.get('average_modernization_score', 0):.1f}%\")\n        \n        return summary\n    \n    def generate_kpi_trends(self, kpi_data_list):\n        \"\"\"Generate KPI trends section from multiple data points.\"\"\"\n        if not kpi_data_list or len(kpi_data_list) < 2:\n            return [\"KPI TRENDS\", \"=\" * 20, \"Insufficient data for trend analysis\"]\n        \n        trends = []\n        trends.append(\"KPI TRENDS\")\n        trends.append(\"=\" * 20)\n        \n        # Extract trend data\n        timestamps = []\n        modernization_scores = []\n        code_quality_scores = []\n        documentation_scores = []\n        \n        for data in kpi_data_list:\n            timestamp = data.get('timestamp', '')\n            kpi_summary = data.get('kpis', {}).get('summary', {})\n            \n            timestamps.append(timestamp)\n            modernization_scores.append(kpi_summary.get('modernization_score', 0))\n            code_quality_scores.append(kpi_summary.get('code_quality_score', 0))\n            documentation_scores.append(kpi_summary.get('documentation_score', 0))\n        \n        # Calculate trends\n        if len(modernization_scores) >= 2:\n            mod_trend = modernization_scores[-1] - modernization_scores[0]\n            cq_trend = code_quality_scores[-1] - code_quality_scores[0]\n            doc_trend = documentation_scores[-1] - documentation_scores[0]\n            \n            trends.append(f\"Modernization score trend: {mod_trend:+.1f}%\")\n            trends.append(f\"Code quality score trend: {cq_trend:+.1f}%\")\n            trends.append(f\"Documentation score trend: {doc_trend:+.1f}%\")\n        \n        # Recent changes\n        if len(modernization_scores) >= 2:\n            recent_mod = modernization_scores[-1] - modernization_scores[-2]\n            recent_cq = code_quality_scores[-1] - code_quality_scores[-2]\n            recent_doc = documentation_scores[-1] - documentation_scores[-2]\n            \n            trends.append(f\"\\nRecent changes:\")\n            trends.append(f\"Modernization: {recent_mod:+.1f}%\")\n            trends.append(f\"Code quality: {recent_cq:+.1f}%\")\n            trends.append(f\"Documentation: {recent_doc:+.1f}%\")\n        \n        return trends\n    \n    def generate_detailed_metrics(self, kpi_data):\n        \"\"\"Generate detailed metrics section.\"\"\"\n        if not kpi_data:\n            return [\"DETAILED METRICS\", \"=\" * 30, \"No KPI data available\"]\n        \n        metrics = []\n        metrics.append(\"DETAILED METRICS\")\n        metrics.append(\"=\" * 30)\n        \n        kpis = kpi_data.get('kpis', {})\n        \n        # Code Quality Metrics\n        cq = kpis.get('code_quality', {})\n        if cq:\n            metrics.append(\"\\nCode Quality:\")\n            metrics.append(f\"  Average cyclomatic complexity: {cq.get('average_cyclomatic_complexity', 0):.2f}\")\n            metrics.append(f\"  Average function length: {cq.get('average_function_length', 0):.1f} lines\")\n            metrics.append(f\"  Comment ratio: {cq.get('overall_comment_ratio', 0):.1f}%\")\n            metrics.append(f\"  Total functions: {cq.get('total_functions', 0)}\")\n            metrics.append(f\"  Total files: {cq.get('total_files', 0)}\")\n        \n        # Modernization Coverage\n        mc = kpis.get('modernization_coverage', {})\n        if mc:\n            metrics.append(\"\\nModernization Coverage:\")\n            metrics.append(f\"  Overall rate: {mc.get('modernization_rate', 0):.1f}%\")\n            metrics.append(f\"  Legacy patterns: {mc.get('total_legacy_patterns', 0)}\")\n            metrics.append(f\"  Modern patterns: {mc.get('total_modern_patterns', 0)}\")\n            \n            # Pattern breakdown\n            legacy_breakdown = mc.get('legacy_breakdown', {})\n            if legacy_breakdown:\n                metrics.append(\"\\n  Legacy pattern breakdown:\")\n                for pattern, count in sorted(legacy_breakdown.items(), key=lambda x: x[1], reverse=True):\n                    metrics.append(f\"    {pattern}: {count}\")\n            \n            modern_breakdown = mc.get('modern_breakdown', {})\n            if modern_breakdown:\n                metrics.append(\"\\n  Modern pattern breakdown:\")\n                for pattern, count in sorted(modern_breakdown.items(), key=lambda x: x[1], reverse=True):\n                    metrics.append(f\"    {pattern}: {count}\")\n        \n        # Documentation Metrics\n        dm = kpis.get('documentation_metrics', {})\n        if dm:\n            metrics.append(\"\\nDocumentation:\")\n            metrics.append(f\"  Coverage: {dm.get('documentation_coverage', 0):.1f}%\")\n            metrics.append(f\"  API docs: {dm.get('total_api_docs', 0)}\")\n            metrics.append(f\"  Public methods: {dm.get('total_public_methods', 0)}\")\n        \n        return metrics\n    \n    def generate_progress_highlights(self, progress_data):\n        \"\"\"Generate progress highlights section.\"\"\"\n        if not progress_data:\n            return [\"PROGRESS HIGHLIGHTS\", \"=\" * 35, \"No progress data available\"]\n        \n        highlights = []\n        highlights.append(\"PROGRESS HIGHLIGHTS\")\n        highlights.append(\"=\" * 35)\n        \n        summary = progress_data.get('summary', {})\n        methods = progress_data.get('methods', {})\n        method_scores = progress_data.get('method_scores', {})\n        \n        # Overall progress\n        highlights.append(f\"\\nOverall Progress:\")\n        highlights.append(f\"  {summary.get('methods_modernized', 0)} out of {summary.get('total_methods_tracked', 0)} methods modernized\")\n        highlights.append(f\"  {summary.get('modernization_rate', 0):.1f}% completion rate\")\n        highlights.append(f\"  {summary.get('average_modernization_score', 0):.1f}% average quality score\")\n        \n        # Recently modernized methods\n        modernized_methods = [name for name, info in methods.items() if info.get('modern_version', False)]\n        if modernized_methods:\n            highlights.append(f\"\\nModernized Methods:\")\n            for method in sorted(modernized_methods):\n                score = method_scores.get(method, 0)\n                highlights.append(f\"  ✓ {method} (score: {score:.1f}%)\")\n        \n        # Top priorities\n        priorities = [(name, 100 - method_scores.get(name, 0)) for name in methods.keys() \n                     if methods[name].get('found', False) and not methods[name].get('modern_version', False)]\n        priorities.sort(key=lambda x: x[1], reverse=True)\n        \n        if priorities:\n            highlights.append(f\"\\nTop Priorities:\")\n            for method, priority in priorities[:5]:\n                highlights.append(f\"  • {method} (needs {priority:.1f} points)\")\n        \n        return highlights\n    \n    def generate_recommendations(self, kpi_data, progress_data):\n        \"\"\"Generate recommendations section.\"\"\"\n        recommendations = []\n        recommendations.append(\"RECOMMENDATIONS\")\n        recommendations.append(\"=\" * 30)\n        \n        recs = []\n        \n        # Based on KPI data\n        if kpi_data:\n            kpis = kpi_data.get('kpis', {})\n            \n            # Code quality recommendations\n            cq = kpis.get('code_quality', {})\n            if cq.get('average_cyclomatic_complexity', 0) > 10:\n                recs.append(\"🔧 Reduce cyclomatic complexity by breaking down complex functions\")\n            \n            if cq.get('average_function_length', 0) > 50:\n                recs.append(\"📏 Reduce function length by extracting helper functions\")\n            \n            if cq.get('overall_comment_ratio', 0) < 20:\n                recs.append(\"📝 Improve code documentation and inline comments\")\n            \n            # Modernization recommendations\n            mc = kpis.get('modernization_coverage', {})\n            if mc.get('modernization_rate', 0) < 25:\n                recs.append(\"🚀 Start with basic modernization patterns (smart pointers, Result types)\")\n            elif mc.get('modernization_rate', 0) < 50:\n                recs.append(\"⚡ Focus on error handling modernization\")\n            elif mc.get('modernization_rate', 0) < 75:\n                recs.append(\"🎯 Complete memory management modernization\")\n            \n            # Pattern-specific recommendations\n            legacy_breakdown = mc.get('legacy_breakdown', {})\n            if legacy_breakdown.get('legacy_casts_c_style_casts', 0) > 100:\n                recs.append(\"🔄 Replace C-style casts with safe C++ casts\")\n            \n            if legacy_breakdown.get('legacy_memory_management_addref_release', 0) > 50:\n                recs.append(\"🧠 Convert manual reference counting to smart pointers\")\n        \n        # Based on progress data\n        if progress_data:\n            summary = progress_data.get('summary', {})\n            \n            if summary.get('modernization_rate', 0) < 30:\n                recs.append(\"🎯 Focus on modernizing GetRangeAt and GetRangeCount first\")\n            \n            patterns = progress_data.get('patterns', {})\n            for pattern_type, pattern_data in patterns.items():\n                legacy_count = pattern_data.get('legacy', 0)\n                modern_count = pattern_data.get('modern', 0)\n                total = legacy_count + modern_count\n                \n                if total > 0 and (modern_count / total) < 0.3:\n                    recs.append(f\"📊 Modernize {pattern_type} patterns in nsSelection\")\n        \n        # Default recommendations if no specific issues found\n        if not recs:\n            recs.append(\"✅ Code quality metrics look good\")\n            recs.append(\"📈 Continue with systematic modernization\")\n            recs.append(\"🔍 Consider expanding analysis to other files\")\n        \n        recommendations.extend(recs)\n        \n        return recommendations\n    \n    def generate_next_steps(self, progress_data):\n        \"\"\"Generate next steps section.\"\"\"\n        next_steps = []\n        next_steps.append(\"NEXT STEPS\")\n        next_steps.append(\"=\" * 20)\n        \n        if not progress_data:\n            next_steps.append(\"1. Run progress analysis on nsSelection.cpp\")\n            next_steps.append(\"2. Establish baseline measurements\")\n            next_steps.append(\"3. Begin modernization of core methods\")\n            return next_steps\n        \n        summary = progress_data.get('summary', {})\n        methods = progress_data.get('methods', {})\n        method_scores = progress_data.get('method_scores', {})\n        \n        # Determine next steps based on progress\n        if summary.get('modernization_rate', 0) < 10:\n            next_steps.append(\"1. Create modern versions of GetRangeAt and GetRangeCount\")\n            next_steps.append(\"2. Implement Result<T> error handling pattern\")\n            next_steps.append(\"3. Add compatibility wrappers for existing code\")\n        elif summary.get('modernization_rate', 0) < 50:\n            next_steps.append(\"1. Continue modernizing core selection methods\")\n            next_steps.append(\"2. Replace manual reference counting with smart pointers\")\n            next_steps.append(\"3. Update out parameters to return values\")\n        elif summary.get('modernization_rate', 0) < 80:\n            next_steps.append(\"1. Complete modernization of remaining methods\")\n            next_steps.append(\"2. Replace C-style casts with safe casts\")\n            next_steps.append(\"3. Improve inline documentation\")\n        else:\n            next_steps.append(\"1. Finalize nsSelection modernization\")\n            next_steps.append(\"2. Create comprehensive test suite\")\n            next_steps.append(\"3. Plan modernization of additional files\")\n        \n        # Add specific method recommendations\n        unmodernized = [name for name, info in methods.items() \n                       if info.get('found', False) and not info.get('modern_version', False)]\n        if unmodernized:\n            next_steps.append(f\"\\nPriority methods to modernize:\")\n            # Sort by modernization need (lower score = higher priority)\n            method_priorities = [(name, method_scores.get(name, 0)) for name in unmodernized]\n            method_priorities.sort(key=lambda x: x[1])\n            \n            for method, score in method_priorities[:3]:\n                next_steps.append(f\"  • {method} (current score: {score:.1f}%)\")\n        \n        return next_steps\n    \n    def generate_comprehensive_report(self, kpi_file=None, progress_file=None, trend_files=None):\n        \"\"\"Generate a comprehensive KPI report.\"\"\"\n        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')\n        output_file = self.reports_dir / f'comprehensive_kpi_report_{timestamp}.txt'\n        \n        # Load data\n        kpi_data = self.load_kpi_data(kpi_file) if kpi_file else None\n        progress_data = self.load_progress_data(progress_file) if progress_file else None\n        \n        # Load trend data\n        trend_data = []\n        if trend_files:\n            for file_path in trend_files:\n                data = self.load_kpi_data(file_path)\n                if data:\n                    trend_data.append(data)\n        \n        # Generate report sections\n        report = []\n        \n        # Header\n        report.append(\"=\" * 80)\n        report.append(\"MOZILLA 1.0 MODERNIZATION - COMPREHENSIVE KPI REPORT\")\n        report.append(f\"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\")\n        report.append(\"=\" * 80)\n        \n        # Executive Summary\n        report.extend(self.generate_executive_summary(kpi_data, progress_data))\n        report.append(\"\")\n        \n        # KPI Trends\n        report.extend(self.generate_kpi_trends(trend_data))\n        report.append(\"\")\n        \n        # Detailed Metrics\n        report.extend(self.generate_detailed_metrics(kpi_data))\n        report.append(\"\")\n        \n        # Progress Highlights\n        report.extend(self.generate_progress_highlights(progress_data))\n        report.append(\"\")\n        \n        # Recommendations\n        report.extend(self.generate_recommendations(kpi_data, progress_data))\n        report.append(\"\")\n        \n        # Next Steps\n        report.extend(self.generate_next_steps(progress_data))\n        report.append(\"\")\n        \n        # Footer\n        report.append(\"=\" * 80)\n        report.append(\"End of Report\")\n        report.append(\"=\" * 80)\n        \n        # Write report\n        with open(output_file, 'w') as f:\n            f.write('\\n'.join(report))\n        \n        print(f\"Comprehensive KPI report generated: {output_file}\")\n        return output_file\n    \n    def generate_weekly_report(self, kpi_file=None, progress_file=None, baseline_file=None):\n        \"\"\"Generate a weekly progress report.\"\"\"\n        timestamp = datetime.now().strftime('%Y%m%d')\n        output_file = self.reports_dir / f'weekly_report_{timestamp}.txt'\n        \n        # Load data\n        kpi_data = self.load_kpi_data(kpi_file) if kpi_file else None\n        progress_data = self.load_progress_data(progress_file) if progress_file else None\n        baseline_data = self.load_kpi_data(baseline_file) if baseline_file else None\n        \n        report = []\n        \n        # Header\n        report.append(\"=\" * 60)\n        report.append(\"WEEKLY MODERNIZATION PROGRESS REPORT\")\n        report.append(f\"Week ending: {datetime.now().strftime('%Y-%m-%d')}\")\n        report.append(\"=\" * 60)\n        \n        # This week's achievements\n        if progress_data:\n            summary = progress_data.get('summary', {})\n            report.append(f\"\\nTHIS WEEK'S ACHIEVEMENTS:\")\n            report.append(f\"Methods modernized: {summary.get('methods_modernized', 0)}\")\n            report.append(f\"Modernization rate: {summary.get('modernization_rate', 0):.1f}%\")\n            report.append(f\"Average quality score: {summary.get('average_modernization_score', 0):.1f}%\")\n        \n        # Changes from baseline\n        if baseline_data and kpi_data:\n            report.append(f\"\\nCHANGES FROM BASELINE:\")\n            current_summary = kpi_data.get('kpis', {}).get('summary', {})\n            baseline_summary = baseline_data.get('kpis', {}).get('summary', {})\n            \n            mod_change = current_summary.get('modernization_score', 0) - baseline_summary.get('modernization_score', 0)\n            cq_change = current_summary.get('code_quality_score', 0) - baseline_summary.get('code_quality_score', 0)\n            doc_change = current_summary.get('documentation_score', 0) - baseline_summary.get('documentation_score', 0)\n            \n            report.append(f\"Modernization score: {mod_change:+.1f}%\")\n            report.append(f\"Code quality score: {cq_change:+.1f}%\")\n            report.append(f\"Documentation score: {doc_change:+.1f}%\")\n        \n        # This week's focus areas\n        report.append(f\"\\nTHIS WEEK'S FOCUS AREAS:\")\n        report.append(f\"• Error handling modernization\")\n        report.append(f\"• Smart pointer adoption\")\n        report.append(f\"• Test coverage improvement\")\n        \n        # Next week's goals\n        report.append(f\"\\nNEXT WEEK'S GOALS:\")\n        if progress_data:\n            summary = progress_data.get('summary', {})\n            current_rate = summary.get('modernization_rate', 0)\n            target_rate = min(current_rate + 10, 100)\n            report.append(f\"• Achieve {target_rate:.0f}% modernization rate\")\n            report.append(f\"• Modernize 2-3 additional methods\")\n            report.append(f\"• Improve code documentation\")\n        \n        # Issues and blockers\n        report.append(f\"\\nISSUES AND BLOCKERS:\")\n        report.append(f\"• None reported this week\")\n        \n        report.append(\"\\n\" + \"=\" * 60)\n        \n        # Write report\n        with open(output_file, 'w') as f:\n            f.write('\\n'.join(report))\n        \n        print(f\"Weekly report generated: {output_file}\")\n        return output_file\n\ndef main():\n    parser = argparse.ArgumentParser(description='Generate KPI reports for Mozilla 1.0 modernization')\n    parser.add_argument('--root', default='.', help='Root directory of the codebase')\n    parser.add_argument('--kpi-data', help='KPI data JSON file')\n    parser.add_argument('--progress-data', help='Progress data JSON file')\n    parser.add_argument('--baseline', help='Baseline data file for comparison')\n    parser.add_argument('--trend-files', nargs='*', help='Multiple KPI files for trend analysis')\n    parser.add_argument('--report-type', choices=['comprehensive', 'weekly'], default='comprehensive',\n                        help='Type of report to generate')\n    parser.add_argument('--output', help='Output file path (auto-generated if not specified)')\n    \n    args = parser.parse_args()\n    \n    generator = KPIReportGenerator(args.root)\n    \n    if args.report_type == 'comprehensive':\n        output_file = generator.generate_comprehensive_report(\n            kpi_file=args.kpi_data,\n            progress_file=args.progress_data,\n            trend_files=args.trend_files\n        )\n    elif args.report_type == 'weekly':\n        output_file = generator.generate_weekly_report(\n            kpi_file=args.kpi_data,\n            progress_file=args.progress_data,\n            baseline_file=args.baseline\n        )\n    \n    if args.output:\n        # Copy to specified output location\n        import shutil\n        shutil.copy2(output_file, args.output)\n        print(f\"Report also saved to: {args.output}\")\n\nif __name__ == '__main__':\n    main()"