// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Santiago Quiroz Upegui and the gow2-recomp contributors
// UTF-8 to UTF-16 conversion and the per-locale PC Options entry text.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace rex::glue {

inline std::u16string Utf8ToUtf16(std::string_view s) {
  std::u16string r;
  r.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      r.push_back(char16_t(c));
      ++i;
      continue;
    }
    char32_t cp = 0;
    size_t len = 0;
    if ((c & 0xE0) == 0xC0) {
      cp = char32_t(c & 0x1F);
      len = 2;
    } else if ((c & 0xF0) == 0xE0) {
      cp = char32_t(c & 0x0F);
      len = 3;
    } else if ((c & 0xF8) == 0xF0) {
      cp = char32_t(c & 0x07);
      len = 4;
    } else {
      r.push_back(u'\uFFFD');  // stray continuation byte or 0xF8 and up
      ++i;
      continue;
    }
    if (i + len > s.size()) {
      r.push_back(u'\uFFFD');  // truncated sequence
      break;
    }
    bool ok = true;
    for (size_t j = 1; j < len; ++j) {
      const unsigned char d = static_cast<unsigned char>(s[i + j]);
      if ((d & 0xC0) != 0x80) {
        ok = false;
        break;
      }
      cp = (cp << 6) | char32_t(d & 0x3F);
    }
    if (!ok) {
      r.push_back(u'\uFFFD');
      ++i;
      continue;
    }
    constexpr char32_t kMin[] = {0, 0, 0x80, 0x800, 0x10000};
    if (cp < kMin[len] || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
      r.push_back(u'\uFFFD');  // overlong, surrogate or out of range
      i += len;
      continue;
    }
    if (cp > 0xFFFF) {
      cp -= 0x10000;
      r.push_back(char16_t(0xD800 + (cp >> 10)));
      r.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
    } else {
      r.push_back(char16_t(cp));
    }
    i += len;
  }
  return r;
}

struct LocalizedPair {
  std::string_view label;
  std::string_view description;
};

inline bool LocaleIs(std::string_view locale, const char* want) {
  if (locale.size() != 3) return false;
  for (size_t i = 0; i < 3; ++i) {
    char c = locale[i];
    if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    if (c != want[i]) return false;
  }
  return true;
}

inline LocalizedPair DeviceOptionsEntryText(std::string_view locale) {
  if (LocaleIs(locale, "esn") || LocaleIs(locale, "esm"))
    return {"Opciones del dispositivo", "Pantalla, gráficos, controles, red y partidas en este dispositivo."};
  if (LocaleIs(locale, "deu"))
    return {"Geräteoptionen", "Anzeige, Grafik, Steuerung, Netzwerk und Spielstände auf diesem Gerät."};
  if (LocaleIs(locale, "fra"))
    return {"Options appareil", "Affichage, graphismes, commandes, réseau et sauvegardes sur cet appareil."};
  if (LocaleIs(locale, "ita"))
    return {"Opzioni dispositivo", "Schermo, grafica, controlli, rete e salvataggi su questo dispositivo."};
  if (LocaleIs(locale, "pol"))
    return {"Opcje urządzenia", "Ekran, grafika, sterowanie, sieć i zapisy na tym urządzeniu."};
  if (LocaleIs(locale, "cze"))
    return {"Možnosti zařízení", "Obraz, grafika, ovládání, síť a uložené pozice na tomto zařízení."};
  if (LocaleIs(locale, "hun"))
    return {"Eszközbeállítások", "Kép, grafika, irányítás, hálózat és mentések ezen az eszközön."};
  if (LocaleIs(locale, "rus"))
    return {"Настройки устройства", "Экран, графика, управление, сеть и сохранения на этом устройстве."};
  if (LocaleIs(locale, "kor")) return {"기기 옵션", "이 기기의 화면, 그래픽, 조작, 네트워크 및 저장 설정."};
  if (LocaleIs(locale, "cht")) return {"裝置選項", "此裝置的顯示、圖形、控制、網路與存檔設定。"};
  return {"Device Options", "Graphics, controls, network and saves."};
}

inline LocalizedPair PcOptionsEntryText(std::string_view locale, bool android) {
  if (android) return DeviceOptionsEntryText(locale);
  if (LocaleIs(locale, "esn") || LocaleIs(locale, "esm"))
    return {"Opciones de PC", "Pantalla, gráficos, controles, red y partidas en este PC."};
  if (LocaleIs(locale, "deu"))
    return {"PC-Optionen", "Anzeige, Grafik, Steuerung, Netzwerk und Spielstände auf diesem PC."};
  if (LocaleIs(locale, "fra"))
    return {"Options PC", "Affichage, graphismes, commandes, réseau et sauvegardes sur ce PC."};
  if (LocaleIs(locale, "ita"))
    return {"Opzioni PC", "Schermo, grafica, controlli, rete e salvataggi su questo PC."};
  if (LocaleIs(locale, "pol"))
    return {"Opcje PC", "Ekran, grafika, sterowanie, sieć i zapisy na tym komputerze."};
  if (LocaleIs(locale, "cze"))
    return {"Možnosti PC", "Obraz, grafika, ovládání, síť a uložené pozice na tomto PC."};
  if (LocaleIs(locale, "hun"))
    return {"PC-beállítások", "Kép, grafika, irányítás, hálózat és mentések ezen a PC-n."};
  if (LocaleIs(locale, "rus"))
    return {"Настройки ПК", "Экран, графика, управление, сеть и сохранения на этом ПК."};
  if (LocaleIs(locale, "kor")) return {"PC 옵션", "이 PC의 화면, 그래픽, 조작, 네트워크 및 저장 설정."};
  if (LocaleIs(locale, "cht")) return {"PC 選項", "此電腦的顯示、圖形、控制、網路與存檔設定。"};
  return {"PC Options", "Graphics, controls, network and saves."};
}

inline std::string_view PcSavingMessage(std::string_view locale, bool android) {
  if (android) {
    if (LocaleIs(locale, "esn") || LocaleIs(locale, "esm"))
      return "Guardando contenido. No apagues tu dispositivo.";
    if (LocaleIs(locale, "deu"))
      return "Inhalt wird gespeichert. Bitte schalten Sie Ihr Gerät nicht aus.";
    if (LocaleIs(locale, "fra"))
      return "Enregistrement en cours. N'éteignez pas votre appareil.";
    if (LocaleIs(locale, "ita"))
      return "Salvataggio del contenuto. Non spegnere il dispositivo.";
    if (LocaleIs(locale, "pol"))
      return "Trwa zapisywanie. Nie wyłączaj urządzenia.";
    if (LocaleIs(locale, "cze"))
      return "Probíhá ukládání obsahu. Nyní prosím zařízení nevypínejte.";
    if (LocaleIs(locale, "hun"))
      return "A tartalom mentése folyamatban van. Ne kapcsolja ki az eszközt.";
    if (LocaleIs(locale, "rus"))
      return "Идет сохранение. Не выключайте устройство.";
    if (LocaleIs(locale, "kor")) return "콘텐츠 저장 중 - 기기를 끄지 마십시오.";
    if (LocaleIs(locale, "cht")) return "正在儲存內容，請勿關閉裝置電源。";
    return "Saving content. Please don't turn off your device.";
  }
  if (LocaleIs(locale, "esn") || LocaleIs(locale, "esm"))
    return "Guardando contenido. No apagues tu PC.";
  if (LocaleIs(locale, "deu"))
    return "Inhalt wird gespeichert. Bitte schalten Sie Ihren PC nicht aus.";
  if (LocaleIs(locale, "fra"))
    return "Enregistrement en cours. N'éteignez pas votre PC.";
  if (LocaleIs(locale, "ita"))
    return "Salvataggio del contenuto. Non spegnere il PC.";
  if (LocaleIs(locale, "pol"))
    return "Trwa zapisywanie. Nie wyłączaj komputera.";
  if (LocaleIs(locale, "cze"))
    return "Probíhá ukládání obsahu. Nyní prosím PC nevypínejte.";
  if (LocaleIs(locale, "hun"))
    return "A tartalom mentése folyamatban van. Ne kapcsolja ki a PC-t.";
  if (LocaleIs(locale, "rus"))
    return "Идет сохранение. Не выключайте ПК.";
  if (LocaleIs(locale, "kor")) return "콘텐츠 저장 중 - PC를 끄지 마십시오.";
  if (LocaleIs(locale, "cht")) return "正在儲存內容，請勿關閉電腦電源。";
  return "Saving content. Please don't turn off your PC.";
}

}  // namespace rex::glue
