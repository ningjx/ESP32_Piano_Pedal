Import("env")
import os
import shutil
import re
from platformio import fs

# 仅从 CPPDEFINES 解析 FW_VERSION，避免多源和引号问题
fw_version = "0.0.0"

cpp_defines = env.get("CPPDEFINES", [])
for define in cpp_defines:
    # 可能是 ("FW_VERSION", "1.1.0") 或 "FW_VERSION=1.1.0"
    if isinstance(define, (list, tuple)) and len(define) == 2 and define[0] == "FW_VERSION":
        fw_version = str(define[1])
        break
    if isinstance(define, str):
        match = re.search(r'FW_VERSION=(?:"?)([^"\s]+)(?:"?)', define)
        if match:
            fw_version = match.group(1)
            break

# 仅保留版本号中的数字和点，例如从 "1.1.0" 或 v1.1.0 提取 1.1.0
m = re.search(r'([0-9]+(?:\.[0-9]+)*)', fw_version)
fw_version = m.group(1) if m else "0.0.0"

print(f"[versioning] 提取的固件版本号: {fw_version}")

# 1）修改程序名（影响 .elf / .map 等文件名）
env.Replace(PROGNAME=f"firmware_v{fw_version}")

# 2）在构建完成后，复制默认 firmware.bin 为带版本号的固件
def copy_versioned_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dst = os.path.join(build_dir, f"firmware_v{fw_version}.bin")

    if os.path.exists(src):
        shutil.copy(src, dst)
        print(f"[versioning] 生成带版本固件: {dst}")
    else:
        print(f"[versioning] 未找到 {src}，无法生成带版本固件")

env.AddPostAction("buildprog", copy_versioned_firmware)

