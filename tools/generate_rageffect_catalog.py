"""Regenerate the effect .str catalog.

Asset names come from HighPriest.exe. This entry point is kept so existing
commands still work; the implementation lives in generate_skill_tables.py.
There is no manual or fuzzy name map.
"""

from generate_skill_tables import main


if __name__ == "__main__":
    main()
