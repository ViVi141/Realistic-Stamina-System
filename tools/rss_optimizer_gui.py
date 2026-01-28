#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RSS优化器可视化GUI
RSS Optimizer Visualization GUI

功能：
1. 实时显示优化过程
2. 参数趋势图
3. 目标函数变化图
4. 优化历史加载
5. 帕累托前沿可视化
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
try:
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("警告：matplotlib未安装，图表功能将不可用")
try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False
    print("警告：numpy未安装，数值计算功能将不可用")
import json
import os
from pathlib import Path
import threading
import time
from typing import Dict, List, Optional

class RSSTunerGUI:
    """RSS优化器可视化GUI"""
    
    def __init__(self, root):
        self.root = root
        self.root.title("RSS优化器可视化 - RSS Optimizer Visualization")
        self.root.geometry("1400x900")
        
        # 选择支持中文的字体
        self.chinese_font = self._get_chinese_font()
        
        # 样式设置
        if MATPLOTLIB_AVAILABLE:
            try:
                plt.style.use('seaborn-v0_8-darkgrid')
            except:
                plt.style.use('default')
                print("警告：seaborn样式不可用，使用默认样式")
            
            # 设置matplotlib中文字体
            try:
                import matplotlib
                matplotlib.rcParams['font.sans-serif'] = [self.chinese_font, 'Arial Unicode MS', 'SimHei', 'Microsoft YaHei']
                matplotlib.rcParams['axes.unicode_minus'] = False  # 解决负号显示问题
                print(f"已设置matplotlib字体为: {self.chinese_font}")
            except Exception as e:
                print(f"警告：无法设置matplotlib字体: {e}")
        
        # 数据存储
        self.optimization_data = {
            'iterations': [],
            'playability_burden': [],
            'stability_risk': [],
            'physiological_realism': [],
            'params': {}
        }
        
        # 当前选择的预设
        self.current_preset = "None"
        
        # 创建UI
        self._create_ui()
    
    def _get_chinese_font(self):
        """获取支持中文的字体"""
        # 尝试常见的中文字体
        chinese_fonts = [
            'Microsoft YaHei',
            'SimHei',
            'SimSun',
            'KaiTi',
            'FangSong',
            'Arial Unicode MS',
            'Segoe UI'
        ]
        
        # 获取系统字体列表
        import tkinter.font as tkfont
        available_fonts = set(tkfont.families())
        
        # 选择第一个可用的中文字体
        for font_name in chinese_fonts:
            if font_name in available_fonts:
                return font_name
        
        # 如果没有找到中文字体，使用默认字体
        return 'Arial'
    
    def _create_ui(self):
        """创建用户界面"""
        # 主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # 标题
        title_label = ttk.Label(
            main_frame,
            text="RSS优化器可视化",
            font=(self.chinese_font, 16, 'bold'),
            foreground='#1e88e5'
        )
        title_label.pack(pady=(0, 10))
        
        # 创建Notebook用于标签页
        notebook = ttk.Notebook(main_frame)
        notebook.pack(fill=tk.BOTH, expand=True)
        
        # 标签页1：实时监控
        self._create_monitor_tab(notebook)
        
        # 标签页2：参数趋势
        self._create_trends_tab(notebook)
        
        # 标签页3：目标函数
        self._create_objectives_tab(notebook)
        
        # 标签页4：优化历史
        self._create_history_tab(notebook)
        
        # 标签页5：帕累托前沿
        self._create_pareto_tab(notebook)
        
        # 底部控制按钮
        self._create_control_panel(main_frame)
    
    def _create_monitor_tab(self, notebook):
        """创建实时监控标签页"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="实时监控", padding="10")
        
        # 当前状态显示
        status_frame = ttk.LabelFrame(tab, text="当前状态", padding="10")
        status_frame.pack(fill=tk.X, pady=(0, 10))
        
        # 迭代次数
        self.iteration_label = ttk.Label(status_frame, text="迭代次数: 0")
        self.iteration_label.pack(fill=tk.X, pady=2)
        
        # 当前目标值
        self.playability_label = ttk.Label(status_frame, text="可玩性负担: 0.0000")
        self.playability_label.pack(fill=tk.X, pady=2)
        
        self.stability_label = ttk.Label(status_frame, text="稳定性风险: 0.0000")
        self.stability_label.pack(fill=tk.X, pady=2)
        
        self.realism_label = ttk.Label(status_frame, text="生理学合理性: 0.0000")
        self.realism_label.pack(fill=tk.X, pady=2)
        
        # 图表区域
        chart_frame = ttk.LabelFrame(tab, text="目标函数变化", padding="10")
        chart_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # 创建Matplotlib图表
        self.fig, self.ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.ax.set_title("目标函数变化趋势", fontsize=12, fontweight='bold', color='white')
        self.ax.set_xlabel("迭代次数", fontsize=10, color='white')
        self.ax.set_ylabel("目标值", fontsize=10, color='white')
        self.ax.tick_params(axis='both', colors='white')
        self.ax.grid(True, alpha=0.3, color='white')
        self.fig.patch.set_facecolor('#2e2e2e')
        self.ax.set_facecolor('#2e2e2e')
        
        # 初始化线条
        self.playability_line, = self.ax.plot([], [], '-', linewidth=2, label='可玩性负担', color='#4ecdc4')
        self.stability_line, = self.ax.plot([], [], '-', linewidth=2, label='稳定性风险', color='#ff6b6b')
        self.realism_line, = self.ax.plot([], [], '-', linewidth=2, label='生理学合理性', color='#34d399')
        
        self.ax.legend(loc='upper right', facecolor='#2e2e2e', edgecolor='white', labelcolor='white')
        
        # 嵌入到Tkinter
        self.canvas = FigureCanvasTkAgg(self.fig, master=chart_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # 预设选择
        preset_frame = ttk.LabelFrame(tab, text="优化预设", padding="10")
        preset_frame.pack(fill=tk.X, pady=(10, 0))
        
        ttk.Label(preset_frame, text="选择预设:").pack(anchor=tk.W, pady=5)
        
        self.preset_var = tk.StringVar(value="None")
        presets = ["EliteStandard", "StandardMilsim", "TacticalAction", "Custom"]
        preset_combo = ttk.Combobox(
            preset_frame,
            textvariable=self.preset_var,
            values=presets,
            state="readonly"
        )
        preset_combo.pack(fill=tk.X, pady=5)
        
        ttk.Button(
            preset_frame,
            text="应用预设",
            command=self._apply_preset
        ).pack(fill=tk.X, pady=5)
    
    def _create_trends_tab(self, notebook):
        """创建参数趋势标签页"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="参数趋势", padding="10")
        
        # 参数选择
        param_frame = ttk.LabelFrame(tab, text="选择参数", padding="10")
        param_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(param_frame, text="选择要显示的参数:").pack(anchor=tk.W, pady=5)
        
        self.param_var = tk.StringVar(value="base_recovery_rate")
        params = [
            "base_recovery_rate",
            "prone_recovery_multiplier",
            "standing_recovery_multiplier",
            "fast_recovery_multiplier",
            "medium_recovery_multiplier",
            "slow_recovery_multiplier",
            "sprint_stamina_drain_multiplier",
            "fatigue_accumulation_coeff",
            "encumbrance_stamina_drain_coeff",
            "posture_crouch_multiplier",
            "posture_prone_multiplier"
        ]
        
        param_combo = ttk.Combobox(
            param_frame,
            textvariable=self.param_var,
            values=params,
            state="readonly"
        )
        param_combo.pack(fill=tk.X, pady=5)
        
        # 图表区域
        chart_frame = ttk.LabelFrame(tab, text="参数变化趋势", padding="10")
        chart_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # 创建Matplotlib图表
        self.trend_fig, self.trend_ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.trend_ax.set_title(f"参数趋势: {self.param_var.get()}", fontsize=12, fontweight='bold', color='white')
        self.trend_ax.set_xlabel("迭代次数", fontsize=10, color='white')
        self.trend_ax.set_ylabel("参数值", fontsize=10, color='white')
        self.trend_ax.tick_params(axis='both', colors='white')
        self.trend_ax.grid(True, alpha=0.3, color='white')
        self.trend_fig.patch.set_facecolor('#2e2e2e')
        self.trend_ax.set_facecolor('#2e2e2e')
        
        # 初始化线条
        self.trend_line, = self.trend_ax.plot([], [], '-', linewidth=2, color='#4ecdc4')
        
        # 嵌入到Tkinter
        self.trend_canvas = FigureCanvasTkAgg(self.trend_fig, master=chart_frame)
        self.trend_canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        ttk.Button(
            chart_frame,
            text="刷新图表",
            command=self._refresh_trend_chart
        ).pack(fill=tk.X, pady=5)
    
    def _create_objectives_tab(self, notebook):
        """创建目标函数标签页"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="目标函数", padding="10")
        
        # 目标函数选择
        obj_frame = ttk.LabelFrame(tab, text="选择目标函数", padding="10")
        obj_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(obj_frame, text="选择要显示的目标函数:").pack(anchor=tk.W, pady=5)
        
        self.obj_var = tk.StringVar(value="playability_burden")
        objectives = [
            "playability_burden",
            "stability_risk",
            "physiological_realism"
        ]
        
        obj_combo = ttk.Combobox(
            obj_frame,
            textvariable=self.obj_var,
            values=objectives,
            state="readonly"
        )
        obj_combo.pack(fill=tk.X, pady=5)
        
        # 图表区域
        chart_frame = ttk.LabelFrame(tab, text="目标函数分布", padding="10")
        chart_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # 创建Matplotlib图表
        self.obj_fig, self.obj_ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.obj_ax.set_title("目标函数分布直方图", fontsize=12, fontweight='bold', color='white')
        self.obj_ax.set_xlabel("目标值", fontsize=10, color='white')
        self.obj_ax.set_ylabel("频率", fontsize=10, color='white')
        self.obj_ax.tick_params(axis='both', colors='white')
        self.obj_ax.grid(True, alpha=0.3, color='white')
        self.obj_fig.patch.set_facecolor('#2e2e2e')
        self.obj_ax.set_facecolor('#2e2e2e')
        
        # 初始化直方图
        self.obj_bars = None
        
        # 嵌入到Tkinter
        self.obj_canvas = FigureCanvasTkAgg(self.obj_fig, master=chart_frame)
        self.obj_canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        ttk.Button(
            chart_frame,
            text="刷新图表",
            command=self._refresh_obj_chart
        ).pack(fill=tk.X, pady=5)
    
    def _create_history_tab(self, notebook):
        """创建优化历史标签页"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="优化历史", padding="10")
        
        # 历史文件列表
        history_frame = ttk.LabelFrame(tab, text="优化历史文件", padding="10")
        history_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        
        # 文件列表
        self.history_listbox = tk.Listbox(
            history_frame,
            height=15,
            selectmode=tk.SINGLE
        )
        self.history_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 10))
        
        # 滚动条
        scrollbar = ttk.Scrollbar(history_frame, orient=tk.VERTICAL, command=self.history_listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.history_listbox.config(yscrollcommand=scrollbar.set)
        
        # 按钮框架
        button_frame = ttk.Frame(tab)
        button_frame.pack(fill=tk.X, pady=(10, 0))
        
        ttk.Button(
            button_frame,
            text="加载历史",
            command=self._load_history
        ).pack(fill=tk.X, pady=5)
        
        ttk.Button(
            button_frame,
            text="刷新列表",
            command=self._refresh_history_list
        ).pack(fill=tk.X, pady=5)
        
        ttk.Button(
            button_frame,
            text="清除历史",
            command=self._clear_history
        ).pack(fill=tk.X, pady=5)
        
        # 初始化历史列表
        self._refresh_history_list()
    
    def _create_pareto_tab(self, notebook):
        """创建帕累托前沿标签页"""
        tab = ttk.Frame(notebook)
        notebook.add(tab, text="帕累托前沿", padding="10")
        
        # 图表区域
        chart_frame = ttk.LabelFrame(tab, text="帕累托前沿（可玩性 vs 稳定性）", padding="10")
        chart_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # 创建Matplotlib图表
        self.pareto_fig, self.pareto_ax = plt.subplots(figsize=(10, 4), dpi=100)
        self.pareto_ax.set_title("帕累托前沿", fontsize=12, fontweight='bold', color='white')
        self.pareto_ax.set_xlabel("可玩性负担", fontsize=10, color='white')
        self.pareto_ax.set_ylabel("稳定性风险", fontsize=10, color='white')
        self.pareto_ax.tick_params(axis='both', colors='white')
        self.pareto_ax.grid(True, alpha=0.3, color='white')
        self.pareto_fig.patch.set_facecolor('#2e2e2e')
        self.pareto_ax.set_facecolor('#2e2e2e')
        
        # 初始化散点
        self.pareto_scatter = None
        
        # 嵌入到Tkinter
        self.pareto_canvas = FigureCanvasTkAgg(self.pareto_fig, master=chart_frame)
        self.pareto_canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        ttk.Button(
            chart_frame,
            text="刷新前沿",
            command=self._refresh_pareto_chart
        ).pack(fill=tk.X, pady=5)
    
    def _create_control_panel(self, parent):
        """创建底部控制面板"""
        control_frame = ttk.Frame(parent, padding="10")
        control_frame.pack(fill=tk.X, side=tk.BOTTOM)
        
        # 进度条
        self.progress_var = tk.DoubleVar(value=0.0)
        self.progress_bar = ttk.Progressbar(
            control_frame,
            variable=self.progress_var,
            maximum=100.0,
            length=300
        )
        self.progress_bar.pack(fill=tk.X, pady=5)
        
        # 状态标签
        self.status_label = ttk.Label(control_frame, text="状态: 等待数据...")
        self.status_label.pack(fill=tk.X, pady=5)
        
        # 按钮框架
        button_frame = ttk.Frame(control_frame)
        button_frame.pack(fill=tk.X, pady=(5, 0))
        
        ttk.Button(
            button_frame,
            text="加载优化数据",
            command=self._load_optimization_data
        ).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(
            button_frame,
            text="清除数据",
            command=self._clear_data
        ).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(
            button_frame,
            text="导出图表",
            command=self._export_charts
        ).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(
            button_frame,
            text="退出",
            command=self.root.quit
        ).pack(side=tk.RIGHT, padx=5)
    
    def _refresh_monitor_chart(self):
        """刷新监控图表"""
        iterations = self.optimization_data['iterations']
        playability = self.optimization_data['playability_burden']
        stability = self.optimization_data['stability_risk']
        realism = self.optimization_data['physiological_realism']
        
        # 更新线条数据
        self.playability_line.set_data(iterations, playability)
        self.stability_line.set_data(iterations, stability)
        self.realism_line.set_data(iterations, realism)
        
        # 调整坐标轴范围
        if iterations:
            self.ax.set_xlim(0, max(iterations) + 1)
            self.ax.set_ylim(
                min(min(playability), min(stability), min(realism)),
                max(max(playability), max(stability), max(realism))
            )
        
        self.canvas.draw()
    
    def _refresh_trend_chart(self):
        """刷新趋势图表"""
        param_name = self.param_var.get()
        
        if param_name not in self.optimization_data['params']:
            messagebox.showwarning("警告", f"参数 {param_name} 没有数据")
            return
        
        iterations = self.optimization_data['iterations']
        param_values = self.optimization_data['params'][param_name]
        
        # 更新线条数据
        self.trend_line.set_data(iterations, param_values)
        
        # 调整坐标轴
        self.trend_ax.set_xlim(0, max(iterations) + 1)
        self.trend_ax.set_ylim(min(param_values), max(param_values))
        self.trend_ax.set_title(f"参数趋势: {param_name}", fontsize=12, fontweight='bold', color='white')
        
        self.trend_canvas.draw()
    
    def _refresh_obj_chart(self):
        """刷新目标函数图表"""
        obj_name = self.obj_var.get()
        
        if obj_name not in self.optimization_data:
            messagebox.showwarning("警告", f"目标函数 {obj_name} 没有数据")
            return
        
        obj_values = self.optimization_data[obj_name]
        
        # 清除旧的直方图
        self.obj_ax.clear()
        
        # 创建新的直方图
        self.obj_bars = self.obj_ax.hist(
            obj_values,
            bins=30,
            alpha=0.7,
            color='#4ecdc4',
            edgecolor='white'
        )
        
        self.obj_ax.set_title(f"目标函数分布: {obj_name}", fontsize=12, fontweight='bold', color='white')
        self.obj_ax.set_xlabel("目标值", fontsize=10, color='white')
        self.obj_ax.set_ylabel("频率", fontsize=10, color='white')
        
        self.obj_canvas.draw()
    
    def _refresh_pareto_chart(self):
        """刷新帕累托前沿图表"""
        playability = self.optimization_data['playability_burden']
        stability = self.optimization_data['stability_risk']
        
        # 清除旧的散点
        self.pareto_ax.clear()
        
        # 创建新的散点图
        self.pareto_scatter = self.pareto_ax.scatter(
            playability,
            stability,
            alpha=0.6,
            s=100,
            color='#4ecdc4',
            edgecolors='white',
            linewidths=1.5
        )
        
        self.pareto_ax.set_title("帕累托前沿", fontsize=12, fontweight='bold', color='white')
        self.pareto_ax.set_xlabel("可玩性负担", fontsize=10, color='white')
        self.pareto_ax.set_ylabel("稳定性风险", fontsize=10, color='white')
        
        self.pareto_canvas.draw()
    
    def _load_optimization_data(self):
        """加载优化数据"""
        file_path = filedialog.askopenfilename(
            title="加载优化数据",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                self.optimization_data = data
                self.status_label.config(text=f"状态: 已加载 {len(data.get('iterations', []))} 次迭代")
                messagebox.showinfo("成功", "优化数据加载成功！")
                
                # 刷新所有图表
                self._refresh_monitor_chart()
                self._refresh_trend_chart()
                self._refresh_obj_chart()
                self._refresh_pareto_chart()
                
        except Exception as e:
            messagebox.showerror("错误", f"加载失败: {str(e)}")
    
    def _clear_data(self):
        """清除所有数据"""
        self.optimization_data = {
            'iterations': [],
            'playability_burden': [],
            'stability_risk': [],
            'physiological_realism': [],
            'params': {}
        }
        
        # 清除所有图表
        self.playability_line.set_data([], [])
        self.stability_line.set_data([], [])
        self.realism_line.set_data([], [])
        self.trend_line.set_data([], [])
        
        if self.obj_bars:
            self.obj_ax.clear()
            self.obj_bars = None
        
        if self.pareto_scatter:
            self.pareto_ax.clear()
            self.pareto_scatter = None
        
        self.canvas.draw()
        self.trend_canvas.draw()
        self.obj_canvas.draw()
        self.pareto_canvas.draw()
        
        self.status_label.config(text="状态: 数据已清除")
        messagebox.showinfo("成功", "所有数据已清除！")
    
    def _export_charts(self):
        """导出图表为图片"""
        if not self.optimization_data['iterations']:
            messagebox.showwarning("警告", "没有数据可导出")
            return
        
        # 创建保存目录
        save_dir = filedialog.askdirectory(title="选择保存目录")
        if not save_dir:
            return
        
        try:
            # 导出监控图表
            monitor_path = os.path.join(save_dir, "monitor_chart.png")
            self.fig.savefig(monitor_path, dpi=150, bbox_inches='tight', facecolor='#2e2e2e')
            
            # 导出趋势图表
            trend_path = os.path.join(save_dir, "trend_chart.png")
            self.trend_fig.savefig(trend_path, dpi=150, bbox_inches='tight', facecolor='#2e2e2e')
            
            # 导出目标函数图表
            obj_path = os.path.join(save_dir, "objective_chart.png")
            self.obj_fig.savefig(obj_path, dpi=150, bbox_inches='tight', facecolor='#2e2e2e')
            
            # 导出帕累托前沿图表
            pareto_path = os.path.join(save_dir, "pareto_chart.png")
            self.pareto_fig.savefig(pareto_path, dpi=150, bbox_inches='tight', facecolor='#2e2e2e')
            
            messagebox.showinfo("成功", f"图表已导出到:\n{save_dir}")
            
        except Exception as e:
            messagebox.showerror("错误", f"导出失败: {str(e)}")
    
    def _refresh_history_list(self):
        """刷新历史文件列表"""
        self.history_listbox.delete(0, tk.END)
        
        # 查找JSON文件
        current_dir = Path.cwd()
        json_files = list(current_dir.glob("*.json"))
        
        # 过滤优化相关的JSON文件
        optimization_files = [
            f for f in json_files
            if any(keyword in f.name.lower() for keyword in ['optimization', 'rss', 'config', 'bug'])
        ]
        
        for file_path in sorted(optimization_files):
            self.history_listbox.insert(tk.END, file_path.name)
    
    def _load_history(self):
        """加载选中的历史文件"""
        selection = self.history_listbox.curselection()
        if not selection:
            return
        
        file_path = Path.cwd() / self.history_listbox.get(selection[0])
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                
                # 提取优化数据
                if 'iterations' in data:
                    self.optimization_data = data
                    self.status_label.config(text=f"状态: 已加载 {len(data.get('iterations', []))} 次迭代")
                    messagebox.showinfo("成功", "历史数据加载成功！")
                    
                    # 刷新所有图表
                    self._refresh_monitor_chart()
                    self._refresh_trend_chart()
                    self._refresh_obj_chart()
                    self._refresh_pareto_chart()
                    
        except Exception as e:
            messagebox.showerror("错误", f"加载失败: {str(e)}")
    
    def _clear_history(self):
        """清除历史列表"""
        self.history_listbox.delete(0, tk.END)
        messagebox.showinfo("成功", "历史列表已清除！")
    
    def _apply_preset(self):
        """应用预设"""
        preset = self.preset_var.get()
        if preset == "None":
            messagebox.showwarning("警告", "请选择一个预设")
            return
        
        # 这里可以添加加载预设配置的逻辑
        messagebox.showinfo("预设", f"已选择预设: {preset}")
    
    def update_data(self, iteration, playability, stability, realism, params):
        """更新优化数据"""
        self.optimization_data['iterations'].append(iteration)
        self.optimization_data['playability_burden'].append(playability)
        self.optimization_data['stability_risk'].append(stability)
        self.optimization_data['physiological_realism'].append(realism)
        
        # 更新参数数据
        for param_name, param_value in params.items():
            if param_name not in self.optimization_data['params']:
                self.optimization_data['params'][param_name] = []
            self.optimization_data['params'][param_name].append(param_value)
        
        # 更新UI
        self.iteration_label.config(text=f"迭代次数: {iteration}")
        self.playability_label.config(text=f"可玩性负担: {playability:.4f}")
        self.stability_label.config(text=f"稳定性风险: {stability:.4f}")
        self.realism_label.config(text=f"生理学合理性: {realism:.4f}")
        
        # 更新进度条
        progress = (iteration / 1000.0) * 100
        self.progress_var.set(progress)
        
        # 刷新图表
        self._refresh_monitor_chart()


def main():
    """主函数"""
    root = tk.Tk()
    app = RSSTunerGUI(root)
    root.mainloop()


if __name__ == '__main__':
    main()
