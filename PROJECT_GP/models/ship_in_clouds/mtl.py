#!/usr/bin/env python3
"""
Fixes MTL files by converting Ke (emission) to Kd (diffuse)
Run this after exporting from Blender
"""

import sys
import os
import shutil

def fix_mtl_file(mtl_path):
    """
    Reads an MTL file and converts Ke values to Kd values
    """
    
    if not os.path.exists(mtl_path):
        print(f"Error: {mtl_path} not found!")
        return False
    
    # Backup original
    backup_path = mtl_path + ".backup"
    if not os.path.exists(backup_path):
        shutil.copy2(mtl_path, backup_path)
        print(f"Created backup: {backup_path}")
    
    # Read file
    with open(mtl_path, 'r') as f:
        lines = f.readlines()
    
    # Process
    new_lines = []
    current_material = None
    materials_fixed = []
    
    for line in lines:
        stripped = line.strip()
        
        # Track current material
        if stripped.startswith('newmtl '):
            current_material = stripped.replace('newmtl ', '')
        
        # Check for emission (Ke)
        if stripped.startswith('Ke '):
            parts = stripped.split()
            if len(parts) >= 4:
                r, g, b = parts[1], parts[2], parts[3]
                
                # Check if it's not black
                if float(r) > 0.01 or float(g) > 0.01 or float(b) > 0.01:
                    # Replace with Kd (diffuse)
                    new_lines.append(f"Kd {r} {g} {b}\n")
                    new_lines.append(f"# Original: {line}")
                    
                    if current_material and current_material not in materials_fixed:
                        materials_fixed.append(current_material)
                    continue
        
        # Check for black diffuse (Kd 0 0 0)
        elif stripped.startswith('Kd '):
            parts = stripped.split()
            if len(parts) >= 4:
                r, g, b = float(parts[1]), float(parts[2]), float(parts[3])
                
                # If black, replace with gray
                if r < 0.01 and g < 0.01 and b < 0.01:
                    new_lines.append("Kd 0.8 0.8 0.8  # Was black, set to gray\n")
                    if current_material and current_material not in materials_fixed:
                        materials_fixed.append(current_material)
                    continue
        
        # Keep original line
        new_lines.append(line)
    
    # Write fixed file
    with open(mtl_path, 'w') as f:
        f.writelines(new_lines)
    
    print(f"\n{'='*60}")
    print(f"Fixed {len(materials_fixed)} materials:")
    for mat in materials_fixed:
        print(f"  - {mat}")
    print(f"{'='*60}\n")
    
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python fix_mtl.py <path_to_file.mtl>")
        print("\nExample:")
        print("  python fix_mtl.py models/island/ship_in_clouds.mtl")
        sys.exit(1)
    
    mtl_path = sys.argv[1]
    fix_mtl_file(mtl_path)
    print("Done! Your MTL file should now work in OpenGL.")