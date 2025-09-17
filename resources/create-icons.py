#!/usr/bin/env python3
"""
生成Qt应用程序所需的各种尺寸图标
需要安装: pip install Pillow cairosvg
"""

import os
import sys
from pathlib import Path

try:
    import cairosvg
    from PIL import Image
    import io
except ImportError as e:
    print(f"错误: 缺少必要的库: {e}")
    print("请运行: pip install Pillow cairosvg")
    sys.exit(1)

def svg_to_png(svg_path, output_path, size):
    """将SVG转换为指定尺寸的PNG"""
    try:
        # 使用cairosvg将SVG转换为PNG字节
        png_bytes = cairosvg.svg2png(
            url=str(svg_path),
            output_width=size,
            output_height=size
        )

        # 使用PIL处理PNG数据
        with Image.open(io.BytesIO(png_bytes)) as img:
            # 确保是RGBA模式（支持透明度）
            if img.mode != 'RGBA':
                img = img.convert('RGBA')

            # 保存PNG文件
            img.save(output_path, 'PNG', optimize=True)
            print(f"✓ 生成 {output_path} ({size}x{size})")

    except Exception as e:
        print(f"✗ 生成 {output_path} 失败: {e}")

def create_ico_file(png_files, ico_path):
    """从多个PNG文件创建ICO文件"""
    try:
        # 打开第一个PNG作为基础
        with Image.open(png_files[0]) as img:
            # 准备其他尺寸的图像
            sizes = []
            for png_file in png_files:
                with Image.open(png_file) as size_img:
                    # 转换为RGBA确保透明度支持
                    if size_img.mode != 'RGBA':
                        size_img = size_img.convert('RGBA')
                    sizes.append(size_img.copy())

            # 保存为ICO格式
            img.save(ico_path, format='ICO', sizes=[(img.width, img.height) for img in sizes])
            print(f"✓ 生成 {ico_path}")

    except Exception as e:
        print(f"✗ 生成 {ico_path} 失败: {e}")

def main():
    # 获取脚本所在目录
    script_dir = Path(__file__).parent
    svg_file = script_dir / "app-icon.svg"

    if not svg_file.exists():
        print(f"错误: 找不到SVG文件 {svg_file}")
        return

    print("开始生成应用程序图标...")

    # 定义需要的图标尺寸
    sizes = {
        # Windows ICO标准尺寸
        16: "app-icon-16.png",
        24: "app-icon-24.png",
        32: "app-icon-32.png",
        48: "app-icon-48.png",
        64: "app-icon-64.png",
        128: "app-icon-128.png",
        256: "app-icon-256.png",

        # macOS额外尺寸
        512: "app-icon-512.png",
        1024: "app-icon-1024.png",
    }

    png_files = []

    # 生成各种尺寸的PNG
    for size, filename in sizes.items():
        output_path = script_dir / filename
        svg_to_png(svg_file, output_path, size)
        if output_path.exists():
            png_files.append(output_path)

    # 生成Windows ICO文件
    if png_files:
        ico_files = [f for f in png_files if int(f.stem.split('-')[-1]) <= 256]  # ICO限制256x256
        if ico_files:
            ico_path = script_dir / "app-icon.ico"
            create_ico_file(ico_files[:7], ico_path)  # ICO格式限制图标数量

    print("\n图标生成完成!")
    print("生成的文件:")
    for file in script_dir.glob("app-icon*"):
        if file.suffix in ['.png', '.ico']:
            print(f"  - {file.name}")

if __name__ == "__main__":
    main()