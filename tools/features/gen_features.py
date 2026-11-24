#!/usr/bin/env python3
"""Generate docs/FEATURES.md: every labelled control in deskHPSDR's menus and
every action, from the upstream source. Run from the repo root:
    python3 -I tools/features/gen_features.py > docs/FEATURES.new.md
then diff against docs/FEATURES.md and carry new items over by hand (the
real file holds ticks and "not doing" notes, so never overwrite it blindly).
"""
import re, subprocess, sys

SRC = 'src'
SECTIONS = {
    'Receive operating (M4)': ['vfo', 'band', 'bandstack', 'mode', 'filter', 'agc', 'meter', 'store', 'rx'],
    'Receive DSP and display (M3/M5)': ['noise', 'equalizer', 'display', 'fft', 'screen', 'diversity', 'ddc',
                                        'toolbar', 'switch'],
    'Hardware: HL2, IO board, HL2+, PA, filters (M6)': ['radio', 'ant', 'oc', 'pa', 'xvtr'],
    'Transmit (M7, only when Randal asks)': ['tx', 'cw', 'vox', 'ps'],
    'Station integration (M8)': ['rigctl', 'midi', 'extras'],
    'Probably not on Android (decide per item)': ['saturn', 'exit', 'about', 'new'],
}
STR = r'"((?:[^"\\]|\\.)+)"'
PATTERNS = [
    (re.compile(r'gtk_(?:label_new|check_button_new_with_label|button_new_with_label|'
                r'radio_button_new_with_label(?:_from_widget)?|toggle_button_new_with_label|frame_new)'
                r'\s*\((?:[^"()]*,\s*)?' + STR), ''),
    (re.compile(r'gtk_label_set_(?:markup|text)\s*\([^,]*,\s*' + STR), ''),
    (re.compile(r'snprintf\s*\(\s*\w*label\w*\s*,[^,]*,\s*' + STR), ''),
    (re.compile(r'gtk_combo_box_text_append(?:_text)?\s*\([^"]*' + STR + r'\s*\)'), '  _(choice)_'),
]


def clean(s):
    s = s.replace('\\n', ' ').replace('\\"', '"')
    s = re.sub(r'<[^>]+>', '', s)                      # markup
    s = re.sub(r'\s*(RX|TX)?\s*%[-0-9.]*[dsfu]', '', s)  # "RX%d", "%d"
    return re.sub(r'\s+', ' ', s).strip(' :')


def main():
    up = subprocess.run(['git', 'rev-parse', '--short', 'upstream/master'],
                        capture_output=True, text=True).stdout.strip() or '?'
    out = [f'''# deskHPSDR feature checklist

Every labelled control in deskHPSDR's menus and every action, **generated
from the source** (upstream `{up}`, `src/*_menu.c`, `src/actions.c`) by
`tools/features/gen_features.py`, so nothing is forgotten while the phone UI
is designed section by section.

- Tick an item when droidHPSDR has it **and** it was checked on the radio.
- "not doing: <why>" for anything Randal decides to drop.
- Some labels are headings or dialog buttons, not features: mark them
  "heading" and tick them the first time the section is worked on.
- After an upstream update, regenerate into a new file and diff (see the
  script's docstring); never overwrite this file, it holds the ticks.

Which milestone each section belongs to: docs/ROADMAP.md.
''']
    total = 0
    for sec, menus in SECTIONS.items():
        out.append(f'\n## {sec}\n')
        for m in menus:
            text = open(f'{SRC}/{m}_menu.c', encoding='utf-8', errors='replace').read()
            items, seen = [], set()
            for m_ in sorted((mm.start(), mm.group(1), tag) for rx, tag in PATTERNS for mm in rx.finditer(text)):
                s = clean(m_[1])
                if len(s) < 2 or s in seen or s in ('Close', 'OK', 'Cancel'):
                    continue
                seen.add(s)
                items.append(f'- [ ] {s}{m_[2]}')
            out.append(f'\n### {m.capitalize()} menu (`src/{m}_menu.c`)\n')
            out += items
            total += len(items)
    text = open(f'{SRC}/actions.c', encoding='utf-8').read()
    tab = text[text.index('ACTION_TABLE ActionTable[] = {'):]
    tab = tab[:tab.index('};')]
    acts = [clean(a) for a in re.findall(r'\{\s*[A-Z0-9_]+,\s*' + STR, tab)]
    out.append('\n## Actions (toolbar buttons, MIDI, shortcuts; `src/actions.c`)\n')
    out.append('Each needs a home in the phone UI (button, gesture, slider, menu entry) or a "not doing".\n')
    for a in acts:
        if a != 'None':
            out.append(f'- [ ] {a}')
            total += 1
    print('\n'.join(out))
    print(f'{total} items', file=sys.stderr)


if __name__ == '__main__':
    main()
