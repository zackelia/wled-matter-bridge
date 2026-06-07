import sys
import subprocess
from pathlib import Path

if len(sys.argv) < 2:
    print("Usage: generate_version_header.py <output_path>")
    sys.exit(1)

output_path = Path(sys.argv[1])
git_tag = subprocess.check_output(['git', 'describe', '--tags']).decode().strip()

Path(output_path).parent.mkdir(exist_ok=True)
output_path.write_text("#pragma once\n")
output_path.write_text(f'#define CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING "{git_tag}"')
output_path.write_text(f'#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING "{git_tag}"')
