"""Regenerate the checked-in Vulkan world fragment shader using DXC."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dxc",default=shutil.which("dxc"))
args=parser.parse_args()
if not args.dxc:parser.error("Provide --dxc pointing to the Vulkan SDK's dxc executable")
with tempfile.TemporaryDirectory() as directory:
    output=Path(directory)/"world-fragment.spv"
    subprocess.run([args.dxc,"-spirv","-T","ps_6_0","-E","PSMain","-O3",
                    "-fspv-target-env=vulkan1.0","-fvk-use-dx-layout",
                    str(root/"src/render3d/shaders/vulkan_world.hlsl"),"-Fo",str(output)],check=True)
    validator=shutil.which("spirv-val")
    if validator:subprocess.run([validator,"--target-env","vulkan1.0",str(output)],check=True)
    data=output.read_bytes()
    lines=["    "+", ".join(f"0x{byte:02X}" for byte in data[i:i+12])+"," for i in range(0,len(data),12)]
    replacement="static const uint8_t kVulkanPsSpirv[] = {\n"+"\n".join(lines)+"\n};"
    header=root/"src/render3d/VulkanShaders.generated.h"
    text,count=re.subn(r"static const uint8_t kVulkanPsSpirv\[\] = \{.*?\n\};",replacement,header.read_text(),flags=re.S)
    if count!=1:raise RuntimeError("Expected exactly one fragment shader in generated header")
    header.write_text(text)
