#!/usr/bin/env python3
import os
import xml.etree.ElementTree as ET  # nosec


def get_translation_status(file_path: str) -> tuple[int, int, int, int] | None:
    try:
        tree = ET.parse(file_path)  # nosec
        root = tree.getroot()

        total = 0
        unfinished = 0
        empty = 0
        automated = 0

        for message in root.findall(".//message"):
            total += 1
            translation = message.find("translation")
            is_unfinished = False

            if translation is not None:
                if translation.get("type") == "unfinished":
                    is_unfinished = True
                    unfinished += 1
                if not translation.text or not translation.text.strip():
                    empty += 1
            else:
                empty += 1

            # Check for "Automated translation" in translatorcomment
            if not is_unfinished:
                comment = message.find("translatorcomment")
                if (comment is not None and comment.text
                        and "Automated translation" in comment.text):
                    automated += 1

        return total, unfinished, empty, automated
    except Exception as e:
        print(f"Error parsing {file_path}: {e}")
        return None


def main() -> None:
    translations_dir = "translations"
    ts_files = [f for f in os.listdir(translations_dir) if f.endswith(".ts")]
    ts_files.sort()

    print(
        f"{'Language':<10} | {'Total':<6} | {'Unfinished':<10} | {'Empty':<6} | {'Automated':<9}"
    )
    print("-" * 58)

    for ts_file in ts_files:
        lang = os.path.splitext(ts_file)[0]
        if lang == "en":
            continue
        file_path = os.path.join(translations_dir, ts_file)
        status = get_translation_status(file_path)

        if status:
            total, unfinished, empty, automated = status
            print(
                f"{lang:<10} | {total:<6} | {unfinished:<10} | {empty:<6} | {automated:<9}"
            )


if __name__ == "__main__":
    main()
