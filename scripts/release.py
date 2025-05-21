#!/usr/bin/env python3
"""
Release script for creating a new version.
Updates VERSION file, commits the change, creates a tag, and pushes to remote.
"""
import re
import subprocess
import sys

# Set to True for dry run, False for actual execution
DRY_RUN = False

def check_current_branch():
    """Verify that we are on the main branch"""
    try:
        result = subprocess.run("git rev-parse --abbrev-ref HEAD",
                                shell=True, check=True,
                                stdout=subprocess.PIPE,
                                universal_newlines=True)
        current_branch = result.stdout.strip()
        if current_branch != "main":
            print(f"Error: Not on main branch. Current branch is '{current_branch}'")
            return False
        return True
    except subprocess.CalledProcessError:
        print("Error: Failed to determine current branch")
        return False

def check_git_status():
    """Verify that the working directory is clean"""
    try:
        result = subprocess.run("git status --porcelain", shell=True, check=True,
                                stdout=subprocess.PIPE, universal_newlines=True)
        if result.stdout:
            print("Error: Working directory is not clean. Please commit or stash changes.")
            return False
        return True
    except subprocess.CalledProcessError:
        print("Error: Failed to check git status")
        return False

def check_version_exists(version):
    """Check if version tag already exists"""
    tag = f"v{version}"
    try:
        result = subprocess.run(f"git tag -l {tag}",
                                shell=True, check=True,
                                stdout=subprocess.PIPE,
                                universal_newlines=True)
        if result.stdout.strip():
            print(f"Error: Version {version} (tag {tag}) already exists")
            return True
        return False
    except subprocess.CalledProcessError:
        print("Error: Failed to check existing tags")
        return True

def get_current_version():
    """Read the current version from the VERSION file"""
    try:
        with open('VERSION', 'r') as f:
            return f.read().strip()
    except IOError:
        print("Error: VERSION file not found or unreadable")
        return None

def update_version_file(version):
    """Write the version to the VERSION file"""
    if DRY_RUN:
        print(f"DRY RUN: Would update VERSION file with {version}")
        return True

    try:
        with open('VERSION', 'w') as f:
            f.write(version)
        print(f"Updated VERSION file with {version}")
        return True
    except IOError as e:
        print(f"Error updating VERSION file: {e}")
        return False

def validate_version(version):
    """Validate that the version string has the format x.y.z"""
    if not re.match(r'^\d+\.\d+\.\d+$', version):
        print(f"Error: Version '{version}' does not match the format x.y.z")
        return False
    return True

def run_command(command, description):
    """Run a shell command and print the result"""
    print(f"Running: {command}")
    if DRY_RUN:
        print("DRY RUN: Command not executed.")
        return True
    try:
        result = subprocess.run(command, shell=True, check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
        print(f"{description}: Success")
        if result.stdout:
            print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"{description}: Failed")
        print(f"Error: {e}")
        print(f"Output: {e.stdout}")
        print(f"Error output: {e.stderr}")
        return False

def main():
    if DRY_RUN:
        print("Running in DRY RUN mode - no changes will be made")

    # Check if we're on main branch
    if not check_current_branch():
        sys.exit(1)

    # Check git status first
    if not check_git_status():
        sys.exit(1)

    # Get and validate current version
    current_version = get_current_version()
    if current_version:
        print(f"Current version: {current_version}")

    # Get version from user
    version = input("Enter the version number (e.g. 1.2.3): ").strip()

    # Validate version format
    if not validate_version(version):
        sys.exit(1)

    # Check if version already exists
    if check_version_exists(version):
        sys.exit(1)

    # Update VERSION file
    if not update_version_file(version):
        sys.exit(1)

    # Git operations
    tag = f"v{version}"

    # Stage VERSION file and commit
    if not run_command("git add VERSION", "Staging VERSION file"):
        sys.exit(1)

    if not run_command(f"git commit -m 'Release {tag}'", "Committing changes"):
        sys.exit(1)

    # Create tag
    if not run_command(f"git tag {tag}", f"Creating tag {tag}"):
        sys.exit(1)

    # Push to origin
    if not run_command("git push origin main", "Pushing to main branch"):
        sys.exit(1)

    if not run_command(f"git push origin {tag}", f"Pushing tag {tag}"):
        sys.exit(1)

    print(f"\nRelease {version} completed successfully!")

if __name__ == "__main__":
    main()
