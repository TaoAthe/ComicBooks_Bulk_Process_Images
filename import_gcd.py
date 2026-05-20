"""
import_gcd.py  —  Convert GCD MySQL dump to a compact local SQLite database.

Reads only three tables from the dump:
  gcd_publisher  →  id, name
  gcd_series     →  id, name, year_began, publisher_id, deleted
  gcd_issue      →  id, series_id, number, publication_date, deleted

Produces:  G:\Big\Comicer\gcd.db
  Tables:
    publisher(id, name)
    series(id, name, year_began, publisher_id)
    issue(id, series_id, number, publication_date)
  Indexes on series.name and issue.number for fast lookup.

Usage:
    python import_gcd.py [path_to_dump.sql]

Default dump path: G:\Big\Comicer\current\2026-04-29.sql
"""

import sqlite3
import sys
import re
import os
import time

DUMP_PATH = r"G:\Big\Comicer\current\2026-04-29.sql"
DB_PATH   = r"G:\Big\Comicer\gcd.db"

WANTED_TABLES = {"gcd_publisher", "gcd_series", "gcd_issue"}

# ── column indices we care about (0-based, parsed from CREATE TABLE) ─────────
COLS = {
    "gcd_publisher": ["id", "name"],
    "gcd_series":    ["id", "name", "year_began", "publisher_id", "deleted"],
    "gcd_issue":     ["id", "series_id", "number", "publication_date", "deleted"],
}

def get_col_order(create_sql):
    """Return ordered list of column names from a CREATE TABLE block."""
    cols = []
    for line in create_sql.split("\n"):
        line = line.strip()
        if line.startswith("`") and not line.startswith("`PRIMARY") and not line.upper().startswith("PRIMARY") \
                and not line.upper().startswith("KEY") and not line.upper().startswith("UNIQUE") \
                and not line.upper().startswith("CONSTRAINT"):
            m = re.match(r"`(\w+)`", line)
            if m:
                cols.append(m.group(1))
    return cols

def parse_values_line(line):
    """Parse a MySQL VALUES(...),(...) line into a list of row tuples."""
    # Strip leading INSERT INTO ... VALUES and trailing semicolon
    m = re.match(r"INSERT INTO `\w+` VALUES\s*(.+);?\s*$", line, re.DOTALL)
    if not m:
        return []
    raw = m.group(1).rstrip(";").strip()

    rows = []
    i = 0
    n = len(raw)
    while i < n:
        if raw[i] != '(':
            i += 1
            continue
        i += 1  # skip '('
        fields = []
        current = []
        in_str = False
        escape = False
        while i < n:
            c = raw[i]
            if escape:
                current.append(c)
                escape = False
            elif c == '\\':
                current.append(c)
                escape = True
            elif c == "'" and not in_str:
                in_str = True
                current.append(c)
            elif c == "'" and in_str:
                in_str = False
                current.append(c)
            elif not in_str and c == ',':
                fields.append(''.join(current).strip())
                current = []
            elif not in_str and c == ')':
                fields.append(''.join(current).strip())
                rows.append(fields)
                i += 1
                break
            else:
                current.append(c)
            i += 1
        # skip comma between rows
        while i < n and raw[i] in (' ', ','):
            i += 1
    return rows

def field_to_value(f):
    """Convert a raw MySQL field string to a Python value."""
    f = f.strip()
    if f.upper() == "NULL":
        return None
    if f.startswith("'") and f.endswith("'"):
        s = f[1:-1]
        s = s.replace("\\'", "'").replace("\\\\", "\\").replace("\\n", "\n").replace("\\r", "\r")
        return s
    try:
        return int(f)
    except ValueError:
        try:
            return float(f)
        except ValueError:
            return f

def main():
    dump_path = sys.argv[1] if len(sys.argv) > 1 else DUMP_PATH
    db_path   = sys.argv[2] if len(sys.argv) > 2 else DB_PATH

    if not os.path.exists(dump_path):
        print(f"ERROR: dump not found at {dump_path}")
        sys.exit(1)

    print(f"Source: {dump_path}  ({os.path.getsize(dump_path)//1024//1024} MB)")
    print(f"Target: {db_path}")

    # ── set up SQLite ─────────────────────────────────────────────────────────
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    cur.executescript("""
        PRAGMA journal_mode=WAL;
        PRAGMA synchronous=NORMAL;

        DROP TABLE IF EXISTS issue;
        DROP TABLE IF EXISTS series;
        DROP TABLE IF EXISTS publisher;

        CREATE TABLE publisher (
            id      INTEGER PRIMARY KEY,
            name    TEXT NOT NULL
        );

        CREATE TABLE series (
            id           INTEGER PRIMARY KEY,
            name         TEXT NOT NULL,
            year_began   INTEGER,
            publisher_id INTEGER
        );

        CREATE TABLE issue (
            id               INTEGER PRIMARY KEY,
            series_id        INTEGER NOT NULL,
            number           TEXT NOT NULL,
            publication_date TEXT
        );

        CREATE INDEX idx_series_name  ON series(name);
        CREATE INDEX idx_issue_num    ON issue(number);
        CREATE INDEX idx_issue_series ON issue(series_id);
    """)
    con.commit()

    # ── stream the dump ────────────────────────────────────────────────────────
    col_order   = {}   # table → [col names in dump order]
    want_idx    = {}   # table → {col_name: index_in_dump_row}
    insert_idx  = {}   # table → {col_name: index_we_want}

    pub_rows    = []
    series_rows = []
    issue_rows  = []

    BATCH = 50000
    current_table = None
    create_buf = []
    in_create = False

    t0 = time.time()
    bytes_read = 0
    file_size = os.path.getsize(dump_path)

    print("Scanning dump… (this will take a few minutes for a 3.5 GB file)")

    with open(dump_path, encoding="utf-8", errors="replace") as f:
        for lineno, line in enumerate(f, 1):
            bytes_read += len(line)

            # Progress every 500k lines
            if lineno % 500000 == 0:
                pct = bytes_read / file_size * 100
                elapsed = time.time() - t0
                print(f"  {pct:.1f}%  ({bytes_read//1024//1024} MB)  {elapsed:.0f}s  "
                      f"pub={len(pub_rows)} series={len(series_rows)} issue={len(issue_rows)}")

            stripped = line.strip()

            # Detect CREATE TABLE for a wanted table
            m = re.match(r"CREATE TABLE `(\w+)`", stripped)
            if m:
                tname = m.group(1)
                if tname in WANTED_TABLES:
                    in_create = True
                    current_table = tname
                    create_buf = [stripped]
                continue

            if in_create:
                create_buf.append(stripped)
                if stripped.startswith(") ENGINE"):
                    in_create = False
                    # Parse column order
                    create_sql = "\n".join(create_buf)
                    col_order[current_table] = get_col_order(create_sql)
                    # Build index maps
                    order = col_order[current_table]
                    want_idx[current_table] = {
                        c: order.index(c) for c in COLS[current_table] if c in order
                    }
                    current_table = None
                continue

            # Detect INSERT INTO for a wanted table
            m = re.match(r"INSERT INTO `(\w+)` VALUES", stripped)
            if not m:
                continue
            tname = m.group(1)
            if tname not in WANTED_TABLES or tname not in want_idx:
                continue

            rows = parse_values_line(stripped)
            idx  = want_idx[tname]

            for raw_row in rows:
                if len(raw_row) <= max(idx.values()):
                    continue
                vals = [field_to_value(raw_row[i]) for i in range(len(raw_row))]

                if tname == "gcd_publisher":
                    pid  = vals[idx["id"]]
                    name = vals[idx["name"]]
                    pub_rows.append((pid, name))
                    if len(pub_rows) >= BATCH:
                        cur.executemany("INSERT OR IGNORE INTO publisher VALUES(?,?)", pub_rows)
                        pub_rows = []

                elif tname == "gcd_series":
                    deleted = vals[idx.get("deleted", -1)] if "deleted" in idx else 0
                    if deleted:
                        continue
                    sid  = vals[idx["id"]]
                    name = vals[idx["name"]]
                    yr   = vals[idx.get("year_began")] if "year_began" in idx else None
                    pub  = vals[idx.get("publisher_id")] if "publisher_id" in idx else None
                    series_rows.append((sid, name, yr, pub))
                    if len(series_rows) >= BATCH:
                        cur.executemany("INSERT OR IGNORE INTO series VALUES(?,?,?,?)", series_rows)
                        series_rows = []

                elif tname == "gcd_issue":
                    deleted = vals[idx.get("deleted", -1)] if "deleted" in idx else 0
                    if deleted:
                        continue
                    iid  = vals[idx["id"]]
                    sid  = vals[idx["series_id"]]
                    num  = vals[idx["number"]]
                    pub_date = vals[idx.get("publication_date")] if "publication_date" in idx else None
                    issue_rows.append((iid, sid, num, pub_date))
                    if len(issue_rows) >= BATCH:
                        cur.executemany("INSERT OR IGNORE INTO issue VALUES(?,?,?,?)", issue_rows)
                        issue_rows = []

    # Flush remaining
    if pub_rows:
        cur.executemany("INSERT OR IGNORE INTO publisher VALUES(?,?)", pub_rows)
    if series_rows:
        cur.executemany("INSERT OR IGNORE INTO series VALUES(?,?,?,?)", series_rows)
    if issue_rows:
        cur.executemany("INSERT OR IGNORE INTO issue VALUES(?,?,?,?)", issue_rows)

    con.commit()

    # ── summary ───────────────────────────────────────────────────────────────
    elapsed = time.time() - t0
    pub_count    = cur.execute("SELECT COUNT(*) FROM publisher").fetchone()[0]
    series_count = cur.execute("SELECT COUNT(*) FROM series").fetchone()[0]
    issue_count  = cur.execute("SELECT COUNT(*) FROM issue").fetchone()[0]
    db_size      = os.path.getsize(db_path) // 1024 // 1024

    print(f"\nDone in {elapsed:.0f}s")
    print(f"  publishers : {pub_count:,}")
    print(f"  series     : {series_count:,}")
    print(f"  issues     : {issue_count:,}")
    print(f"  gcd.db size: {db_size} MB")

    # Quick sanity check
    row = cur.execute("""
        SELECT s.name, s.year_began, p.name, i.publication_date
        FROM series s
        JOIN publisher p ON p.id = s.publisher_id
        JOIN issue i ON i.series_id = s.id
        WHERE s.name LIKE '%Spectacular Spider%' AND i.number = '5'
        LIMIT 1
    """).fetchone()
    if row:
        print(f"\nSample lookup — Spectacular Spider-Man #5:")
        print(f"  Series : {row[0]}  ({row[1]})")
        print(f"  Publisher: {row[2]}")
        print(f"  Cover date: {row[3]}")
    con.close()

if __name__ == "__main__":
    main()
