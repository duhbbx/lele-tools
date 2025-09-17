# -*- coding: utf-8 -*-
import struct
import os

def create_ico_file(png_files, ico_filename):
    valid_files = []
    for png_file in png_files:
        if os.path.exists(png_file):
            valid_files.append(png_file)

    if not valid_files:
        print("No PNG files found for ICO creation")
        return

    # ICO文件头
    ico_header = struct.pack('<HHH', 0, 1, len(valid_files))

    entries = bytearray()
    image_data = bytearray()
    offset = 6 + len(valid_files) * 16

    for png_file in valid_files:
        try:
            with open(png_file, 'rb') as f:
                data = f.read()

            # 从文件名获取尺寸
            size = int(png_file.split('-')[-1].split('.')[0])
            if size > 255:
                size = 0  # ICO格式中0表示256

            # 目录条目
            entry = struct.pack('<BBBBHHII',
                size, size,     # 宽度，高度
                0,              # 调色板颜色数
                0,              # 保留
                1,              # 颜色平面
                24,             # 每像素位数
                len(data),      # 图像数据大小
                offset          # 数据偏移
            )

            entries.extend(entry)
            image_data.extend(data)
            offset += len(data)

        except Exception as e:
            print("Warning: Cannot process " + png_file + ": " + str(e))

    # 写入ICO文件
    if entries:
        with open(ico_filename, 'wb') as f:
            f.write(ico_header)
            f.write(entries)
            f.write(image_data)
        print("Generated " + ico_filename)

def main():
    png_files = [
        "app-icon-16.png",
        "app-icon-24.png",
        "app-icon-32.png",
        "app-icon-48.png",
        "app-icon-64.png",
        "app-icon-128.png"
    ]

    create_ico_file(png_files, "app-icon.ico")
    print("ICO file created successfully!")

if __name__ == "__main__":
    main()