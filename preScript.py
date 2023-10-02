import os
import shutil
import platform
import contextlib

Import("env")

operating_system = platform.system()
if operating_system == 'Linux' or operating_system == 'Darwin':  # Unix/Linux/macOS
    devnull = '/dev/null'
elif operating_system == 'Windows':  # Windows
    devnull = 'NUL'

def copy_files(source, target, env):
    source_dir = os.path.join('src', 'latest', env["PIOENV"].replace('_', '.'), 'filesystem')
    target_dir = os.path.join('data')

    # Copy files to data dir
    print(f"[SCRIPT] Copying from \"{source_dir}\" to \"{target_dir}\"...")
    if os.path.exists(target_dir): shutil.rmtree(target_dir)
    shutil.copytree(source_dir, target_dir)

def remove_files(source, target, env):
    target_dir = os.path.join('data')

    # Remove unneeded path
    shutil.rmtree(target_dir)
    print("[SCRIPT] Removed temporary \"data\" folder.")

env.AddPreAction("$BUILD_DIR/littlefs.bin", copy_files)
env.AddPostAction("$BUILD_DIR/littlefs.bin", remove_files)