# -*- coding: utf-8 -*-
import struct
import zlib
import os

def create_simple_png(width, height, filename):
    # PNG文件头
    png_signature = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data) & 0xffffffff
    ihdr_chunk = struct.pack('>I', len(ihdr_data)) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)

    # 创建图像数据
    image_data = bytearray()
    center_x, center_y = width // 2, height // 2
    radius = min(width, height) // 2 - 1

    for y in range(height):
        row_data = bytearray([0])  # 滤波器字节
        for x in range(width):
            dx = x - center_x
            dy = y - center_y
            distance = (dx*dx + dy*dy) ** 0.5

            if distance <= radius:
                # 蓝紫色渐变
                intensity = 1.0 - (distance / radius) * 0.5
                r = int(103 * intensity)
                g = int(110 * intensity)
                b = int(229 * intensity)
            else:
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

    print("Generated " + filename + " (" + str(width) + "x" + str(height) + ")")

def main():
    print("Generating basic PNG icons...")

    sizes = [16, 24, 32, 48, 64, 128, 256]

    for size in sizes:
        filename = "app-icon-" + str(size) + ".png"
        create_simple_png(size, size, filename)

    print("Basic icons generated successfully!")

if __name__ == "__main__":
    main()