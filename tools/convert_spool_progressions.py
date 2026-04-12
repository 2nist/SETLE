#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPOOL_ROOT = ROOT / "external" / "Spool-main" / "exported_progressions"
OUT_ROOT = ROOT / "assets" / "progressions"

DATASETS = {
    "when_in_rome_classical_curated": "Classical",
    "uspop2002": "Pop",
    "mattssongs": "Folk",
}

LIMITS = {
    "when_in_rome_classical_curated": 30,
    "uspop2002": 5,
    "mattssongs": 8,
}

NOTE_TO_PC = {
    "C": 0, "C#": 1, "DB": 1,
    "D": 2, "D#": 3, "EB": 3,
    "E": 4, "F": 5, "F#": 6, "GB": 6,
    "G": 7, "G#": 8, "AB": 8,
    "A": 9, "A#": 10, "BB": 10,
    "B": 11,
}


def parse_root(symbol: str) -> int:
    if not symbol:
        return 60
    token = symbol[0].upper()
    if len(symbol) > 1 and symbol[1] in ("#", "b"):
        token += symbol[1].upper()
    pc = NOTE_TO_PC.get(token, 0)
    return 60 + pc


def detect_quality(symbol: str, chord_type: str) -> str:
    s = (symbol or "").lower() + " " + (chord_type or "").lower()
    if "dim" in s or "o" in s:
        return "diminished"
    if "aug" in s or "+" in s:
        return "augmented"
    if "maj7" in s:
        return "major7"
    if "7" in s:
        return "dominant7"
    if "min" in s or s.startswith("m"):
        return "minor"
    return "major"


def convert_file(path: Path, category: str) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    progression = (data.get("progressions") or [{}])[0]
    name = progression.get("name") or data.get("catalog", {}).get("title") or path.stem

    chords = []
    for event in progression.get("events", []):
        symbol = event.get("x-chordLiteral") or event.get("romanNumeral") or event.get("root") or "C"
        quality = detect_quality(symbol, event.get("type", ""))
        chords.append(
            {
                "symbol": symbol,
                "quality": quality,
                "rootMidi": parse_root(symbol),
                "durationBeats": float(event.get("durationBeats", 1.0)),
            }
        )

    return {
        "schema": "setle.progression.v1",
        "name": name,
        "category": category,
        "source": path.name,
        "chords": chords,
    }


def main() -> None:
    total = 0
    for dataset, category in DATASETS.items():
        in_dir = SPOOL_ROOT / dataset / "progressions"
        out_dir = OUT_ROOT / dataset
        out_dir.mkdir(parents=True, exist_ok=True)
        for old in out_dir.glob("*.setle-prg"):
            old.unlink()

        count = 0
        limit = LIMITS.get(dataset, 10_000)
        for src in sorted(in_dir.glob("*.spl.prg"))[:limit]:
            converted = convert_file(src, category)
            out_name = src.stem.replace(".spl", "") + ".setle-prg"
            (out_dir / out_name).write_text(json.dumps(converted, indent=2), encoding="utf-8")
            count += 1

        total += count
        print(f"{dataset}: {count}")

    print(f"total: {total}")


if __name__ == "__main__":
    main()
