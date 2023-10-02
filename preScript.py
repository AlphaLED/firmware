import os
import shutil

Import("env")

def partitions_copy():
    source = os.path.join('partitions', env["PIOENV"].replace('_', '.') + '.csv')
    target = os.path.join('.', 'partitions.csv')

    # Copy csv file
    shutil.copy(source, target)

def partitions_clear(source, target, env):
    target = os.path.join('.', 'partitions.csv')

    # Remove unneeded csv
    os.remove(target)

def copy_data(source, target, env):
    source_dir = os.path.join('src', 'latest', env["PIOENV"].replace('_', '.'), 'filesystem')
    target_dir = os.path.join('data')

    # Copy files to data dir
    if os.path.exists(target_dir): shutil.rmtree(target_dir)
    shutil.copytree(source_dir, target_dir)

def remove_data(source, target, env):
    target_dir = os.path.join('data')

    # Remove unneeded path
    shutil.rmtree(target_dir)

partitions_copy()
env.AddPostAction("buildprog", partitions_clear)

env.AddPreAction("$BUILD_DIR/littlefs.bin", copy_data)
env.AddPostAction("$BUILD_DIR/littlefs.bin", remove_data)