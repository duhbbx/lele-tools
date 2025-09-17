# -*- coding: utf-8 -*-
import struct
import zlib
import math

def create_tool_icon_png(size, filename):
    """创建带有工具元素的PNG图标"""

    # PNG文件头
    png_signature = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk - 使用RGBA模式支持透明度
    ihdr_data = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)  # RGBA, 8-bit
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data) & 0xffffffff
    ihdr_chunk = struct.pack('>I', len(ihdr_data)) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)

    # 创建图像数据
    image_data = bytearray()
    center_x, center_y = size // 2, size // 2

    # 计算缩放因子
    scale = size / 256.0

    # 圆角矩形参数
    bg_margin = int(20 * scale)
    bg_radius = int(48 * scale)
    bg_size = size - 2 * bg_margin

    for y in range(size):
        row_data = bytearray([0])  # PNG滤波器字节

        for x in range(size):
            # 默认透明
            r, g, b, a = 0, 0, 0, 0

            # 检查是否在圆角矩形内
            if (bg_margin <= x < bg_margin + bg_size and
                bg_margin <= y < bg_margin + bg_size):

                # 计算到矩形边角的距离，实现圆角
                corner_dist = float('inf')

                # 四个角落的圆角检测
                corners = [
                    (bg_margin + bg_radius, bg_margin + bg_radius),          # 左上
                    (bg_margin + bg_size - bg_radius, bg_margin + bg_radius), # 右上
                    (bg_margin + bg_radius, bg_margin + bg_size - bg_radius), # 左下
                    (bg_margin + bg_size - bg_radius, bg_margin + bg_size - bg_radius) # 右下
                ]

                in_background = True

                # 检查四个角是否超出圆角范围
                if x < bg_margin + bg_radius and y < bg_margin + bg_radius:
                    # 左上角
                    dist = math.sqrt((x - corners[0][0])**2 + (y - corners[0][1])**2)
                    if dist > bg_radius:
                        in_background = False
                elif x >= bg_margin + bg_size - bg_radius and y < bg_margin + bg_radius:
                    # 右上角
                    dist = math.sqrt((x - corners[1][0])**2 + (y - corners[1][1])**2)
                    if dist > bg_radius:
                        in_background = False
                elif x < bg_margin + bg_radius and y >= bg_margin + bg_size - bg_radius:
                    # 左下角
                    dist = math.sqrt((x - corners[2][0])**2 + (y - corners[2][1])**2)
                    if dist > bg_radius:
                        in_background = False
                elif x >= bg_margin + bg_size - bg_radius and y >= bg_margin + bg_size - bg_radius:
                    # 右下角
                    dist = math.sqrt((x - corners[3][0])**2 + (y - corners[3][1])**2)
                    if dist > bg_radius:
                        in_background = False

                if in_background:
                    # 渐变背景 (蓝紫色)
                    progress = (y - bg_margin) / bg_size
                    r = int(103 + (118 - 103) * progress)  # 从 #676eea 到 #764ba2
                    g = int(110 + (75 - 110) * progress)
                    b = int(234 + (162 - 234) * progress)
                    a = 255

                    # 绘制工具元素
                    tool_drawn = False

                    # 扳手 (中心偏左)
                    wrench_x = center_x - int(20 * scale)
                    wrench_y = center_y
                    wrench_len = int(40 * scale)
                    wrench_width = int(6 * scale)

                    if (wrench_x - wrench_len//2 <= x <= wrench_x + wrench_len//2 and
                        wrench_y - wrench_width//2 <= y <= wrench_y + wrench_width//2):
                        # 扳手主体
                        r, g, b = 245, 158, 11  # 橙色
                        tool_drawn = True

                    # 扳手头部圆形
                    head_x = wrench_x + wrench_len//2
                    head_radius = int(8 * scale)
                    head_dist = math.sqrt((x - head_x)**2 + (y - wrench_y)**2)
                    if (head_radius - int(2 * scale) <= head_dist <= head_radius):
                        r, g, b = 245, 158, 11
                        tool_drawn = True

                    # 螺丝刀 (中心偏右上)
                    screwdriver_x = center_x + int(15 * scale)
                    screwdriver_y = center_y - int(15 * scale)
                    screwdriver_len = int(35 * scale)
                    screwdriver_width = int(3 * scale)

                    # 旋转45度的螺丝刀
                    dx = x - screwdriver_x
                    dy = y - screwdriver_y
                    cos45 = 0.707
                    sin45 = 0.707
                    rotated_x = dx * cos45 + dy * sin45
                    rotated_y = -dx * sin45 + dy * cos45

                    if (-screwdriver_len//2 <= rotated_x <= screwdriver_len//2 and
                        -screwdriver_width//2 <= rotated_y <= screwdriver_width//2):
                        r, g, b = 255, 255, 255  # 白色
                        tool_drawn = True

                    # 螺丝刀手柄
                    handle_start = int(15 * scale)
                    handle_width = int(8 * scale)
                    if (handle_start <= rotated_x <= screwdriver_len//2 and
                        -handle_width//2 <= rotated_y <= handle_width//2):
                        r, g, b = 245, 158, 11  # 橙色手柄
                        tool_drawn = True

                    # 齿轮 (右上角装饰)
                    if size >= 64:  # 只在较大尺寸显示齿轮
                        gear_x = center_x + int(35 * scale)
                        gear_y = center_y - int(35 * scale)
                        gear_outer = int(12 * scale)
                        gear_inner = int(6 * scale)

                        gear_dist = math.sqrt((x - gear_x)**2 + (y - gear_y)**2)

                        # 齿轮外圈
                        if gear_inner <= gear_dist <= gear_outer:
                            # 齿轮齿 (8个齿)
                            angle = math.atan2(y - gear_y, x - gear_x)
                            tooth_angle = (angle + math.pi) / (2 * math.pi) * 8
                            if abs(tooth_angle - round(tooth_angle)) < 0.3:
                                r, g, b = 255, 255, 255
                                a = 180
                                tool_drawn = True

                        # 齿轮中心
                        if gear_dist <= gear_inner:
                            r, g, b = 255, 255, 255
                            a = 200
                            tool_drawn = True

                    # 高光效果 (左上角)
                    highlight_x = bg_margin + int(30 * scale)
                    highlight_y = bg_margin + int(20 * scale)
                    highlight_dist = math.sqrt((x - highlight_x)**2 + (y - highlight_y)**2)

                    if highlight_dist <= int(20 * scale) and not tool_drawn:
                        # 添加高光
                        highlight_strength = 1.0 - (highlight_dist / (20 * scale))
                        highlight_strength = max(0, highlight_strength * 0.3)

                        r = min(255, int(r + 255 * highlight_strength))
                        g = min(255, int(g + 255 * highlight_strength))
                        b = min(255, int(b + 255 * highlight_strength))

            row_data.extend([r, g, b, a])

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

    print(f"Generated tool icon: {filename} ({size}x{size})")

def main():
    print("Generating tool-themed PNG icons...")

    sizes = [16, 24, 32, 48, 64, 128, 256]

    for size in sizes:
        filename = f"app-icon-{size}.png"
        create_tool_icon_png(size, filename)

    print("\nTool-themed icons generated successfully!")
    print("Features:")
    print("- Modern gradient background (blue to purple)")
    print("- Wrench tool element")
    print("- Screwdriver at 45-degree angle")
    print("- Gear decoration (visible on larger sizes)")
    print("- Subtle highlight effect")

if __name__ == "__main__":
    main()