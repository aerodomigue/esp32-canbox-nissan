import subprocess
import os

Import("env")

def get_app_version():
    # 1. Check for environment variable (GitHub Actions)
    version = os.environ.get("APP_VERSION")
    if version:
        return version
    
    # 2. Try to get git hash (Local)
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).strip().decode("utf-8")
    except Exception:
        return "dev"

version = get_app_version()
print(f"\n--- [BUILD INFO] APP_VERSION: {version} ---\n")

# To pass a string to C++ via -D, we must wrap it in triple quotes: '"value"'
# This ensures the compiler receives it as a string literal.
env.Append(CPPDEFINES=[
    ("APP_VERSION", f'\\"{version}\\"')
])
