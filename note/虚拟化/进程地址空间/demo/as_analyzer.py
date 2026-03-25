#!/usr/bin/env python3
import sys
import os
import platform
import subprocess
import tempfile

def get_arch():
    machine = platform.machine().lower()
    if 'x86_64' in machine:
        return 'x86-64'
    elif 'aarch64' in machine or 'arm64' in machine:
        return 'arm64'
    return machine

def disassemble(buffer, arch, start_addr):
    """使用 objdump 反汇编原始二进制缓冲"""
    with tempfile.NamedTemporaryFile() as tmp:
        tmp.write(buffer)
        tmp.flush()
        
        # 确定架构参数
        arch_map = {
            'x86-64': ['-m', 'i386', '-M', 'x86-64'],
            'arm64': ['-m', 'aarch64']
        }
        
        cmd = ['objdump', '-D', '-b', 'binary'] + arch_map.get(arch, []) + \
              ['--adjust-vma=' + hex(start_addr), tmp.name]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            # 跳过 objdump 前几行头信息
            lines = result.stdout.splitlines()[7:]
            print('\n'.join(lines[:30])) # 只输出前 30 行汇编
            if len(lines) > 30:
                print("...")
        except subprocess.CalledProcessError as e:
            print(f"Disassembly failed: {e}")

def analyze_as(pid):
    maps_path = f"/proc/{pid}/maps"
    mem_path = f"/proc/{pid}/mem"
    
    if not os.path.exists(maps_path):
        print(f"Error: Process {pid} not found.")
        return

    arch = get_arch()
    print(f"--- Analyzing Address Space for PID {pid} (Arch: {arch}) ---")
    print(f"{'Address Range':<25} {'Perms':<6} {'Offset':<10} {'Path/Region'}")
    print("-" * 70)

    try:
        with open(maps_path, 'r') as f_maps, open(mem_path, 'rb') as f_mem:
            for line in f_maps:
                parts = line.split()
                if len(parts) < 5: continue
                
                addr_range = parts[0]
                perms = parts[1]
                offset = parts[2]
                path = parts[-1] if len(parts) > 5 else "[anon]"
                
                print(f"{addr_range:<25} {perms:<6} {offset:<10} {path}")

                # 如果是可执行区域，尝试读取并反汇编首 1KB
                if 'x' in perms:
                    start_addr = int(addr_range.split('-')[0], 16)
                    try:
                        f_mem.seek(start_addr)
                        code = f_mem.read(1024)
                        print(f"  [Disassembling first 1KB of {path} at {hex(start_addr)}]")
                        disassemble(code, arch, start_addr)
                        print("-" * 50)
                    except Exception as e:
                        print(f"  [Could not read memory: {e}]")

    except PermissionError:
        print("Error: Permission denied. Try running with sudo to read /proc/[pid]/mem.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: ./as_analyzer.py <pid>")
        sys.exit(1)
    
    analyze_as(sys.argv[1])
