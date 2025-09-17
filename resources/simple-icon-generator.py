#!/usr/bin/env python3
"""
简单的图标生成器，使用PIL创建应用程序图标
只需要安装: pip install Pillow
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as e:
    print(f"错误: 缺少PIL库: {e}")
    print("请运行: pip install Pillow")
    sys.exit(1)

def create_icon(size, output_path):
    """创建指定尺寸的应用图标"""
    # 创建RGBA图像（支持透明度）
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 计算尺寸比例
    scale = size / 256.0

    # 绘制圆角矩形背景
    margin = int(20 * scale)
    bg_size = size - 2 * margin
    bg_corner = int(48 * scale)

    # 渐变背景（简化为单色）
    bg_color = (79, 70, 229, 255)  # 蓝紫色
    draw.rounded_rectangle(
        [margin, margin, margin + bg_size, margin + bg_size],
        radius=bg_corner,
        fill=bg_color
    )

    # 绘制工具箱
    box_x = int(64 * scale)
    box_y = int(80 * scale)
    box_w = int(128 * scale)
    box_h = int(96 * scale)
    box_corner = int(8 * scale)

    # 工具箱主体
    tool_color = (248, 250, 252, 240)  # 浅灰色半透明
    draw.rounded_rectangle(
        [box_x, box_y, box_x + box_w, box_y + box_h],
        radius=box_corner,
        fill=tool_color
    )

    # 工具箱手柄
    handle_x = int(108 * scale)
    handle_y = int(64 * scale)
    handle_w = int(40 * scale)
    handle_h = int(16 * scale)
    handle_corner = int(8 * scale)

    draw.rounded_rectangle(
        [handle_x, handle_y, handle_x + handle_w, handle_y + handle_h],
        radius=handle_corner,
        fill=tool_color
    )

    # 绘制工具（简化版）
    tool_accent = (245, 158, 11, 255)  # 橙色

    # 扳手
    wrench_x = int(80 * scale)
    wrench_y = int(110 * scale)
    wrench_w = int(24 * scale)
    wrench_h = int(4 * scale)
    draw.rounded_rectangle(
        [wrench_x, wrench_y, wrench_x + wrench_w, wrench_y + wrench_h],
        radius=int(2 * scale),
        fill=tool_accent
    )

    # 螺丝刀
    screwdriver_x = int(120 * scale)
    screwdriver_y = int(95 * scale)
    screwdriver_w = int(2 * scale)
    screwdriver_h = int(20 * scale)
    draw.rectangle(
        [screwdriver_x, screwdriver_y, screwdriver_x + screwdriver_w, screwdriver_y + screwdriver_h],
        fill=tool_accent
    )

    # 锤子
    hammer_x = int(140 * scale)
    hammer_y = int(108 * scale)
    hammer_w = int(16 * scale)
    hammer_h = int(8 * scale)
    draw.rounded_rectangle(
        [hammer_x, hammer_y, hammer_x + hammer_w, hammer_y + hammer_h],
        radius=int(2 * scale),
        fill=tool_accent
    )

    # 齿轮装饰
    gear_x = int(200 * scale)
    gear_y = int(60 * scale)
    gear_r = int(6 * scale)

    if gear_r > 2:  # 只在较大尺寸时绘制细节
        gear_color = (248, 250, 252, 160)
        draw.ellipse(
            [gear_x - gear_r, gear_y - gear_r, gear_x + gear_r, gear_y + gear_r],
            fill=gear_color
        )

    # 保存图像
    img.save(output_path, 'PNG', optimize=True)
    print(f"✓ 生成 {output_path} ({size}x{size})")

def create_ico_from_pngs(png_files, ico_path):
    """从PNG文件创建ICO文件"""
    try:
        if not png_files:
            return

        # 打开第一个PNG
        with Image.open(png_files[0]) as img:
            # 收集所有尺寸的图像
            images = []
            for png_file in png_files:
                with Image.open(png_file) as size_img:
                    if size_img.mode != 'RGBA':
                        size_img = size_img.convert('RGBA')
                    images.append(size_img.copy())

            # 保存为ICO
            img.save(ico_path, format='ICO', sizes=[(img.width, img.height) for img in images])
            print(f"✓ 生成 {ico_path}")

    except Exception as e:
        print(f"✗ 生成 {ico_path} 失败: {e}")

def main():
    script_dir = Path(__file__).parent
    print("开始生成应用程序图标...")

    # 定义需要的图标尺寸
    sizes = [16, 24, 32, 48, 64, 128, 256, 512]

    png_files = []

    # 生成各种尺寸的PNG
    for size in sizes:
        filename = f"app-icon-{size}.png"
        output_path = script_dir / filename
        create_icon(size, output_path)
        png_files.append(output_path)

    # 生成Windows ICO文件
    ico_files = [f for f in png_files if int(f.stem.split('-')[-1]) <= 256]
    if ico_files:
        ico_path = script_dir / "app-icon.ico"
        create_ico_from_pngs(ico_files, ico_path)

    print("\n图标生成完成!")
    print("生成的文件:")
    for file in script_dir.glob("app-icon-*.png"):
        print(f"  - {file.name}")
    if (script_dir / "app-icon.ico").exists():
        print(f"  - app-icon.ico")

if __name__ == "__main__":
    main()