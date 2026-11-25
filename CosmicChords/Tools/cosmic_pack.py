# Builds the 20 "Cosmic Pack" progressions (chords + top lines) and appends them to library.json.
# Melodies are written in C (tonic = 0); a trailing ' raises an octave, a trailing , lowers one.
import json, re, sys

PC = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6,
      "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}

def line(text):
    notes, t = [], 0.0
    for tok in text.split():
        m = re.fullmatch(r"([A-G][#b]?|r)([',]*)(\d*\.?\d+)", tok)
        assert m, tok
        name, octs, dur = m.group(1), m.group(2), float(m.group(3))
        if name != "r":
            semis = PC[name] + 12 * octs.count("'") - 12 * octs.count(",")
            notes.append([semis, t, dur])
        t += dur
    return notes, t

def chord(spec):
    # "r q" or "r q /bass"
    parts = spec.split()
    c = {"r": parts[0], "q": parts[1] if len(parts) > 1 and not parts[1].startswith("/") else ""}
    for p in parts[1:]:
        if p.startswith("/"):
            c["bass"] = p[1:]
    return c

PACK = [
  ("Nebula Drift", "Major", ["Dreamy", "Romantic"],
   [("I maj9", 1, "G1.5 B.5 D'1 C'1"), ("bVI maj7", 1, "C'1.5 Eb'1 D'.5 G1"),
    ("IV maj9", 1, "A1 G.5 E1.5 G1"), ("iv m6", 1, "Ab1.5 F.5 D2")]),
  ("Orbit of Venus", "Major", ["Romantic", "Happy"],
   [("I maj7", 1, "E'1.5 D'.5 B1 G1"), ("VI 7b9", 1, "C#'1 Bb.5 A.5 G1 E1"),
    ("ii m9", 1, "E'1.5 D'.5 C'1 A1"), ("V 13", 1, "B1 D'.5 E'1.5 F'1"), ("I maj9", 2, "E'2 D'1 B1 C'4")]),
  ("Lydian Satellites", "Lydian", ["Dreamy", "Epic"],
   [("I maj9", 1, "E1 F#.5 G1.5 B1"), ("II add9", 1, "A1 F#1 E1.5 D.5"),
    ("vii m7", 1, "F#1.5 A.5 B1 D'1"), ("II add9 /I", 1, "E'2 D'1 F#1")]),
  ("Dorian Comet", "Dorian", ["Chill", "Dreamy"],
   [("i m9", 1, "Eb'1 D'.5 Bb1.5 G1"), ("IV 9", 1, "A1.5 G.5 Eb1 C1"),
    ("bIII maj7", 1, "G1 Bb.5 D'1.5 C'1"), ("bVII maj7", 1, "D'1 C'.5 A1.5 F1")]),
  ("Event Horizon", "Harmonic Minor", ["Dark", "Tense"],
   [("i mmaj7", 1, "G1 B.5 Eb'1.5 D'1"), ("bVI maj7", 1, "C'1.5 Bb.5 G1 Ab1"),
    ("ii° m7b5", 1, "F1 Ab1 C'1.5 Bb.5"), ("V 7b9", 1, "B1.5 Ab.5 F1 D1")]),
  ("Supernova Gospel", "Major", ["Epic", "Happy"],
   [("IV maj7", 1, "A1 C'1 E'1.5 D'.5"), ("#iv° dim7", 1, "Eb'2 C'1 A1"),
    ("I  /V", 1, "G1 C'1 E'1.5 D'.5"), ("VI 7", 1, "C#'1 A1 G1 E1"), ("ii m9", 1, "F1 A1 C'1 E'1"),
    ("V 7sus4", 1, "F'1.5 D'.5 C'2"), ("I add9", 2, "D'1 E'3 C'4")]),
  ("Neo-Soul Moonwalk", "Major", ["Romantic", "Chill"],
   [("ii m9", 1, "A.5 C'.5 E'1.5 D'.5 C'1"), ("V 13", 1, "F1 E1.5 D.5 B,1"),
    ("iii m7", 0.5, "D'1 B1"), ("VI 7b9", 0.5, "Bb1 C#'1"), ("ii m9", 1, "E'1.5 D'.5 C'1 A1"),
    ("bII 7", 1, "Ab1.5 F.5 Eb1 Db1"), ("I maj9", 2, "E2 G1 B1 D'4")]),
  ("Gravity Well", "Phrygian", ["Dark", "Tense"],
   [("i m7", 1, "G1 Bb.5 C'2 Db'.5"), ("bII maj7", 1, "C'1.5 Ab.5 F1 Db1"),
    ("iv m7", 1, "Eb'1 C'1 Ab1.5 G.5"), ("bII maj7 /I", 1, "F1 Eb.5 Db1.5 C1")]),
  ("Starlight Bossa", "Major", ["Romantic", "Chill"],
   [("I maj9", 1, "E1 D.5 E1 G.5 B1"), ("#i° dim7", 1, "Bb1.5 G.5 E2"),
    ("ii m9", 1, "A1 G.5 A.5 C'1 E'1"), ("#ii° dim7", 1, "C'1.5 A.5 F#2"),
    ("iii m7", 1, "G1 B1 D'2"), ("VI 7b9", 1, "Bb1.5 G.5 E1 C#1"),
    ("ii m9", 1, "D1 F1 A1 C'1"), ("V 13", 1, "F1 E1 D1 B,1")]),
  ("Andromeda Rise", "Major", ["Epic", "Dreamy"],
   [("I", 1, "E1 G1 C'2"), ("III", 1, "B1.5 G#.5 E'2"), ("vi m", 1, "E'1.5 C'.5 A2"),
    ("bVI", 1, "Eb'1.5 C'.5 Ab2"), ("IV", 1, "C'1 A1 F1 A1"), ("iv m", 1, "C'1 Ab2 G1"),
    ("I add9", 2, "D'2 E'2 C'4")]),
  ("Midnight Quasar", "Minor", ["Melancholic", "Chill"],
   [("i m9", 1, "D'1.5 Eb'.5 Bb1 G1"), ("iv m9", 1, "G1 Ab1 C'1 Eb'1"),
    ("bVII 13", 1, "D'1 C'.5 Ab1.5 G1"), ("bIII maj7", 1, "G1.5 Bb.5 D'2"),
    ("bVI maj7", 1, "G1 C'1 Eb'2"), ("ii° m7b5", 0.5, "F1 Ab1"), ("V 7b9", 0.5, "Ab1 B,1")]),
  ("Neapolitan Night", "Harmonic Minor", ["Dark", "Melancholic"],
   [("i mmaj7", 1, "Eb'1 D'.5 B1.5 G1"), ("bVI maj7", 1, "C'1.5 Bb.5 G2"),
    ("iv m6", 1, "Ab1 F1 D1 C1"), ("bII maj7", 0.5, "Db'1 C'1"), ("V 7b9", 0.5, "B1 Ab1")]),
  ("Pulsar Groove", "Minor", ["Chill", "Dreamy"],
   [("i m11", 1, "G.5 Bb.5 C'1 D'1 Bb1"), ("bVI maj9", 1, "C'1.5 Bb.5 G1 Eb1"),
    ("bIII 6", 1, "G1 C'1 Bb1.5 G.5"), ("bVII add9", 1, "D'1 C'1 Bb1 F1")]),
  ("Saturn's Rings", "Mixolydian", ["Happy", "Chill"],
   [("I 9", 1, "Bb1.5 G.5 E1 D1"), ("v m7", 1, "F1 G.5 Bb1.5 D'1"),
    ("IV maj7", 1, "E'1.5 C'.5 A1 F1"), ("bVII 6", 1, "G1 F.5 D1.5 Bb,1")]),
  ("Interstellar Ballad", "Major", ["Romantic", "Melancholic"],
   [("I maj7", 1, "E1 G1 B1.5 C'.5"), ("V  /VII", 1, "D'2 B1 G1"), ("vi m7", 1, "C'1.5 B.5 A1 E1"),
    ("vi m7 /V", 1, "G1 A1 C'1 E'1"), ("IV maj7", 1, "E'2 C'1 A1"), ("iv m6", 1, "Ab1.5 G.5 F2"),
    ("I  /V", 1, "E1 G1 C'2"), ("V 7sus4", 1, "F1 D1 C2")]),
  ("Photon Funk", "Dorian", ["Happy", "Chill"],
   [("i m9", 0.5, "Bb.5 C'.5 Eb'1"), ("IV 13", 0.5, "D'1 C'1"), ("i m9", 0.5, "Bb.5 G.5 Eb1"),
    ("IV 13", 0.5, "A1 C'1"), ("bIII maj7", 1, "D'1.5 Bb.5 G1 F1"), ("bVII 6", 1, "G1 F1 D1.5 C.5")]),
  ("Celestial Choir", "Minor", ["Epic", "Melancholic"],
   [("i m9", 1, "G1 Bb1 C'1 D'1"), ("bVI maj7", 1, "Eb'2 C'1 G1"), ("bIII add9", 1, "F1.5 G.5 Bb2"),
    ("bVII sus2", 1, "C'1 Bb1 F2"), ("iv m9", 2, "Ab2 G1 F1 Eb2 G2"), ("I", 2, "E4 C'4")]),
  ("Retrograde Motion", "Major", ["Tense", "Romantic"],
   [("I maj7", 1, "E'1.5 D'.5 B1 G1"), ("bIII 7", 1, "G1 Bb1 Db'2"), ("bVI maj7", 1, "C'1.5 Bb.5 G2"),
    ("VII 7", 1, "A1 F#1 D#2"), ("III maj7", 1, "G#1.5 B.5 D#'2"), ("V 7b9", 1, "F1 Ab1 B,1 D1"),
    ("I maj9", 2, "E2 D1 E1 G4")]),
  ("Red Giant Blues", "Mixolydian", ["Happy", "Tense"],
   [("I 9", 1, "G.5 Bb.5 C'1 Eb'.5 E'1.5"), ("IV 9", 1, "Eb'1 C'1 A1 G1"),
    ("I 9", 1, "E'1.5 D'.5 C'1 Bb1"), ("#iv° dim7", 1, "Eb'1 C'1 A1 F#1"),
    ("I  /V", 1, "G1 E1 C2"), ("VI 7b9", 1, "C#'1 Bb1 G1 E1"),
    ("ii m7", 1, "F1 A1 C'1 D'1"), ("V 13", 1, "E'1.5 F'.5 D'1 B1")]),
  ("Dark Matter", "Harmonic Minor", ["Dark", "Tense"],
   [("i m", 1, "G1 Eb1 C'1.5 B.5"), ("bII", 1, "Ab1.5 F.5 Db'2"),
    ("bVI maj7", 1, "G1 C'1 Eb'1 C'1"), ("vii° dim7", 1, "Ab1 F1 D1 B,1")]),
]

def build():
    out = []
    for i, (name, scale, moods, chords) in enumerate(PACK, 1):
        cs, bars, top = [], [], []
        for spec, length, mel in chords:
            cs.append(chord(spec))
            bars.append(length)
            notes, total = line(mel)
            assert abs(total - length * 4) < 1e-9, (name, spec, total, length)
            top.append(notes)
        out.append({"id": "cosmic-%03d" % i, "name": name, "genre": "Cosmic Pack", "moods": moods,
                    "scale": scale, "chords": cs, "bars": bars, "topline": top})
    assert len(out) == 20
    return out

if __name__ == "__main__":
    path = sys.argv[1]
    lib = json.load(open(path))
    lib = [e for e in lib if e.get("genre") != "Cosmic Pack"]
    pack = build()
    rows = [json.dumps(e, ensure_ascii=False) for e in pack + lib]
    open(path, "w").write("[\n  " + ",\n  ".join(rows) + "\n]\n")
    print("wrote", len(pack), "pack entries,", len(lib), "others")
