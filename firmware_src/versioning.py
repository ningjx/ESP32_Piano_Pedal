Import("env")
import os
import shutil

# 从 platformio.ini 读取自定义的固件版本号
fw_version = env.GetProjectOption("firmware_version")

if not fw_version:
    fw_version = "0.0.0"

# 1）修改程序名（影响 .elf / .map 等文件名）
env.Replace(PROGNAME=f"firmware_v{fw_version}")


# 2）在构建完成后，从 PlatformIO 生成的默认 firmware.bin
#    复制一份为 firmware_vX.Y.Z.bin，方便你拿去烧录/发布
def copy_versioned_firmware(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dst = os.path.join(build_dir, f"firmware_v{fw_version}.bin")

    if os.path.exists(src):
        shutil.copy(src, dst)
        print(f"[versioning] 生成带版本固件: {dst}")
    else:
        print(f"[versioning] 未找到 {src}，无法生成带版本固件")


# 在 buildprog（编译固件）完成后执行拷贝动作
env.AddPostAction("buildprog", copy_versioned_firmware)

