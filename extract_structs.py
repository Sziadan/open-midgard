import sys
import os

def extract_struct(target, content):
    import re
    # Match struct __cppobj Target or const struct __cppobj Target
    # Relaxed pattern to find the start and end of the struct
    pattern = r'(?:const\s+)?(?:struct|class)\s+(?:__cppobj\s+)?' + re.escape(target) + r'\b[^{]*\{.*?\n\};'
    match = re.search(pattern, content, re.DOTALL)
    if match:
        return match.group(0)
    return f"--- {target} NOT FOUND ---"

if __name__ == "__main__":
    targets = sys.argv[1:]
    with open('HighPriest.exe.utf8.h', 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    for t in targets:
        print(f"[{t}]")
        print(extract_struct(t, content))
        print("-" * 40)
