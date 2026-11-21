#!/usr/bin/env python3
"""Builds docs/TSS-1 Manual.pdf and docs/TSS-1 Update History.pdf (HTML printed with headless Chrome).

    python3 docs/manual/build_docs.py

Chapters may share a page but never break across pages: each chapter is kept whole
(break-inside: avoid) and moves to the next page when it does not fit in the space left.
"""
import base64, re, shutil, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
DOCS = ROOT / "docs"
VERSION = re.search(r"project\(TSS1 VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text()).group(1)
sys.path.insert(0, str(ROOT / "tools"))


def img(name):
    return "data:image/jpeg;base64," + base64.b64encode((HERE / "img" / name).read_bytes()).decode()


def presets():
    src = (ROOT / "Source" / "Presets.h").read_text()
    rows, group = [], ""
    for line in src.splitlines():
        m = re.match(r"\s*//\s*(.+)", line)
        if m and "{" not in line and m.group(1).strip().lower() in ("retro computing", "cinematic ai", "modern and technical", "character"):
            group = m.group(1).strip().upper()
        m = re.match(r'\s*\{\s*"(\w+)",\s*"([^"]+)",\s*"([^"]+)",\s*"(\w+)",\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}', line)
        if m:
            rows.append((group,) + m.groups())
    return rows


LOGO = """<svg viewBox="0 0 40 40" width="{s}" height="{s}"><polygon points="20,2 35.6,11 35.6,29 20,38 4.4,29 4.4,11"
 fill="none" stroke="{c}" stroke-width="2.6"/><text x="20" y="25.5" font-family="Helvetica,Arial" font-weight="700"
 font-size="14" text-anchor="middle" fill="{c}">TR</text></svg>"""

CSS = """
@page { size: A4; margin: 16mm 15mm 18mm 15mm; }
* { box-sizing: border-box; }
body { font-family: "Helvetica Neue", Helvetica, Arial, sans-serif; font-size: 10pt; line-height: 1.45; color: #2b2820; margin: 0; }
.cover { height: 262mm; display: flex; flex-direction: column; justify-content: space-between; break-after: page;
         background: #e3dac3; border-radius: 6mm; padding: 14mm 14mm 12mm; }
.cover .brand { display: flex; align-items: center; gap: 4mm; font-weight: 700; letter-spacing: 3px; font-size: 12pt; color: #4a4232; }
.cover h1 { font-size: 64pt; margin: 0; letter-spacing: 2px; color: #3a3428; }
.cover h2 { font-size: 15pt; margin: 2mm 0 8mm; letter-spacing: 5px; font-weight: 700; color: #4a4232; }
.cover .crt { background: #06180c; color: #7dffa0; font-family: Menlo, Consolas, "DejaVu Sans Mono", monospace; font-weight: 700;
              border-radius: 5mm; padding: 7mm 8mm; font-size: 11pt; line-height: 1.7; border: 3mm solid #3b342a; }
.cover .crt .dim { color: #3fcf6c; }
.cover img { width: 100%; border-radius: 3mm; box-shadow: 0 2mm 6mm rgba(0,0,0,.25); }
.cover .foot { display: flex; justify-content: space-between; font-size: 9pt; color: #4a4232; letter-spacing: 1px; }
section { break-inside: avoid; page-break-inside: avoid; margin: 0 0 7mm; }
h3 { font-family: Menlo, Consolas, "DejaVu Sans Mono", monospace; font-size: 12.5pt; margin: 0 0 2.5mm; padding: 2mm 3mm;
     background: #06180c; color: #7dffa0; border-radius: 2mm; letter-spacing: 1px; }
h3 .n { color: #3fcf6c; margin-right: 3mm; }
h4 { font-size: 10.5pt; margin: 3mm 0 1mm; color: #3a3428; }
p { margin: 0 0 2mm; }
ul { margin: 0 0 2mm; padding-left: 5mm; } li { margin-bottom: 1mm; }
.k { font-family: Menlo, Consolas, "DejaVu Sans Mono", monospace; font-weight: 700; font-size: 9pt; background: #e8e0cb;
     padding: 0 1.2mm; border-radius: 1mm; color: #2b2820; white-space: nowrap; }
.shot { width: 100%; border-radius: 2mm; box-shadow: 0 1mm 3mm rgba(0,0,0,.2); margin: 1mm 0 2mm; }
.half { width: 49%; }
.row { display: flex; gap: 2%; align-items: flex-start; }
table { width: 100%; border-collapse: collapse; font-size: 9pt; margin: 1mm 0 2mm; }
th { text-align: left; background: #e3dac3; padding: 1.4mm 2mm; font-size: 8.5pt; letter-spacing: .5px; }
td { padding: 1.1mm 2mm; border-bottom: .3mm solid #e3dac3; vertical-align: top; }
td.c, th.c { text-align: center; }
tr.g td { background: #06180c; color: #7dffa0; font-family: Menlo, Consolas, monospace; font-weight: 700; font-size: 8.5pt; letter-spacing: 1px; }
.note { background: #fff4d6; border-left: 1.5mm solid #e0a92e; padding: 2.5mm 3.5mm; border-radius: 1mm; margin: 2mm 0; }
.tip { background: #e6f7ea; border-left: 1.5mm solid #3fcf6c; padding: 2.5mm 3.5mm; border-radius: 1mm; margin: 2mm 0; }
.callouts { counter-reset: c; columns: 2; column-gap: 6mm; padding-left: 0; list-style: none; }
.callouts li { break-inside: avoid; }
.callouts li > b:first-child { display: inline-block; min-width: 5mm; height: 5mm; border-radius: 50%; background: #06180c; color: #7dffa0;
              text-align: center; font-size: 8pt; line-height: 5mm; margin-right: 1.5mm; }
.end { text-align: center; color: #4a4232; margin-top: 10mm; font-size: 9pt; letter-spacing: 1px; }
"""


def cover(title, subtitle_lines, picture=True):
    crt = "".join(f"<div class='{'dim' if i % 2 else ''}'>&gt; {l}</div>" for i, l in enumerate(subtitle_lines))
    return f"""<div class="cover">
<div class="brand">{LOGO.format(s=34, c='#4a4232')} TRANSISTOR LABS</div>
<div><h1>TSS-1</h1><h2>TRANSISTOR SPEECH SYSTEM</h2>
<div class="crt">{crt}</div></div>
{f'<img src="{img("panel.jpg")}">' if picture else ''}
<div class="foot"><span>{title}</span><span>VERSION {VERSION}</span><span>TICCOROSS.GUMROAD.COM</span></div>
</div>"""


def manual():
    s, n = [], [0]

    def ch(title, body):
        n[0] += 1
        s.append(f"<section><h3><span class='n'>{n[0]:02d}</span>{title}</h3>{body}</section>")

    ch("INTRODUCTION", """
<p>TSS-1 is a robotic text-to-speech instrument for your DAW. Type up to twenty words or phrases, then play them from MIDI
like samples, or play one phrase melodically across the keyboard so the machine sings it in tune. The voice is generated
by <b>eSpeak-NG formant synthesis</b>: no recordings and no AI models, just the buzzing, synthetic speech of classic
talking computers, speech chips and film robots.</p>
<ul><li>20 phrase slots, each played by its own MIDI key (C1 to G2 by default).</li>
<li><b>Trigger</b> mode plays phrases like one-shot samples; <b>Melodic</b> mode tunes the selected phrase to the notes you play.</li>
<li><b>Sync</b> speaks a phrase one syllable per beat, locked to your DAW's tempo (1/4 to 1/16 triplets).</li>
<li>26 voice presets, from Dr. Sbaitso and DECtalk to HAL 9000 and GLaDOS-style voices.</li>
<li>Speed, pitch, intonation range, word gap and volume, plus a robot effects chain: filter, harmonic boost and bit crusher.</li>
<li>Phrases are rendered in the background while you type, so notes play instantly with no delay.</li>
<li>Every control can be automated and MIDI-mapped; phrases and settings are saved with your project.</li></ul>""")

    ch("SYSTEM REQUIREMENTS", """
<table><tr><th>Platform</th><th>Formats</th><th>Requirements</th></tr>
<tr><td><b>macOS</b></td><td>Audio Unit, VST3, standalone app</td><td>macOS 10.13 High Sierra or later (including macOS 12 Monterey and newer);
Apple Silicon and Intel (Universal); a 64-bit host such as Ableton Live, Logic Pro, Bitwig or Reaper.</td></tr>
<tr><td><b>Windows</b></td><td>VST3, standalone app</td><td>Windows 10 or 11, 64-bit; a 64-bit VST3 host.</td></tr>
<tr><td><b>Linux</b></td><td>VST3, LV2, standalone app</td><td>x86-64, a recent distribution (glibc 2.35 or later); a 64-bit host.</td></tr></table>
<ul><li>About 20 MB of disk space. Around 2 MB of voice data is written the first time the plug-in runs
(macOS: <span class="k">~/Library/Application Support/Transistor Labs/TSS-1</span>; Windows: <span class="k">C:\\ProgramData\\Transistor Labs\\TSS-1</span>).</li>
<li>A MIDI keyboard or MIDI clips to play it. Any sample rate; the screen works at 60 % to 200 % size.</li></ul>""")

    ch("INSTALLATION", """
<h4>macOS</h4>
<p>Open <span class="k">TSS-1-""" + VERSION + """-macOS.pkg</span> and follow the steps. It installs the Audio Unit, the VST3 and the
standalone app; click <b>Customize</b> to leave any of them out. Then rescan plug-ins in your DAW (Ableton Live: Settings &rarr;
Plug-Ins, hold Option and click Rescan).</p>
<div class="note"><b>macOS security warning.</b> The installer is not notarised by Apple, so the first time you open it macOS says it
"cannot be opened because Apple cannot check it for malicious software" (or that it is from an unidentified developer).
To open it: <b>right-click (or Control-click) the .pkg and choose Open</b>, then click Open in the dialog. On macOS 13 and later
you may instead need to try opening it once, then go to <b>System Settings &rarr; Privacy &amp; Security</b>, scroll down and click
<b>Open Anyway</b> next to the TSS-1 message, and confirm with your password.</div>
<h4>Windows</h4>
<p>Run <span class="k">TSS-1-""" + VERSION + """-Windows-Setup.exe</span>. The VST3 goes to <span class="k">C:\\Program Files\\Common Files\\VST3</span>.
If Windows SmartScreen appears, click <b>More info</b> and then <b>Run anyway</b>. Rescan plug-ins in your DAW.</p>
<h4>Linux</h4>
<p>Unpack the <span class="k">.tar.gz</span> and run <span class="k">./install.sh</span> (installs to <span class="k">~/.vst3</span>,
<span class="k">~/.lv2</span> and <span class="k">~/.local/bin</span>, no root needed).</p>
<h4>Plug-ins only</h4>
<p>The zip files contain just the plug-in bundles (AU, VST3) for copying by hand into your plug-in folders.</p>""")

    ch("THE PANEL", f"""
<img class="shot" src="{img('panel.jpg')}">
<ol class="callouts">
<li><b>1</b><strong>Header</strong> click the <b>TR logo</b> for the About window; click the store address to visit the store.</li>
<li><b>2</b><strong>Screen</strong> voice preset, mode, play options, the phrase of the selected slot, the waveform and the readouts.</li>
<li><b>3</b><strong>Voice knobs</strong> SPEED, PITCH and RANGE.</li>
<li><b>4</b><strong>Effects knobs</strong> AMOUNT, HARMONIC and CRUSH.</li>
<li><b>5</b><strong>Switches</strong> FX (the whole effects chain) and FILTER.</li>
<li><b>6</b><strong>Phrase slots</strong> 20 keys, one per phrase.</li></ol>
<p>Knobs: drag up and down (or turn the mouse wheel); double-click to return to the default. Drag a corner of the window
to resize it. Only typing on the screen uses the computer keyboard; the rest of the time it stays with your DAW.</p>""")

    ch("PHRASE SLOTS", f"""
<img class="shot" src="{img('keys.jpg')}">
<ul><li><b>Click a key</b> to select that slot: its phrase appears on the screen, ready to edit, and the key gets a green frame.</li>
<li><b>Shift-click a key</b> to hear it (preview).</li>
<li>The <b>LED</b> on each key is green when the phrase is rendered and ready to play, blinks while it is being rendered,
and is dark when the slot is empty. The key lights up and goes down while its phrase plays.</li>
<li>In Trigger mode slot 1 is played by the <b>root key</b> (C1, MIDI note 36, by default) and the others follow chromatically,
so the 20 slots cover C1 to G2. Change the root on the screen (<span class="k">KEYS C1-G2</span>: drag up or down).</li></ul>""")

    ch("THE SCREEN", f"""
<div class="row"><img class="shot half" src="{img('screen.jpg')}"><div class="half">
<p>Everything shown in brackets or bright green can be clicked or dragged.</p>
<ul><li><span class="k">VOICE</span> click the name for the list of 26 presets; <span class="k">[&lt;]</span> <span class="k">[&gt;]</span> step through them.
Choosing a preset sets SPEED, PITCH, GAP and VOL to its values.</li>
<li><span class="k">MODE</span> click to switch between TRIGGER and MELODIC. On the right: the keys used (drag to move the root).</li>
<li><span class="k">PLAY</span> MONO (a new note cuts the one playing) or POLY (phrases overlap, up to 8);
ONE-SHOT (phrases always play to the end) or GATE (they stop when you release the key);
SYNC: click for the beat-synced syllable mode (chapter Beat Sync). LEVEL: drag for the output level.</li>
<li><span class="k">SLOT</span> the selected slot, its key and whether it is READY.</li></ul></div></div>
<h4>Typing a phrase</h4>
<p>Click in the dotted box and type (up to 240 characters); the status line shows TYPING. The phrase is rendered a moment
after you stop typing. <b>Return</b> finishes and plays it, <b>Esc</b> cancels the edit; either one (or clicking anywhere else
on the panel) gives the computer keyboard back to your DAW, so Ableton Live's computer MIDI keyboard plays TSS-1 again.
TSS-1 only takes the keyboard while you are typing a phrase. Punctuation shapes the speech: commas and full stops add pauses,
a question mark raises the intonation.</p>
<h4>Waveform and readouts</h4>
<p>While a phrase plays, the word being spoken appears above the waveform and the played part lights up. The bottom line shows
<span class="k">WPM</span> <span class="k">PITCH</span> <span class="k">RANGE</span> <span class="k">GAP</span> <span class="k">VOL</span>:
drag any of them up or down, double-click to reset. The value of the knob you turn is highlighted here.</p>""")

    ch("VOICE CONTROLS", """
<table><tr><th>Control</th><th>Range</th><th>What it does</th></tr>
<tr><td><b>SPEED</b> (knob, WPM)</td><td>80 - 300</td><td>Speaking rate in words per minute.</td></tr>
<tr><td><b>PITCH</b> (knob)</td><td>0 - 99</td><td>Base pitch of the voice.</td></tr>
<tr><td><b>RANGE</b> (knob)</td><td>0 - 99</td><td>Intonation: how much the pitch moves within the phrase. 0 = flat monotone robot, 50 = normal, 99 = very expressive.</td></tr>
<tr><td><b>GAP</b> (screen)</td><td>0 - 30</td><td>Extra pause between words, in 10 ms steps.</td></tr>
<tr><td><b>VOL</b> (screen)</td><td>10 - 200</td><td>Loudness of the voice itself (above 100 it drives the effects harder).</td></tr></table>
<p>Changing a voice control re-renders all the phrases in the background; their LEDs blink until they are ready, which takes
a fraction of a second.</p>""")

    ch("TRIGGER AND MELODIC MODES", f"""
<div class="row"><div class="half">
<h4>Trigger</h4>
<p>Each key plays its own phrase at the voice's natural pitch, like a sampler with 20 pads. Velocity sets the volume.
Use MONO for a single talking machine that cuts itself off, POLY for overlapping voices, GATE for chopped, rhythmic speech.</p>
<h4>Melodic</h4>
<p>The keyboard plays the <b>selected slot</b> at the pitch of the note you hold, anywhere on the keyboard. TSS-1 renders the phrase
again at the right voice pitch for each note (so it does not sound sped-up or chipmunky) and fine-tunes it to the exact note.
All notes from C0 to C6 are prepared in the background; a note that is not ready yet plays at once from the nearest one.</p>
<div class="tip"><b>Tip:</b> turn RANGE to 0 in Melodic mode: the voice then holds a steady pitch, and sings exactly on the note.</div>
</div><img class="shot half" src="{img('melodic.jpg')}"></div>""")

    ch("BEAT SYNC", """
<p>Sync makes TSS-1 speak a phrase <b>one syllable per step</b>, locked to your DAW's tempo and grid: the robotic,
chopped vocal of electro records. Click <span class="k">SYNC</span> on the screen and choose a step length:
<b>1/4, 1/8, 1/16, 1/8 triplet or 1/16 triplet</b> (or OFF).</p>
<ul><li>When a note starts, the first syllable is spoken at once; each following syllable starts exactly on the next step of the
DAW's grid. "Transistor speech system online" becomes TRAN - SIS - TOR - SPEECH - SYS - TEM - ON - LINE.</li>
<li>A syllable longer than the step is cut at the next step; a shorter one leaves a gap, so the rhythm always stays on the grid.</li>
<li><b>ONE-SHOT</b>: the phrase is spoken once through. <b>GATE</b>: the syllables loop for as long as you hold the key and stop
when you release it, so the length of your MIDI notes decides how much of the phrase you hear.</li>
<li>Works in Trigger and Melodic mode (in Melodic mode every syllable is sung at the note you play).</li>
<li>It follows tempo changes and loops in the arrangement. When the transport is stopped it runs at the DAW's tempo
(120 BPM if the host gives none).</li></ul>
<div class="tip"><b>Tip:</b> SYNC 1/16, GATE on, one long note: an endless machine chant. Words split the way eSpeak
pronounces them, so write phrases as you want them heard.</div>""")

    ch("EFFECTS", """
<p>The robot effects chain from DJZ-Speak, plus a bit crusher. It runs live, so the knobs can be moved and automated while a phrase plays.</p>
<table><tr><th>Control</th><th>What it does</th></tr>
<tr><td><b>FX</b> switch</td><td>Turns the whole chain on or off (the LED lights when on).</td></tr>
<tr><td><b>AMOUNT</b> (0.5 - 2.0)</td><td>Overall intensity: how strongly the filter, the harmonic boost and the mechanical quantisation act.</td></tr>
<tr><td><b>FILTER</b> switch</td><td>A vintage computer filter: lifts the upper mids, then smooths the very top, like a small speaker.</td></tr>
<tr><td><b>HARMONIC</b> (1.0 - 2.0)</td><td>Saturation that adds metallic harmonics. 1.0 is clean.</td></tr>
<tr><td><b>CRUSH</b> (0 - 1)</td><td>Bit and sample-rate reduction, from 16 bits down to 4 bits and down to about 2.7 kHz: speech-chip grit at the top.</td></tr>
<tr><td><b>LEVEL</b> (screen)</td><td>Output level, -24 to +6 dB, with a soft limiter.</td></tr></table>""")

    rows = presets()
    body = "<table><tr><th>Preset</th><th>eSpeak voice</th><th class='c'>Speed</th><th class='c'>Pitch</th><th class='c'>Volume</th><th class='c'>Gap</th></tr>"
    last = None
    for g, _id, name, lang, var, sp, pi, am, gp in rows:
        if g != last:
            body += f"<tr class='g'><td colspan='6'>{g}</td></tr>"
            last = g
        body += f"<tr><td><b>{name}</b></td><td>{lang} + {var}</td><td class='c'>{sp}</td><td class='c'>{pi}</td><td class='c'>{am}</td><td class='c'>{gp}</td></tr>"
    body += "</table><p>The voice settings of the presets come from DJZ-Speak. Variants m1 - m4 are male voices, f1 - f4 female; BRITISH ANDROID uses Received Pronunciation English.</p>"
    ch("VOICE PRESETS", body)

    ch("AUTOMATION AND MIDI", """
<p>All of these can be automated in your DAW and MIDI-mapped (Ableton Live: Configure the plug-in, or map its parameters):
Voice, Speed, Pitch, Range, Word Gap, Voice Volume, Mode, Root Note, Voices (mono/poly), Gate, Sync, FX, FX Amount, Filter,
Harmonic Boost, Crush and Level.</p>
<ul><li>Automating Voice changes the eSpeak voice only; choosing a preset on the screen also sets Speed, Pitch, Gap and Volume.</li>
<li>The phrases, the selected slot and every setting are saved with your project and in DAW presets.</li>
<li>MIDI: notes and velocity play the slots (Trigger) or the pitch (Melodic); All Notes Off stops every voice.</li></ul>""")

    ch("TIPS", """
<ul><li><b>Spell it how it sounds.</b> eSpeak reads English text by rules; if a word comes out wrong, write it phonetically
("Aquabahn" &rarr; "Ah-kwa-bahn"), or split it with spaces or hyphens.</li>
<li><b>Numbers and letters</b> are spoken: "1 2 3 4", "T S S 1". Short words in separate slots give you a rhythmic speech kit.</li>
<li><b>Electro vocals:</b> TERMINATOR or HEAVY METAL voice, RANGE 0, GATE on, CRUSH around 0.4, then play the phrase melodically.</li>
<li><b>Old computer:</b> DR. SBAITSO or APPLE II with FILTER on and CRUSH at 0.6 - 0.8.</li>
<li><b>Slower and deeper:</b> lower SPEED and PITCH; raise GAP for a deliberate, word-by-word computer voice.</li></ul>""")

    ch("ABOUT, CREDITS AND LICENCE", f"""
<div class="row"><img class="shot half" src="{img('about.jpg')}"><div class="half">
<p>Click the TR logo for the About window: version, contact, the store, this manual and the source code.</p>
<p><b>TSS-1</b> &copy; 2026 Transistor Labs. Free software under the <b>GNU General Public License v3.0</b>: you may use it
for any music, commercial or not; its source code is published at github.com/Spunkmeyer77/TSS-1.</p>
<p>Speech synthesis: <b>eSpeak-NG</b> 1.52 (GPL-3.0), by Reece H. Dunn and contributors, based on eSpeak by Jonathan Duddington.
Voice presets and effects after <b>DJZ-Speak</b> by MushroomFleet (Apache-2.0). Built with JUCE.</p>
<p>The preset names describe styles of voice; they are not affiliated with or endorsed by the owners of those names.</p>
<p><b>Contact:</b> ticcoross@gmail.com<br><b>Store:</b> ticcoross.gumroad.com</p></div></div>""")

    return cover("OWNER'S MANUAL", ["TRANSISTOR SPEECH SYSTEM ONLINE", "VOICE: CLASSIC ROBOT", "MODE: TRIGGER  KEYS C1-G2",
                                     "TYPE A PHRASE. PLAY A KEY."]) + "".join(s) + \
        f"<div class='end'>{LOGO.format(s=26, c='#4a4232')}<br>TRANSISTOR LABS &middot; TSS-1 &middot; VERSION {VERSION}</div>"


HISTORY = [
    ("1.1.1", [
        "Fixed: TSS-1 took the computer keyboard whenever its window was active, so Ableton Live's computer MIDI keyboard typed into the phrase box. The phrase box now only takes the keyboard after you click it; Return, Esc or a click anywhere else on the panel gives it back.",
    ]),
    ("1.1.0", [
        "New BEAT SYNC mode: speaks a phrase one syllable per step, locked to the DAW's tempo and grid (1/4, 1/8, 1/16, 1/8 triplet, 1/16 triplet). Choose it with SYNC on the screen; GATE makes the syllables loop while the key is held.",
        "The computer keyboard now stays with your DAW: Ableton Live's computer MIDI keyboard plays TSS-1. The plug-in only takes the keyboard while you type a phrase; Return, Esc or a click elsewhere gives it back.",
        "The window is cropped to the case: no more empty space around the hardware.",
        "The status line shows TYPING while you edit a phrase.",
    ]),
    ("1.0.0", [
        "First release of TSS-1, the Transistor Speech System: a robotic text-to-speech instrument (AU, VST3, LV2 and standalone app for macOS, Windows and Linux).",
        "eSpeak-NG 1.52 formant speech synthesis built in, with its English voice data; nothing else to install.",
        "20 phrase slots played from MIDI (C1 to G2, root note adjustable), rendered in the background so notes play instantly.",
        "Trigger mode (phrases as one-shots) and Melodic mode (the selected phrase sung in tune on every key, prepared from C0 to C6).",
        "26 voice presets after DJZ-Speak: retro computing, cinematic AI, modern and technical, and character voices.",
        "SPEED, PITCH and RANGE knobs; word gap, voice volume, mono/poly, one-shot/gate and output level on the screen.",
        "Robot effects chain: FX and FILTER switches, AMOUNT, HARMONIC boost and CRUSH (bit and sample-rate reduction).",
        "Retro computer panel with a green CRT screen: type phrases on the screen, live waveform and the word being spoken.",
        "Resizable window (60 % to 200 %), custom About window with contact, store, manual and source code.",
        "Every control can be automated and MIDI-mapped; phrases and settings are saved with your project.",
    ]),
]


def history():
    body = ""
    for ver, items in HISTORY:
        body += f"<section><h3><span class='n'>&gt;</span>VERSION {ver}</h3><ul>" + "".join(f"<li>{i}</li>" for i in items) + "</ul></section>"
    return cover("UPDATE HISTORY", ["UPDATE HISTORY", f"CURRENT VERSION: {VERSION}"], picture=False) + body


def to_pdf(html, out):
    chrome = next((c for c in ("google-chrome", "chromium", "chromium-browser") if shutil.which(c)), None)
    if chrome is None:
        sys.exit("needs Chrome or Chromium for printing")
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "doc.html"
        src.write_text(f"<!doctype html><html><head><meta charset='utf-8'><style>{CSS}</style></head><body>{html}</body></html>")
        subprocess.run([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                        f"--print-to-pdf={out}", str(src)], check=True, capture_output=True)
    print("wrote", out)


if __name__ == "__main__":
    to_pdf(manual(), DOCS / "TSS-1 Manual.pdf")
    to_pdf(history(), DOCS / "TSS-1 Update History.pdf")
