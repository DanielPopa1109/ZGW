"""Compatibility entry point for TC375 C export.

The authoritative export reads the schema-v3 PyTorch state_dict directly.
"""
from export_c import main

if __name__ == "__main__":
    main()
