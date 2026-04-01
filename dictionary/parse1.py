import xml.etree.ElementTree as ET

def extract_words(xml_path, out_path):
    words = set()

    tree = ET.parse(xml_path)
    root = tree.getroot()

    for child in root:
        #grammemes
        #restrictions
        #lemmata
        #link_types
        #links
        #  print(child.tag, child.attrib)
        if (child.tag == "lemmata"):
            for lemma in child:
                for words in lemma:
                    print(lemma.attrib["id"])
        if (child.tag == "grammemes"):
            for grammeme in child:
                print(grammeme.tag, grammeme.attrib)

    print(root.tag)

    #
    #
    #  for event, elem in ET.iterparse(xml_path, events=("end",)):
    #      if elem.tag in ("l", "f"):
    #          word = elem.get("t")
    #          if word:
    #              # собираем граммемы из вложенных <g v="...">
    #              grammemes = [g.get("v") for g in elem.findall("g")]
    #
    #              # пример фильтрации: оставляем только существительные в именительном падеже
    #              if "NOUN" in grammemes and "nomn" in grammemes:
    #                  words.add(word.lower())
    #
    #      elem.clear()  # освобождаем память
    #
    #  with open(out_path, "w", encoding="utf-8") as f:
    #      for w in sorted(words):
    #          f.write(w + "\n")
    #
    #  print(f"Готово! Сохранено {len(words)} слов.")


# пример запуска
extract_words("opencorpora/dict.opcorpora.xml", "scrabble_words.txt")
