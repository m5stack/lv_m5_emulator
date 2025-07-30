#!/usr/bin/env python3
"""
RISC-V ARM File Cleanup Script for ESP32-P4
Removes ARM-specific assembly files from LVGL library after installation
"""

import os
import shutil

Import("env")

def remove_arm_files_from_lvgl():
    """Remove ARM assembly files from LVGL for RISC-V compatibility"""
    
    project_dir = env.subst("$PROJECT_DIR")
    board_env = env.subst("$PIOENV") 
    lvgl_lib_dir = os.path.join(project_dir, ".pio", "libdeps", board_env, "lvgl")
    
    print("=" * 60)
    print("RISC-V ARM File Cleanup")
    print("=" * 60)
    print(f"Target: {lvgl_lib_dir}")
    
    if not os.path.exists(lvgl_lib_dir):
        print("⚠️  LVGL library not found - cleanup will run after installation")
        return False
    
    # ARM-specific paths to remove
    arm_paths = [
        os.path.join(lvgl_lib_dir, "src", "draw", "sw", "blend", "helium"),
        os.path.join(lvgl_lib_dir, "src", "draw", "sw", "blend", "neon"),
        os.path.join(lvgl_lib_dir, "src", "draw", "sw", "blend", "arm2d"),
        os.path.join(lvgl_lib_dir, "src", "draw", "sw", "arm2d"),
    ]
    
    removed_items = 0
    
    # Remove ARM directories
    for arm_path in arm_paths:
        if os.path.exists(arm_path):
            rel_path = os.path.relpath(arm_path, lvgl_lib_dir)
            print(f"🗑️  Removing: {rel_path}")
            try:
                shutil.rmtree(arm_path)
                print(f"✅ Success: {rel_path}")
                removed_items += 1
            except Exception as e:
                print(f"❌ Failed: {rel_path} - {e}")
    
    # Remove ARM assembly files
    for root, dirs, files in os.walk(lvgl_lib_dir):
        for file in files:
            if file.endswith('.S') and any(kw in file.lower() for kw in ['helium', 'neon', 'arm2d']):
                file_path = os.path.join(root, file)
                rel_path = os.path.relpath(file_path, lvgl_lib_dir)
                print(f"🗑️  Removing: {rel_path}")
                try:
                    os.remove(file_path)
                    print(f"✅ Success: {rel_path}")
                    removed_items += 1
                except Exception as e:
                    print(f"❌ Failed: {rel_path} - {e}")
    
    print(f"🎯 Cleanup completed: {removed_items} items removed")
    print("=" * 60)
    return True

def post_lib_deps_action(source, target, env):
    """Called after library dependencies are processed"""
    if remove_arm_files_from_lvgl():
        # Force rebuild of library after cleaning
        build_dir = env.subst("$BUILD_DIR")
        # Find all LVGL library build directories dynamically
        if os.path.exists(build_dir):
            for item in os.listdir(build_dir):
                lib_path = os.path.join(build_dir, item, "lvgl")
                if os.path.exists(lib_path) and item.startswith("lib"):
                    print(f"🧹 Cleaning compiled ARM objects: {lib_path}")
                    shutil.rmtree(lib_path)

def pre_build_action(source, target, env):
    """Called before building starts"""
    build_dir = env.subst("$BUILD_DIR")
    
    # Remove ARM files from source
    remove_arm_files_from_lvgl()
    
    # Remove any compiled ARM objects from all library build directories
    if os.path.exists(build_dir):
        for item in os.listdir(build_dir):
            if item.startswith("lib"):
                lib_build_dir = os.path.join(build_dir, item, "lvgl")
                if os.path.exists(lib_build_dir):
                    for root, dirs, files in os.walk(lib_build_dir):
                        for file in files:
                            if file.endswith('.o') and any(kw in file.lower() for kw in ['helium', 'neon', 'arm2d']):
                                file_path = os.path.join(root, file)
                                print(f"🗑️  Removing compiled ARM object: {os.path.relpath(file_path, build_dir)}")
                                try:
                                    os.remove(file_path)
                                    print(f"✅ Success")
                                except Exception as e:
                                    print(f"❌ Failed: {e}")

# Run immediately when script loads
print("🚀 Running initial ARM cleanup...")
remove_arm_files_from_lvgl()

# Register hooks for different build stages
print("🔧 Registering ARM cleanup hooks...")

# Use the correct hook timing according to PlatformIO docs
# This runs after dependencies are resolved but before compilation
def clean_arm_files_before_build(source, target, env):
    """Clean ARM files and compiled objects before build starts"""
    print("🔄 Pre-build ARM cleanup...")
    
    # Clean source files
    remove_arm_files_from_lvgl()
    
    # Clean any existing compiled objects from all library build directories
    build_dir = env.subst("$BUILD_DIR")
    
    if os.path.exists(build_dir):
        for item in os.listdir(build_dir):
            if item.startswith("lib"):
                lib_build_dir = os.path.join(build_dir, item, "lvgl")
                if os.path.exists(lib_build_dir):
                    print(f"🧹 Cleaning compiled ARM objects from: {lib_build_dir}")
                    # Remove the entire LVGL build directory to force recompilation
                    try:
                        shutil.rmtree(lib_build_dir)
                        print(f"✅ Removed compiled LVGL objects from {item} - will recompile without ARM files")
                    except Exception as e:
                        print(f"❌ Failed to remove build directory {lib_build_dir}: {e}")

# Hook at the earliest possible stage - when checking program size
env.AddPreAction("checkprogsize", clean_arm_files_before_build)

# Hook before building the program
env.AddPreAction("buildprog", clean_arm_files_before_build)

# Hook before building libraries  
env.AddPreAction("buildlib", clean_arm_files_before_build)

print("✅ ARM cleanup system ready for ESP32-P4 RISC-V build")
