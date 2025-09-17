#!/usr/bin/env python3
"""
使用内置库创建基本PNG图标
不依赖外部库，直接生成简单的二进制PNG
"""
import struct
import zlib

def create_simple_png(width, height, filename):
    """创建一个简单的PNG文件"""

    # PNG文件头
    png_signature = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)  # RGB, 8-bit
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data) & 0xffffffff
    ihdr_chunk = struct.pack('>I', len(ihdr_data)) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)

    # 创建图像数据 (简单的渐变圆)
    image_data = bytearray()
    center_x, center_y = width // 2, height // 2
    radius = min(width, height) // 2 - 2

    for y in range(height):
        row_data = bytearray([0])  # 滤波器字节
        for x in range(width):
            # 计算到中心的距离
            dx = x - center_x
            dy = y - center_y
            distance = (dx*dx + dy*dy) ** 0.5

            if distance <= radius:
                # 在圆内 - 蓝紫色渐变
                intensity = 1.0 - (distance / radius) * 0.6
                r = int(103 * intensity)   # 深蓝
                g = int(110 * intensity)   #
                b = int(229 * intensity)   # 蓝色
            else:
                # 圆外透明
                r = g = b = 0

            row_data.extend([r, g, b])

        image_data.extend(row_data)

    # 压缩图像数据
    compressed_data = zlib.compress(bytes(image_data))

    # IDAT chunk
    idat_crc = zlib.crc32(b'IDAT' + compressed_data) & 0xffffffff
    idat_chunk = struct.pack('>I', len(compressed_data)) + b'IDAT' + compressed_data + struct.pack('>I', idat_crc)

    # IEND chunk
    iend_crc = zlib.crc32(b'IEND') & 0xffffffff
    iend_chunk = struct.pack('>I', 0) + b'IEND' + struct.pack('>I', iend_crc)

    # 写入文件
    with open(filename, 'wb') as f:
        f.write(png_signature)
        f.write(ihdr_chunk)
        f.write(idat_chunk)
        f.write(iend_chunk)

    print(f"✓ 生成 {filename} ({width}x{height})")

def create_ico_file(png_files, ico_filename):
    """创建简单的ICO文件"""
    ico_header = struct.pack('<HHH', 0, 1, len(png_files))  # ICO格式头

    entries = bytearray()
    image_data = bytearray()
    offset = 6 + len(png_files) * 16  # 头部 + 目录条目

    for png_file in png_files:
        try:
            with open(png_file, 'rb') as f:
                data = f.read()

            # 从文件名推断尺寸
            size = int(png_file.split('-')[-1].split('.')[0])
            if size > 255:
                size = 0  # ICO格式中0表示256

            # 目录条目
            entry = struct.pack('<BBBBHHII',
                size, size,     # 宽度，高度
                0,              # 调色板颜色数
                0,              # 保留
                1,              # 颜色平面
                32,             # 每像素位数
                len(data),      # 图像数据大小
                offset          # 数据偏移
            )

            entries.extend(entry)
            image_data.extend(data)
            offset += len(data)

        except Exception as e:
            print(f"警告: 无法处理 {png_file}: {e}")

    # 写入ICO文件
    if entries:
        with open(ico_filename, 'wb') as f:
            f.write(ico_header)
            f.write(entries)
            f.write(image_data)
        print(f"✓ 生成 {ico_filename}")

def main():
    print("生成基本PNG图标...")

    sizes = [16, 24, 32, 48, 64, 128, 256]
    png_files = []

    for size in sizes:
        filename = f"app-icon-{size}.png"
        create_simple_png(size, size, filename)
        png_files.append(filename)

    # 生成ICO文件（使用前6个尺寸）
    create_ico_file(png_files[:6], "app-icon.ico")

    print("\n✓ 基本图标生成完成!")

if __name__ == "__main__":
    main()