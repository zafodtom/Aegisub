import pathlib
import subprocess
import sys
import tempfile


ASS_TEMPLATE = r"""[Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Original\Nsecond line
"""

SSA_TEMPLATE = r"""[Script Info]
ScriptType: v4.00
PlayResX: 1920
PlayResY: 1080

[V4 Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding
Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,1,2,0,2,20,20,20,0,1

[Events]
Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: Marked=0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Original\Nsecond line
"""

SRT_TEMPLATE = """1
00:00:01,000 --> 00:00:03,000
Original first line
Original second line

"""


def run(bridge: pathlib.Path, *args: pathlib.Path) -> None:
    subprocess.run([bridge, *args], check=True)


def verify_format(bridge: pathlib.Path, directory: pathlib.Path, extension: str, template: str) -> None:
    source = directory / f"source{extension}"
    update = directory / "update.tsv"
    saved = directory / f"saved{extension}"
    readback = directory / f"readback-{extension[1:]}.tsv"

    source.write_text(template, encoding="utf-8", newline="\n")
    update.write_text(
        "AEGISUB-WINUI-BRIDGE\t1\n"
        "00:00:01.000\t00:00:03.000\tPrvní\\r\\ndruhý\n",
        encoding="utf-8",
        newline="\n",
    )

    run(bridge, "--write", source, update, saved)
    run(bridge, saved, readback)

    saved_text = saved.read_text(encoding="utf-8")
    if extension == ".srt":
        assert "První\ndruhý" in saved_text
    else:
        assert "První\\Ndruhý" in saved_text

    rows = readback.read_text(encoding="utf-8").splitlines()
    assert rows[0] == "AEGISUB-WINUI-BRIDGE\t2"
    assert rows[1] == "00:00:01.000\t00:00:03.000\tPrvní\\ndruhý\tPrvní\\\\Ndruhý"


def main() -> None:
    bridge = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="aegisub-winui-bridge-") as temp:
        directory = pathlib.Path(temp)
        verify_format(bridge, directory, ".ass", ASS_TEMPLATE)
        verify_format(bridge, directory, ".ssa", SSA_TEMPLATE)
        verify_format(bridge, directory, ".srt", SRT_TEMPLATE)


if __name__ == "__main__":
    main()
